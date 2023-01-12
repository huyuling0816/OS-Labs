
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                            proto.h
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                                                    Forrest Yu, 2005
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

#include "proc.h"

/* klib.asm */
PUBLIC void	out_byte(u16 port, u8 value);
PUBLIC u8	in_byte(u16 port);
PUBLIC void	disp_str(char * info);
PUBLIC void	disp_color_str(char * info, int color);

/* protect.c */
PUBLIC void	init_prot();
PUBLIC u32	seg2phys(u16 seg);

/* klib.c */
PUBLIC void	delay(int time);
PUBLIC char *c_itoa(int num, char *str, int radix);

/* kernel.asm */
void restart();

/* main.c */
// 加入进程
void init();
void A();
void B();
void C();
void D();
void E();
void F();

/* i8259.c */
PUBLIC void put_irq_handler(int irq, irq_handler handler);
PUBLIC void spurious_irq(int irq);

/* clock.c */
PUBLIC void clock_handler(int irq);

/* 以下是系统调用相关 */

/* proc.c */
PUBLIC  int     sys_get_ticks();        /* sys_call */
PUBLIC  int     sys_sleep(int milli_sec);
PUBLIC  int     sys_print(char *str);
PUBLIC  int     sys_color_print(char *str, int color);
PUBLIC  int     sys_P(SEMAPHORE *s);
PUBLIC  int     sys_V(SEMAPHORE *s);
PUBLIC  int     sem_init();
PUBLIC  int     reader(char *name, int cost);
PUBLIC  int     writer(char *name, int cost);

/* syscall.asm */
PUBLIC  void    sys_call();             /* int_handler */
PUBLIC  int     get_ticks();
PUBLIC  int     sleep(int milli_sec);
PUBLIC  int     print(char *str);
PUBLIC  int     color_print(char *str, int color);
PUBLIC  int     P(SEMAPHORE *s);
PUBLIC  int     V(SEMAPHORE *s);
