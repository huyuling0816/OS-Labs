
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
			      console.c
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
						    Forrest Yu, 2005
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/*
	回车键: 把光标移到第一列
	换行键: 把光标前进到下一行
*/


#include "type.h"
#include "const.h"
#include "protect.h"
#include "string.h"
#include "proc.h"
#include "tty.h"
#include "console.h"
#include "global.h"
#include "keyboard.h"
#include "proto.h"

PRIVATE void set_cursor(unsigned int position);
PRIVATE void set_video_start_addr(u32 addr);
PRIVATE void flush(CONSOLE* p_con);

PRIVATE void clearSearchWords(CONSOLE *p_con);
PRIVATE void clearColor(CONSOLE *p_con);
PUBLIC void exit_esc(CONSOLE *p_con);
PUBLIC void search(CONSOLE *p_con);
PUBLIC void rollback(CONSOLE *p_con);

/**
 * 记忆化栈记录光标位置，然后往后移动一格
*/
PUBLIC void push_stk(CONSOLE* p_console, unsigned int pos){
	p_console->pos_stk.pos[p_console->pos_stk.ptr++] = pos;
}

/**
 * 往前移动一格并返回此位置的值
*/
PUBLIC unsigned int pop_stk(CONSOLE* p_console){
	if(p_console->pos_stk.ptr == 0) return 0;
	else{
		(p_console->pos_stk.ptr)--;
		return p_console->pos_stk.pos[p_console->pos_stk.ptr];
	}
}

/*======================================================================*
			   init_screen
 *======================================================================*/

PUBLIC void init_screen(TTY* p_tty)
{
	int nr_tty = p_tty - tty_table;
	p_tty->p_console = console_table + nr_tty;

	int v_mem_size = V_MEM_SIZE >> 1;	/* 显存总大小 (in WORD) */

	int con_v_mem_size                   = v_mem_size / NR_CONSOLES;
	p_tty->p_console->original_addr      = nr_tty * con_v_mem_size;
	p_tty->p_console->v_mem_limit        = con_v_mem_size;
	p_tty->p_console->current_start_addr = p_tty->p_console->original_addr;

	/* 默认光标位置在最开始处 */
	p_tty->p_console->cursor = p_tty->p_console->original_addr;

	if (nr_tty == 0) {
		/* 第一个控制台沿用原来的光标位置 */
		p_tty->p_console->cursor = disp_pos / 2;
		disp_pos = 0;
	}
	else {
		out_char(p_tty->p_console, nr_tty + '0');
		out_char(p_tty->p_console, '#');
	}

	set_cursor(p_tty->p_console->cursor);
}


/*======================================================================*
			   is_current_console
*======================================================================*/
PUBLIC int is_current_console(CONSOLE* p_con)
{
	return (p_con == &console_table[nr_current_console]);
}

/*======================================================================*
			   exit_esc
 *======================================================================*/
void clearColor(CONSOLE *p_con){
	for(int i=0; i<p_con->start_cursor*2; i+=2){
		// V_MEM_BASE: base of color video memory
		*(u8 *) (V_MEM_BASE+i+1) = DEFAULT_CHAR_COLOR;
	}
}

void clearSearchWords(CONSOLE *p_con){
	u8 *p_v = (u8*)(V_MEM_BASE + p_con->cursor*2);
	int len = p_con->cursor - p_con->start_cursor;
	for(int i=0; i<len; i++){
		*(p_v-2-2*i) = ' ';
		*(p_v-1-2*i) = DEFAULT_CHAR_COLOR;
	}
}

PUBLIC void exit_esc(CONSOLE *p_con){
	// 清除搜索词
	clearSearchWords(p_con);
	// 退出时，红色变为白色
	clearColor(p_con);
	// 光标回到进入esc模式之前，并恢复ptr
	p_con->cursor = p_con->start_cursor;
	p_con->pos_stk.ptr = p_con->pos_stk.start;
	// 刷新屏幕
	flush(p_con);
}


/*======================================================================*
			   search
 *======================================================================*/
