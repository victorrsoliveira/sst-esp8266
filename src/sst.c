/*****************************************************************************
* SST platform-independent implementation
*
* Copyright (C) 2006 Miro Samek and Robert Ward. All rights reserved.
*
* This software may be distributed and modified under the terms of the GNU
* General Public License version 2 (GPL) as published by the Free Software
* Foundation and appearing in the file GPL.TXT included in the packaging of
* this file. Please note that GPL Section 2[b] requires that all works based
* on this software must also be made publicly available under the terms of
* the GPL ("Copyleft").
*
* Contact information:
* Email:    miro@quantum-leaps.com
* Internet: www.quantum-leaps.com
*****************************************************************************/

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  NOTE: Modified version of SST by Victor Oliveira
  There were functions added to this file. The functions of the original where
  slightly modified, but their operations are the same. The added functions
  are intended to increment the features of SST, including data structures such
  as semaphores, mailboxes e queues.
  These modifications were done for a project in the course of Real-Time Systems
  in the Federal University of Alagoas, Brazil.

  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/

#include "sst_port.h"
#include "sst_exa.h"
#include "c_types.h"
#include "osapi.h"
#include "mem.h"

/* Public-scope objects ----------------------------------------------------*/
uint8_t SST_currPrio_ = (uint8_t)0xFF;              /* current SST priority */
#if SST_MAX_PRIO == 64
uintX_t SST_readySet_ = (uintX_t)UINT64_C(0x0000000000000000);                        /* SST ready-set */
#else
uintX_t SST_readySet_ = (uintX_t)0;
#endif

typedef struct TaskCBTag TaskCB;
struct TaskCBTag {
  SSTTask task__;                 // Pointer to the task
  SSTEvent lastEvent__;           // Used when task is "suspended"
  SSTEvent *queue__;              // Event queue
  uint8_t end__;                  // The length of the queue
  uint8_t head__;                 // Points to the head (start or ) of the queue
  uint8_t tail__;                 // and the tail
  uint8_t nUsed__;
  uintX_t mask__;
};

/* Local-scope objects -----------------------------------------------------*/
static TaskCB l_taskCB[SST_MAX_PRIO];

