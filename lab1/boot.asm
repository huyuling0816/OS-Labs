    org 07c00h    ;告诉编译器程序加载到7c00处，因为BIOS将软盘内容放在07c00h位置；伪指令是指，不生成对应的二进制指令，只是汇编器使用的
    mov ax,cs
    mov ds,ax
    mov es,ax
    call DispStr
    jmp $
DispStr:
    mov ax, BootMessage
    mov bp,ax
    mov cx,16
    mov ax,01301h
    mov bx,000ch
    mov dl,0
    int 10h
    ret
BootMessage:        db  "Hello, OS world!"
times   510-($-$$)  db  0 ;填充剩下的空间，使生成的二进制文件正好为512字节；硬盘的一个扇区就是512字节
dw 0xaa55