PUBLIC void search(CONSOLE *p_con){
	int wordLen = (p_con->cursor-p_con->start_cursor)*2;
	for (int i=0; i<p_con->start_cursor*2; i+=2){
		int start = i;
		int end = i;
		int isFound = 1;
		for(int j=0; j<wordLen; j+=2){
			char src = *((u8*) (V_MEM_BASE + end));
			int index = j + p_con->start_cursor*2;
			char tgt = *((u8*) (V_MEM_BASE + index)); // searchWord中的第j个字符
			if(src == tgt){
				end += 2;
			}else{
				isFound = 0;
				break;
			}
		}
		if(isFound==1){
			// 标红
			for(int k=start; k<end; k+=2){
				if(*(u8*)(V_MEM_BASE+k) == ' ' || *(u8*)(V_MEM_BASE+k) == '\0'){
					*(u8*)(V_MEM_BASE+k+1) = RED_BACKGROUND_COLOR;
				}else{
					*(u8*)(V_MEM_BASE+k+1) = RED_CHAR_COLOR;//第一个字节是ascii码，第二个表示颜色
				}
			}
		}
	}
}

/*======================================================================*
			   rollback
 *======================================================================*/
PUBLIC void rollback(CONSOLE* p_con){
	if(p_con->pos_stk.input_ptr<0) return;

    char ch = p_con->pos_stk.input[p_con->pos_stk.input_ptr];
	p_con->pos_stk.input_ptr -= 1;

	u8* p_vmem = (u8*)(V_MEM_BASE + p_con->cursor * 2);

	switch (ch)
	{
	case '\b': //增加一个字符
	{
		int count = 0;
		int firstB = p_con->pos_stk.input_ptr + 1;
		if(p_con->pos_stk.input_ptr<0){return;}
		for (int i=p_con->pos_stk.input_ptr+1; i>=0 ;i--){
			if(p_con->pos_stk.input[i] == '\b') count+=1;
			else{
				firstB = i+1;
				break;
			}
		}
		if(firstB - count < 0) {return;}
		ch = p_con->pos_stk.input[firstB - count];
		// // 删去这个字符
		// for(int i=firstB-count; i<p_con->pos_stk.input_ptr; i++){
		// 	p_con->pos_stk.input[i] = p_con->pos_stk.input[i+1];
		// }
		// p_con->pos_stk.input_ptr -= 1;
		if(ch == '\t'){
			if (p_con->cursor < p_con->original_addr +
            	p_con->v_mem_limit - TAB_WIDTH) {
            	push_stk(p_con, p_con->cursor);
            	p_con->cursor += TAB_WIDTH;
				for(int i=0; i<TAB_WIDTH; i++){
					*p_vmem++ = '\0';
					*p_vmem++ = DEFAULT_CHAR_COLOR;
				}
        	}
		}else{
			if (p_con->cursor <
			    p_con->original_addr + p_con->v_mem_limit - 1) {
				push_stk(p_con, p_con->cursor);
				*p_vmem++ = ch;
				// *p_vmem++ = DEFAULT_CHAR_COLOR;
				if(*p_vmem == ' '){
					*p_vmem++ = DEFAULT_CHAR_COLOR;
				}else{
					if(mode == 0){
						*p_vmem++ = DEFAULT_CHAR_COLOR;
					}else{
						*p_vmem++ = RED_CHAR_COLOR;
					}
				}
				p_con->cursor++;
			}
		}
		break;
	}
	default: //删除一个字符
		if (p_con->cursor > p_con->original_addr) {
			// p_con->cursor--;
			// *(p_vmem-2) = ' ';
			// *(p_vmem-1) = DEFAULT_CHAR_COLOR;
			int temp = p_con->cursor;
			p_con->cursor = pop_stk(p_con);
			for(int i=0;i<temp-p_con->cursor;i++){
				*(p_vmem-2*i-2) = ' ';
				*(p_vmem-2*i-1) = DEFAULT_CHAR_COLOR;
			}
		}
		break;
	}

	while (p_con->cursor >= p_con->current_start_addr + SCREEN_SIZE) {
		scroll_screen(p_con, SCR_DN);
	}

	flush(p_con);

}

/*======================================================================*
			   out_char
 *======================================================================*/