/*..........................................................................*/
void SST_task(SSTTask task, uint8_t prio, SSTEvent *queue, uint8_t qlen,
  SSTSignal sig, SSTParam par)
  {
    SSTEvent ie;                                    /* initialization event */
    TaskCB *tcb  = &l_taskCB[prio - 1];
    tcb->task__  = task;
    tcb->queue__ = queue;
    tcb->end__   = qlen;
    tcb->head__  = (uint8_t)0;
    tcb->tail__  = (uint8_t)0;
    tcb->nUsed__ = (uint8_t)0;
    #if SST_MAX_PRIO == 64
    tcb->mask__  = (UINT64_C(0x0000000000000001) << (prio - 1));
    #else
    tcb->mask__  = (1 << (prio - 1));
    #endif
    ie.sig = sig;
    ie.par = par;
    tcb->lastEvent__ = ie;
    tcb->task__(ie);                                 /* initialize the task */
  }
  /*..........................................................................*/
  void SST_run(void) {
    SST_start();                                              /* start ISRs */

    SST_INT_LOCK();
    SST_currPrio_ = (uint8_t)0;   /* set the priority for the SST idle loop */
    SST_schedule_();                  /* process all events produced so far */
    SST_INT_UNLOCK();

    //for (;;) {                                         /* the SST idle loop */
    //    SST_onIdle();                        /* invoke the on-idle callback */
    //}
  }
  /*..........................................................................*/
  uint8_t SST_post(uint8_t prio, SSTSignal sig, SSTParam par) {
    TaskCB *tcb = &l_taskCB[prio - 1];
    SST_INT_LOCK();
    if (tcb->nUsed__ < tcb->end__) {
      tcb->queue__[tcb->head__].sig = sig;/* insert the event at the head */
      tcb->queue__[tcb->head__].par = par;
      if ((++tcb->head__) == tcb->end__) {
        tcb->head__ = (uint8_t)0;                      /* wrap the head */
      }
      if ((++tcb->nUsed__) == (uint8_t)1) {           /* the first event? */
        SST_readySet_ |= tcb->mask__;   /* insert task to the ready set */
        SST_schedule_();            /* check for synchronous preemption */
      }
      SST_INT_UNLOCK();
      return (uint8_t)1;                     /* event successfully posted */
    }
    else {
      SST_INT_UNLOCK();
      return (uint8_t)0;              /* queue full, event posting failed */
    }
  }
  /*..........................................................................*/
  uint8_t SST_mutexLock(uint8_t prioCeiling) {
    uint8_t p;
    SST_INT_LOCK();
    p = SST_currPrio_;               /* the original SST priority to return */
    if (prioCeiling > SST_currPrio_) {
      SST_currPrio_ = prioCeiling;              /* raise the SST priority */
    }
    SST_INT_UNLOCK();
    return p;
  }
  /*..........................................................................*/
  void SST_mutexUnlock(uint8_t orgPrio) {
    SST_INT_LOCK();
    if (orgPrio < SST_currPrio_) {
      SST_currPrio_ = orgPrio;    /* restore the saved priority to unlock */
      SST_schedule_();    /* invoke scheduler after lowering the priority */
    }
    SST_INT_UNLOCK();
  }

  uint8_t log2Lkup(uintX_t rs){
    uint8_t n = 0;
    #if SST_MAX_PRIO == 64
    uintX_t b = UINT64_C(0x0000000000000001) << (SST_MAX_PRIO - 1);
    #else
    uintX_t b = 1 << (SST_MAX_PRIO - 1);
    #endif
    while (n < SST_MAX_PRIO) {
      if (rs & (b >> n)) {
        return SST_MAX_PRIO-n;
      }
      n++;
    }
    return 0;
  }

  /*..........................................................................*/
  /* NOTE: SST_schedule_() the SST scheduler is entered and exited with interrupts LOCKED */
  void SST_schedule_(void) {
    // static uint8_t const log2Lkup[] = {
    //     0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4,
    //     5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    //     6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    //     6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    //     7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    //     7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    //     7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    //     7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    //     8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    //     8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    //     8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    //     8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    //     8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    //     8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    //     8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    //     8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8
    // };
    uint8_t pin = SST_currPrio_;               /* save the initial priority */
    uint8_t p;                                          /* the new priority */
    /* is the new priority higher than the initial? */
    while ((p = log2Lkup(SST_readySet_)) > pin) {
      TaskCB *tcb  = &l_taskCB[p - 1];
      /* get the event out of the queue */
      SSTEvent e = tcb->queue__[tcb->tail__];
      tcb->lastEvent__ = e; // save the last executed event
      if ((++tcb->tail__) == tcb->end__) {
        tcb->tail__ = (uint8_t)0;
      }
      if ((--tcb->nUsed__) == (uint8_t)0) {/* is the queue becoming empty?*/
        SST_readySet_ &= ~tcb->mask__;     /* remove from the ready set */
      }
      SST_currPrio_ = p;        /* this becomes the current task priority */
      SST_INT_UNLOCK();                          /* unlock the interrupts */

      (*tcb->task__)(e);                             /* call the SST task */

      SST_INT_LOCK();            /* lock the interrupts for the next pass */
    }
    SST_currPrio_ = pin;                    /* restore the initial priority */
  }

  void SST_initSemaphore(Semaphore *s) {
    SST_INT_LOCK();
    s->c = 1;
    #if SST_MAX_PRIO == 64
    s->queue = (uintX_t) UINT64_C(0x0000000000000000);
    #else
    s->queue = (uintX_t) 0;
    #endif
    SST_INT_UNLOCK();
  }

  uint8_t SST_wait(Semaphore *s) {
    SST_INT_LOCK();
    if (s->c > 0) {  // Is semaphore available?
      s->c = 0;
      return 1; // Semaphore was successfully taken by the running task
    }
    TaskCB *tcb  = &l_taskCB[SST_currPrio_ - 1];  // Get the current the TCB of current running task
    s->queue |= tcb->mask__;  // Add the task to the semaphore queue
    SST_INT_UNLOCK();
    return 0;  // Semaphore is unavailable so task will be blocked
  }

  /*
  *  NOTE: SST_signal(s) This function should be called only when a semaphore
  *  has been taken.
  *  If there is some task in the queue wanting to get access
  *  on the semaphore, it will be called with the semaphore
  *  available (s->c = 1), so it can take the semaphore during
  *  its execution
  */

  void SST_signal(Semaphore *s) {
    SST_INT_LOCK();
    // Test if the queue is empty
    if (s->queue == 0) {
      s->c = 1;
    }
    // Should call the highest priority task waiting on this semaphore
    // Call task waiting on semaphore
    else {
      uint8_t p = log2Lkup(s->queue);   // Get the highest priority "blocked" task
      os_printf("DEBUG: CALL TASK %d THAT WAS SUSPENDED.\n", p);
      s->queue &= ~(1 << (p - 1));  // Remove this task from the queue
      TaskCB *tcb  = &l_taskCB[p - 1];
      s->c = 1; // this is necessary as it was explained above
      SST_post(p, SIGNAL_SEM_SIG, (tcb->lastEvent__.par));
    }
    SST_INT_UNLOCK();
  }

  void SST_initMailbox(Mailbox *mb) {
    Semaphore sem;
    SST_initSemaphore(&sem);
    SST_INT_LOCK();
    mb->sem = sem;
    mb->data = NULL;
    SST_INT_UNLOCK();
  }

  /*  NOTE: addTaskSemQueue(s) - This is an auxiliary function that adds a task
  * to the semaphore's queue to indicate that the current task has been
  * suspended for some reason (mailbox is empty or full, the data structure
  * queue has no data available or no more space).
  */
  void addTaskSemQueue(Semaphore *sem) {
    TaskCB *tcb  = &l_taskCB[SST_currPrio_ - 1];  // Get the current the TCB of current running task
    sem->queue |= tcb->mask__; // Add the task to the semaphore queue
  }

  /*  NOTE: SST_send(mb, data) Data passed to the mailbox in the function below should be a
  *  point to a global variable in the application, so that the reference
  *  won't be lost during the execution.
  */
  uint8_t SST_send(Mailbox *mb, uint8_t *data) {
    uint8_t w = 0;
    if ((mb->data == NULL)) { // if mailbox is empty, tries to use it
      w = SST_wait(&(mb->sem));	// wait for this mailbox (lock and unlock INTs)
      SST_INT_LOCK();
      if (w) {	// if access is granted to the mailbox, then
        mb->data = data;	// write data to it
        SST_INT_UNLOCK();
        SST_signal(&(mb->sem));	// release lock on mailbox (lock and unlock INTs)
      }
    } else {        // mailbox is already full, so the task will be "blocked"
    SST_INT_LOCK(); // and will wait until the mailbox is empty again
    os_printf("DEBUG: MAILBOX IS FULL!\n");
    addTaskSemQueue(&(mb->sem));
    SST_INT_UNLOCK();
  }
  return w;
}

