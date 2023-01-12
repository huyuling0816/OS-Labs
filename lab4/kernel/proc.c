
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                               proc.c
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                                                    Forrest Yu, 2005
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

#include "type.h"
#include "const.h"
#include "protect.h"
#include "proto.h"
#include "string.h"
#include "proc.h"
#include "global.h"

PRIVATE void push(SEMAPHORE *s, PROCESS *p);
PRIVATE PROCESS *pop(SEMAPHORE *s);

/*======================================================================*
                              schedule
 *======================================================================*/
PUBLIC void schedule()
{
	PROCESS* p;
	int	 greatest_ticks = 0;
    
	// 进程在睡眠但到了wake时刻，唤醒进程
	for(p = proc_table; p<proc_table+NR_TASKS; p++){
		if(p->is_sleep==1){
			if(get_ticks() >= p->wake){
				p->is_sleep = 0;
				p->wake = 0;
			}
		}
	}

	while (!greatest_ticks) {
		for (p = proc_table; p < proc_table+NR_TASKS; p++) {
			// 睡眠或等待时间不分配时间片
			if(p->is_sleep || p->is_wait){
				continue;
			}
			if (p->ticks > greatest_ticks) {
				greatest_ticks = p->ticks;
				p_proc_ready = p;
			}
		}

		if (!greatest_ticks) {
			for (p = proc_table; p < proc_table+NR_TASKS; p++) {
				// 睡眠或等待时间不分配时间片
				if(p->is_sleep || p->is_wait){
					continue;
				}
				p->ticks = p->priority;
			}
		}
	}
}

/*======================================================================*
                           sys_get_ticks
 *======================================================================*/
PUBLIC int sys_get_ticks()
{
	return ticks;
}

// 进程睡眠，不分配时间片
PUBLIC int sys_sleep(int milli_sec){
	// ms转为tick
	// p_proc_ready->sleep_time = milli_sec / (1000 / HZ);
	p_proc_ready->is_sleep = 1;
	p_proc_ready->wake = get_ticks() + (milli_sec / (1000 / HZ));
	schedule();
	return 0;
}

// 打印字符串
PUBLIC int sys_print(char *str){
	disp_str(str);
	return 0;
}

// 带颜色打印字符串
PUBLIC int sys_color_print(char *str, int color)
{
	disp_color_str(str, color);
	return 0;
}

// 信号量P操作
PUBLIC int sys_P(SEMAPHORE *s){
	s->value--;
	// 剩余资源不够，进程进入信号量S的等待队列
	if(s->value<0){
		p_proc_ready->is_wait = 1;
		push(s, p_proc_ready);
		schedule();
	}
	return 0;
}

// 信号量V操作
PUBLIC int sys_V(SEMAPHORE *s){
	s->value++;
	// 还有别的进程在等待这个资源，则唤醒等待队列中的一个进程
	if(s->value <= 0){
		PROCESS *wait_process = pop(s);
		wait_process->is_wait = 0;
	}
	return 0;
}

// 信号量和控制变量
SEMAPHORE s_reader;
SEMAPHORE s_writer;
SEMAPHORE s_mutex;
SEMAPHORE *p_s_reader;
SEMAPHORE *p_s_writer;
SEMAPHORE *p_s_mutex;
int num_readers;

// 初始化信号量
PUBLIC int sem_init()
{
	// 允许同时读的读者数量
	s_reader.value = 3;
	s_reader.size = 0;
	// 只允许1个写者同时写
	s_writer.value = 1;
	s_writer.size = 0;
	// 用于保证对num_readers变量的互斥访问
	s_mutex.value = 1;
	s_mutex.size = 0;
	// 当前有多少读者在读
	num_readers = 0;
    p_s_reader = &s_reader;
	p_s_writer = &s_writer;
	p_s_mutex = &s_mutex;
	return 0;
}

PUBLIC int reader(char *name, int cost) {
	// 对num_readers变量的互斥访问，只有第一个读进程负责加锁
	P(p_s_mutex);
	// P(p_s_reader); 
	if(num_readers == 0){
		P(p_s_writer);
	}
	num_readers++;
	V(p_s_mutex);

	// 读文件
    milli_delay(cost);

    // 随后一个读进程负责解锁
	P(p_s_mutex);
	num_readers--;
	if(num_readers == 0){
		V(p_s_writer);
	}
	// V(p_s_reader); 
	V(p_s_mutex);

	// P(p_s_mutex);
	// P(p_s_reader);
	// if (num_readers == 0)
	// {
	// 	P(p_s_writer);
	// }
	// ++num_readers;
	// V(p_s_mutex);
	// // V(p_s_reader);

	// // 读开始
	// milli_delay(cost);

	// // P(p_s_reader);
	// --num_readers;
	// if (num_readers == 0)
	// {
	// 	V(p_s_writer);
	// }
	// V(p_s_reader);

	return 0;
}

PUBLIC int writer(char *name, int cost){
	P(p_s_writer);
	// 写文件
    milli_delay(cost);
	V(p_s_writer);

	// P(p_s_mutex);
	// P(p_s_writer);

	// // 写开始
	// milli_delay(cost);

	// V(p_s_writer);
	// V(p_s_mutex);

	return 0;
}

// 入队
PRIVATE void push(SEMAPHORE *s, PROCESS *p) {
	if(s->size < 50){
		s->queue[s->size] = p;
		s->size++;
	}
}

// 出队
PRIVATE PROCESS *pop(SEMAPHORE *s){
	PROCESS *p = s->queue[0];
	for(int i=0; i<s->size-1; ++i){
		s->queue[i] = s->queue[i+1];
	}
	s->size--;
	return p ;
}