PUBLIC void out_char(CONSOLE* p_con, char ch)
{   p_con->pos_stk.input_ptr += 1;
	p_con->pos_stk.input[p_con->pos_stk.input_ptr] = ch;
	u8* p_vmem = (u8*)(V_MEM_BASE + p_con->cursor * 2);
	switch(ch) {
	case '\n':
		if (p_con->cursor < p_con->original_addr +
		    p_con->v_mem_limit - SCREEN_WIDTH) {
			push_stk(p_con, p_con->cursor);//存储光标位置
			p_con->cursor = p_con->original_addr + SCREEN_WIDTH * 
				((p_con->cursor - p_con->original_addr) /
				 SCREEN_WIDTH + 1);
		}
		break;
	case '\b':// 删除
		if (p_con->cursor > p_con->original_addr) {
			// p_con->cursor--;
			// *(p_vmem-2) = ' ';
			// *(p_vmem-1) = DEFAULT_CHAR_COLOR;
			int temp = p_con->cursor;
			p_con->cursor = pop_stk(p_con);
			for(int i=0;i<temp-p_con->cursor;i++){
				*(p_vmem-2*i-2) = ' ';
				*(p_vmem-2*i-1) = DEFAULT_CHAR_COLOR;
			}
		}
		break;
	case '\t':
        if (p_con->cursor < p_con->original_addr +
            p_con->v_mem_limit - TAB_WIDTH) {
            push_stk(p_con, p_con->cursor);
            p_con->cursor += TAB_WIDTH;
			for(int i=0; i<TAB_WIDTH; i++){
				*p_vmem++ = '\0';
				*p_vmem++ = DEFAULT_CHAR_COLOR;
			}
        }
        break;
	default:
		if (p_con->cursor <
		    p_con->original_addr + p_con->v_mem_limit - 1) {
			push_stk(p_con, p_con->cursor);
			*p_vmem++ = ch;
			// *p_vmem++ = DEFAULT_CHAR_COLOR;
			if(*p_vmem == ' '){
				*p_vmem++ = DEFAULT_CHAR_COLOR;
			}else{
				if(mode == 0){
					*p_vmem++ = DEFAULT_CHAR_COLOR;
				}else{
					*p_vmem++ = RED_CHAR_COLOR;
				}
			}
			p_con->cursor++;
		}
		break;
	}

	while (p_con->cursor >= p_con->current_start_addr + SCREEN_SIZE) {
		scroll_screen(p_con, SCR_DN);
	}

	flush(p_con);
}

/*======================================================================*
                           flush
*======================================================================*/
PRIVATE void flush(CONSOLE* p_con)
{
        set_cursor(p_con->cursor);
        set_video_start_addr(p_con->current_start_addr);
}

/*======================================================================*
			    set_cursor
 *======================================================================*/
PRIVATE void set_cursor(unsigned int position)
{
	disable_int();
	out_byte(CRTC_ADDR_REG, CURSOR_H);
	out_byte(CRTC_DATA_REG, (position >> 8) & 0xFF);
	out_byte(CRTC_ADDR_REG, CURSOR_L);
	out_byte(CRTC_DATA_REG, position & 0xFF);
	enable_int();
}

/*======================================================================*
			  set_video_start_addr
 *======================================================================*/
PRIVATE void set_video_start_addr(u32 addr)
{
	disable_int();
	out_byte(CRTC_ADDR_REG, START_ADDR_H);
	out_byte(CRTC_DATA_REG, (addr >> 8) & 0xFF);
	out_byte(CRTC_ADDR_REG, START_ADDR_L);
	out_byte(CRTC_DATA_REG, addr & 0xFF);
	enable_int();
}



/*======================================================================*
			   select_console
 *======================================================================*/
PUBLIC void select_console(int nr_console)	/* 0 ~ (NR_CONSOLES - 1) */
{
	if ((nr_console < 0) || (nr_console >= NR_CONSOLES)) {
		return;
	}

	nr_current_console = nr_console;

	set_cursor(console_table[nr_console].cursor);
	set_video_start_addr(console_table[nr_console].current_start_addr);
}

/*======================================================================*
			   scroll_screen
 *----------------------------------------------------------------------*
 滚屏.
 *----------------------------------------------------------------------*
 direction:
	SCR_UP	: 向上滚屏
	SCR_DN	: 向下滚屏
	其它	: 不做处理
 *======================================================================*/
PUBLIC void scroll_screen(CONSOLE* p_con, int direction)
{
	if (direction == SCR_UP) {
		if (p_con->current_start_addr > p_con->original_addr) {
			p_con->current_start_addr -= SCREEN_WIDTH;
		}
	}
	else if (direction == SCR_DN) {
		if (p_con->current_start_addr + SCREEN_SIZE <
		    p_con->original_addr + p_con->v_mem_limit) {
			p_con->current_start_addr += SCREEN_WIDTH;
		}
	}
	else{
	}

	set_video_start_addr(p_con->current_start_addr);
	set_cursor(p_con->cursor);
}

