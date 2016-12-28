/*	=========================================================================
+	~~~   Mailbox application   ~~~																				+
+	This application is showing an example using a mailbox implemented in	+
+	the SST. There are 3 tasks, A, B and C. Task A tries to send data to 	+
+	the mailbox and posts to task B, which tries to send data to the 			+
+	mailbox too. Task C tries to read data from the mailbox when is 			+
+	is available. Task A and Task C are called every 5 and 3 seconds,			+
+	respectively.	As mentioned befeore, task B is called by task A  			+
+	eventually.																														+
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
uint8_t sdata;

static volatile os_timer_t timerTA;
static volatile os_timer_t timerTB;
static volatile os_timer_t timerTC;
static volatile os_timer_t global_timer;

Mailbox mb;

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
*	It is used as a 100ms tick signal and to blink a LED each 1s.
*	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/
static void tickISR() {
	time = time + 1;

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
*	Every 5 seconds task A tries to send data (99) to the mailbox and when it
*	sends successfully, it posts to task B.
*	~~~~~~~~~~~~~~~~~~
*/
void task_A(SSTEvent e)
{
	if (!(e.sig == INIT_SIG)) {
		os_printf("At time %d\n", time);
		if (e.sig == SIGNAL_SEM_SIG) {
			os_printf("SIGNAL EVENT ON TASK A!\n");
		}
		sdata = 99;
		os_printf("Task A tries to send data to the mailbox... ");
		if (SST_send(&mb, &sdata)) {
			os_printf("and the data was sent successfully!\n");
			os_printf("Task A posts to task B\n");
			SST_post(TASK_B_PRIO, TICK_SIG, 0);
		} else {
			os_printf("but it cannot send the data :/\n");
		}
	} else {
		os_printf("Task A was created!\n");
	}
}

/*	~~~   Task B   ~~~
*	When task B is executed it tries to send data (101) to the mailbox.
*	~~~~~~~~~~~~~~~~~~
*/
void task_B(SSTEvent e) {
	if (!(e.sig == INIT_SIG)) {
		os_printf("At time %d\n", time);
		if (e.sig == SIGNAL_SEM_SIG) {
			os_printf("SIGNAL EVENT ON TASK B!\n");
		}
		os_printf("Task B tries to send data to the mailbox... ");
		sdata = 101;
		if (SST_send(&mb, &sdata)) {
			os_printf("and the data was sent successfully!\n");
		} else {
			os_printf("but it cannot send the data :/\n");
		}
	} else {
		os_printf("Task B was created!\n");
	}
}

/*	~~~   Task C   ~~~
*	Every 3 seconds task C tries to read data from the mailbox.
*	~~~~~~~~~~~~~~~~~~
*/
void task_C(SSTEvent e) {
	if (!(e.sig == INIT_SIG)) {
		os_printf("At time %d\n", time);
		if (e.sig == SIGNAL_SEM_SIG) {
			os_printf("SIGNAL EVENT ON TASK C!\n");
		}
		uint8_t data;
		os_printf("Task C tries to get data on the mailbox... ");
		if (SST_receive(&mb, &data)) {
			os_printf("and it receives the data: %d\n", data);
		} else {
			os_printf("but there is no data available :/\n");
		}
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
	os_timer_disarm(&timerTC);

	// Setup timer - Set ISRs calls to the specific timer
	os_timer_setfn(&global_timer, (os_timer_func_t *)tickISR, NULL);
	os_timer_setfn(&timerTA, (os_timer_func_t *)timerTA_ISR, NULL);
	os_timer_setfn(&timerTC, (os_timer_func_t *)timerTC_ISR, NULL);


	// Arm the timer - Configure timers with a specific value
	os_timer_arm(&global_timer, 1000, 1);
	os_timer_arm(&timerTA, 5000, 1);
	os_timer_arm(&timerTC, 3000, 1);

	// Initialize the semaphore
	SST_initMailbox(&mb);

	// Start SST with tasks
	// Task A is created
	SST_task(&task_A, TASK_A_PRIO, taskAQueue, sizeof(taskAQueue)/sizeof(taskAQueue[0]), INIT_SIG, 0);

	// Task B is created
	SST_task(&task_B, TASK_B_PRIO, taskBQueue, sizeof(taskBQueue)/sizeof(taskBQueue[0]), INIT_SIG, 0);

	// Task C is created
	SST_task(&task_C, TASK_C_PRIO, taskCQueue, sizeof(taskCQueue)/sizeof(taskCQueue[0]), INIT_SIG, 0);

	// Start SST scheduler
	SST_run();

	//Start os task
	//system_os_task(user_procTask, user_procTaskPrio,user_procTaskQueue, user_procTaskQueueLen);
}