uint8_t SST_receive(Mailbox *mb, uint8_t *data) {
  uint8_t w = 0;
  if ((mb->data != NULL)) { // Is there any data on this mailbox?
    w = SST_wait(&(mb->sem));	// wait for this mailbox (lock and unlock INTs)
    SST_INT_LOCK();
    if (w) {	// if access is granted to the mailbox, then
      *data = *(mb->data);	// read data from it
      mb->data = NULL;	// clear data space
      SST_INT_UNLOCK();
      SST_signal(&(mb->sem));	// release lock on mailbox (lock and unlock INTs)
    }
  } else {  // there is no data available, so the task will be "suspended"
  SST_INT_LOCK();
  os_printf("DEBUG: NO DATA AVAILABLE!\n");
  addTaskSemQueue(&(mb->sem));
  SST_INT_UNLOCK();
}
return w;
}

void SST_initQueue(Queue *q, uint8_t size) {
  Semaphore sem;
  SST_initSemaphore(&sem);
  SST_INT_LOCK();
  q->sem = sem;
  q->data = (uint8_t*) os_malloc(sizeof(uint8_t)*size);
  q->head = 0;
  q->tail = 0;
  q->nelem = 0;
  q->size = size;
  SST_INT_UNLOCK();
}

uint8_t SST_enqueue(Queue *q, uint8_t data) {
  uint8_t w = 0;
  if (q->nelem < q->size) { // if there's room for one more item
  w = SST_wait(&(q->sem));	// wait for this queue (lock and unlock INTs)
  SST_INT_LOCK();
  if (w) {	// if access is granted to the queue, then
    q->data[(int) q->head] = data;	// write data to it
    q->head = (q->head + 1) % q->size;
    q->nelem++;
    SST_INT_UNLOCK();
    SST_signal(&(q->sem));	// release lock on queue (lock and unlock INTs)
  }
} else {  // if there's no more space in the queue, the task will be "suspended"
SST_INT_LOCK();
os_printf("DEBUG: NO MORE SPACE AVAILABLE!\n");
// REVIEW: SST_enqueue - Add function addTaskSemQueue
addTaskSemQueue(&(q->sem));
/* CHANGED: SST_enqueue - Adding to semaphore's queue
* TaskCB *tcb  = &l_taskCB[SST_currPrio_ - 1];  // Get the current the TCB of current running task
* Semaphore *sem = &(q->sem);
* sem->queue |= tcb->mask__;  // Add the task to the semaphore queue
*/
SST_INT_UNLOCK();
}
return w;
}

uint8_t SST_dequeue(Queue *q, uint8_t *data) {
  uint8_t w = 0;
  if (q->nelem > 0) {
    w = SST_wait(&(q->sem));	// wait for this queue (lock and unlock INTs)
    SST_INT_LOCK();
    if (w) {	// if access is to the queue, then
      *data = q->data[(int) q->tail];	// read data from it
      q->tail = (q->tail + 1) % q->size;
      q->nelem--;
      SST_INT_UNLOCK();
      SST_signal(&(q->sem));	// release lock on queue (lock and unlock INTs)
    }
  } else {  // there is no data available, so the task will be "suspended"
  SST_INT_LOCK();
  os_printf("DEBUG: NO DATA AVAILABLE!\n");
  // REVIEW: SST_dequeue - Add function addTaskSemQueue
  addTaskSemQueue(&(q->sem));
  /* CHANGED: SST_dequeue - Adding to semaphore's queue
  * TaskCB *tcb  = &l_taskCB[SST_currPrio_ - 1];  // Get the current the TCB of current running task
  * Semaphore *sem = &(q->sem);
  * sem->queue |= tcb->mask__;  // Add the task to the semaphore queue
  */
  SST_INT_UNLOCK();
}
return w;
}
