/*	=========================================================================
+	~~~   Semaphore application   ~~~																			+
+	This application is showing an example using a semaphore implemented	+
+	in the SST. There are three tasks: A, B and C. Task A toggles an LED,	+
+	task B uses a semaphore and posts to task C which will try to use the	+
+	but will be blocked since the semaphore would not be released at that +
+	time; then the execution goes back to task B that calls the function	+
+	signal in the semaphore, releasing it and allowing task C to get			+
+	access to the semaphore and continue its execution. Task A is called	+
+	every 500ms and task B is called every 2 seconds. Task C is called by	+
+	task B.																																+
+																																				+
+	Author:	Victor R. S. de Oliveira (vrso at if.ufal.br)									+
+	Course:	Sistemas de Tempo Real 2016.1																	+
+	Instituto de Computacao																								+
+	Universidade Federal de Alagoas																				+
=========================================================================
*/

#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"
//#include "user_config.h"
#include "uart.h"
#include "sst_port.h"
#include "sst_exa.h"
#include <inttypes.h>

// #define user_procTaskPrio        0
// #define user_procTaskQueueLen    1
// os_event_t    user_procTaskQueue[user_procTaskQueueLen];
// static void user_procTask(os_event_t *events);
uint32_t time = 0;

static volatile os_timer_t timerTA;
static volatile os_timer_t timerTB;
static volatile os_timer_t timerTC;
static volatile os_timer_t global_timer;

Semaphore sem;

static SSTEvent taskAQueue[2];
static SSTEvent taskBQueue[2];
static SSTEvent taskCQueue[2];

void SST_start(void) {
	// NOTE: SST_start is not implemented.
}
void SST_onIdle(void) {
	// NOTE: SST_onIdle is not implemented.
}

/*	~~~   TICK interrupt handler   ~~~
*	This function is called when the global_timer overflows.
*	It is used as a 1s tick signal for debugging.
*	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/
static void tickISR() {
	time = time + 1;
}

/*	~~~   Timer A interrupt handler   ~~~
*	This function is called when the timerTA overflows.
*	It posts a TICK_SIG to task A.
*	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/
static void timerTA_ISR() {
	uint8_t pin;

	SST_ISR_ENTRY(pin, TICK_ISR_PRIO);

	SST_post(TASK_A_PRIO, TICK_SIG, 0);     /* post the Tick to Task A */

	SST_ISR_EXIT(pin, 0);
}

/*	~~~   Timer B interrupt handler   ~~~
*	This function is called when the timerTB overflows.
*	It posts a TICK_SIG to task B.
*	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/
static void timerTB_ISR() {
	uint8_t pin;

	SST_ISR_ENTRY(pin, TICK_ISR_PRIO);

	SST_post(TASK_B_PRIO, TICK_SIG, 0);     /* post the Tick to Task B */

	SST_ISR_EXIT(pin, 0);
}

/*	~~~   Timer C interrupt handler   ~~~
*	This function is called when the timerTC overflows.
*	It posts a TICK_SIG to task C.
*	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/
static void timerTC_ISR() {
	uint8_t pin;
	SST_ISR_ENTRY(pin, TICK_ISR_PRIO);

	SST_post(TASK_C_PRIO, TICK_SIG, 0);     /* post the Tick to Task C */

	SST_ISR_EXIT(pin, 0);
}

/*
*	==================================================================
*	NOTE: Tasks cannot use semaphores during initialization.
*	When a task is created there is no information about its priority,
*	so it is impossible to know what task will be "suspended" in the
*	semaphore, unless SST original source code is modified.
*	The same happens when working with mailboxes (and queues), since
*	they use semaphores as mutexes.
*	==================================================================
*/

/*	~~~   Task A   ~~~
*	Toggles an LED and that's all.
*	~~~~~~~~~~~~~~~~~~
*/
void task_A(SSTEvent e)
{
	//Do blinky stuff
	if (GPIO_REG_READ(GPIO_OUT_ADDRESS) & BIT2)
	{
		//Set GPIO2 to LOW
		gpio_output_set(0, BIT2, BIT2, 0);
	}
	else
	{
		//Set GPIO2 to HIGH
		gpio_output_set(BIT2, 0, BIT2, 0);
	}

}

