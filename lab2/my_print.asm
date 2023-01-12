global	asm_print

section .text
asm_print:
  push  ebp
  mov  ebp, esp
  mov  ecx, [ebp+8] ;指向字符串首地址
  mov  edx, [ebp+12] ;长度
  mov  ebx, 1
  mov  eax, 4
  int  80h
  pop  ebp
  ret
