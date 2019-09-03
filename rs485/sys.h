#ifndef _SYS_H_
#define _SYS_H_


/* init base system
 * 16MHz HSI clock, TIM4 1ms systick
 */
extern int sys_init(void);

typedef unsigned long clock_t;
typedef void (*Task)(void);

extern int sys_task_reg_timer(clock_t ms, Task foo);
extern int sys_task_reg_alarm(clock_t ms, Task foo);
extern int sys_task_reg_event(int evt, Task foo);
extern int sys_task_destory(int task_id);

/* system event functions
 * evt: 1~31
 */
extern int sys_event_trigger(int evt);
extern int sys_event_clear(int evt);

extern void sys_run(void);

/* system uptime, unit:1ms */
extern clock_t sys_uptime(void);

#define TASK_STACK_SIZE  16
#define EVENT_SYSTICKS   0


#endif /* _SYS_H_ */