/*	~~~   Task B   ~~~
*	Every 2 seconds Task B calls wait(s) to get access of the semaphore
*	and if it takes the semaphore, it posts to task C. After that, it releases
*	the semaphore calling signal(s).
*	~~~~~~~~~~~~~~~~~~
*/
void task_B(SSTEvent e) {
	if (e.sig != INIT_SIG) {
		os_printf("At time %d\n", time);
		if (e.sig == SIGNAL_SEM_SIG) {
			os_printf("SIGNAL EVENT ON TASK B!\n");
		}
		os_printf("Task B tries to get the semaphore...\n");
		if (SST_wait(&sem)){
			os_printf("Semaphore is taken by task B\n");
			os_printf("Task B posts to task C before the release of the semaphore\n");
			SST_post(TASK_C_PRIO, TICK_SIG, 0);
			os_printf("Task B releases the semaphore!\n");
			SST_signal(&sem);
		}
	} else {
		os_printf("Task B was created!\n");
	}
}

/*	~~~   Task C   ~~~
*	When task C is executed it tries to access the semaphore by calling the
*	function wait(s). After it is done using the semaphore, it calls signal(s).
*	~~~~~~~~~~~~~~~~~~
*/
void task_C(SSTEvent e) {
	if (!(e.sig == INIT_SIG)) {
		os_printf("At time %d\n", time);
		if (e.sig == SIGNAL_SEM_SIG) {
			os_printf("SIGNAL EVENT ON TASK C!\n");
		}
		os_printf("Task C trying to access semaphore...\n");
		if (SST_wait(&sem)){
			os_printf("Task C has the semaphore... ");
			os_printf("and signals!\n");
			SST_signal(&sem);
		} else {
			os_printf("Task C couldn't get the semaphore :/\n");
		}
		// 	os_printf("queue: %d\n", sem.queue);
		// }
	} else {
		os_printf("Task C was created!\n");
	}
}

//Do nothing function
/*
static void ICACHE_FLASH_ATTR
user_procTask(os_event_t *events)
{
os_delay_us(10);
}*/

//	Init function
void ICACHE_FLASH_ATTR
user_init()
{
	// Initialize the GPIO subsystem.
	gpio_init();

	// Initialize UART with baud rate of 9600
	uart_init(BIT_RATE_9600, BIT_RATE_9600);

	// Set GPIO2 to output mode - This is the GPIO connected to the LED in the
	// ESP board
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);

	// Set GPIO2 low - Resets LED to 0
	gpio_output_set(0, BIT2, BIT2, 0);

	// Disarm timer - I think this is to create a timer reference and reset it
	os_timer_disarm(&global_timer);
	os_timer_disarm(&timerTA);
	os_timer_disarm(&timerTB);

	// Setup timer - Set ISRs calls to the specific timer
	os_timer_setfn(&global_timer, (os_timer_func_t *)tickISR, NULL);
	os_timer_setfn(&timerTA, (os_timer_func_t *)timerTA_ISR, NULL);
	os_timer_setfn(&timerTB, (os_timer_func_t *)timerTB_ISR, NULL);


	// Arm the timer - Configure the timer with a specific value
	os_timer_arm(&global_timer, 1000, 1);
	os_timer_arm(&timerTA, 500, 1);
	os_timer_arm(&timerTB, 2000, 1);

	// Initialize the semaphore
	SST_initSemaphore(&sem);

	// Start SST with tasks
	// Task A is created
	SST_task(&task_A, TASK_A_PRIO,taskAQueue, sizeof(taskAQueue)/sizeof(taskAQueue[0]), INIT_SIG, 0);

	// Task B is created
	SST_task(&task_B, TASK_B_PRIO, taskBQueue, sizeof(taskBQueue)/sizeof(taskBQueue[0]), INIT_SIG, 0);

	// Task C is created
	SST_task(&task_C, TASK_C_PRIO, taskCQueue, sizeof(taskCQueue)/sizeof(taskCQueue[0]), INIT_SIG, 0);

	// Start SST scheduler
	SST_run();

	//Start os task
	//system_os_task(user_procTask, user_procTaskPrio,user_procTaskQueue, user_procTaskQueueLen);
}
