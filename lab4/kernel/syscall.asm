
; ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
;                               syscall.asm
; ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
;                                                     Forrest Yu, 2005
; ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

%include "sconst.inc"

_NR_get_ticks       equ 0 ; 要跟 global.c 中 sys_call_table 的定义相对应！
_NR_sleep 			equ 1
_NR_print 			equ 2
_NR_color_print 	equ 3
_NR_P 				equ 4
_NR_V 				equ 5
INT_VECTOR_SYS_CALL equ 0x90

; 导出符号
global	get_ticks
global	sleep
global	print
global	color_print
global	P
global	V

bits 32
[section .text]

; ====================================================================
;                              get_ticks
; ====================================================================
get_ticks:
	mov	eax, _NR_get_ticks
	int	INT_VECTOR_SYS_CALL
	ret

sleep:
	push ebx
	mov ebx, [esp + 8]
	mov	eax, _NR_sleep
	int	INT_VECTOR_SYS_CALL
	pop ebx
	ret

print:
	push ebx
	mov ebx, [esp + 8]
	mov	eax, _NR_print
	int	INT_VECTOR_SYS_CALL
	pop ebx
	ret

color_print:
	push ebx
	mov ebx, [esp + 8]
	mov	ecx, [esp + 12]
	mov	eax, _NR_color_print
	int	INT_VECTOR_SYS_CALL
	pop ebx
	ret

P:
	push ebx
	mov ebx, [esp + 8]
	mov	eax, _NR_P
	int	INT_VECTOR_SYS_CALL
	pop ebx
	ret

V:
	push ebx
	mov ebx, [esp + 8]
	mov	eax, _NR_V
	int	INT_VECTOR_SYS_CALL
	pop ebx
	ret
