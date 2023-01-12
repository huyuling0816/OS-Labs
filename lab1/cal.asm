;定义初始化变量
SECTION .data
error_message db "Invalid", 0ah
msg_len  equ $-error_message

;定义未初始化的全局变量
SECTION .bss
sign: resb 1
first_sign: resb 1
second_sign: resb 1
result_sign: resb 1
len_first_num: resb 1
len_second_num: resb 1
input: resb 255    ;申请255bytes
first_num: resb 255
second_num: resb 255
result: resb 256

SECTION .text
global _start

;从输入行中解析出数字和符号
get_num_and_sign:
    mov ebx, 0    ;记录循环次数
.loop:
    cmp BYTE[ecx], 10    ;遇到换行符
    jz .rett1
    cmp BYTE[ecx], 57    ;遇到非数字
    jg .rett2
    cmp BYTE[ecx], 48    ;遇到非数字
    jl .rett2
    mov dl, BYTE[ecx]
    mov BYTE[eax], dl
    inc eax
    inc ecx
    add ebx, 1 
    jmp .loop
.rett1:
    inc ecx
    ret
.rett2:
    cmp ebx, 0
    jz .rett3    ;如果循环次数为0，表明是第一次碰到非符号数字，即sign
    ret
.rett3:
    mov dl, BYTE[ecx]
    mov BYTE[eax], dl
    inc ecx
    ret

;获取字符串的长度,edx先存放字符串地址，最后保存字符串长度; 寄存器传递参数
str_len:
    push ebx
    mov ebx, edx
.loop:
    cmp BYTE[ebx], 0
    jz .finish
    inc ebx
    jmp .loop
.finish:
    sub ebx, edx
    mov edx, 0
    mov edx, ebx
    pop ebx
    ret

;打印,eax指向要打印到字符串
print:
    push edx    ;被调用者保护
    push ecx    ;字符串地址放入ecx寄存器
    push ebx
    push eax
    mov edx, eax
    mov ecx, eax
    call str_len
    mov ebx, 1
    mov eax, 4    ;发起sys_write系统调用
    int 80h
    pop eax
    pop ebx
    pop ecx
    pop edx
    ret

print_sign:
    push edx    ;被调用者保护
    push ecx    ;字符串地址放入ecx寄存器
    push ebx
    push eax
    mov edx, 1
    mov ecx, eax
    mov ebx, 1
    mov eax, 4    ;发起sys_write系统调用
    int 80h
    pop eax
    pop ebx
    pop ecx
    pop edx
    ret

clear:
    mov eax, sign
    mov ebx, 2500
.clear_loop:
    cmp ebx, 0
    jz .clear_end
    mov BYTE[eax], 0
    inc eax
    dec ebx
    jmp .clear_loop
.clear_end:
    ret

;程序入口
_start:
    call clear
.main_loop:
    ;读取一行输入
    mov edx, 255    ;长度
    mov ecx, input    ;存放的地址
    mov ebx, 1
    mov eax, 3    ;read的系统调用号
    int 80h
    
    ;判断是否结束输入
    mov eax, input
    cmp BYTE[eax], 113
    jz .finish

    ;解析出两个数字及符号
    mov ecx, input

    cmp BYTE[ecx], 45
    jne .get_first_num
    mov eax, first_sign
    mov BYTE[eax], 45
    add ecx, 1
 
.get_first_num:
    mov eax, first_num
    call get_num_and_sign

    mov eax, sign
    call get_num_and_sign

    cmp BYTE[ecx], 45
    jne .get_second_num
    mov eax, second_sign
    mov BYTE[eax], 45
    add ecx, 1
.get_second_num:
    mov eax, second_num
    call get_num_and_sign

    ;判断符号是否有效
    mov eax, sign
    cmp BYTE[eax], 42
    jne .judge1
    cmp BYTE[eax], 43
    jne .judge2
.judge1:
    cmp BYTE[eax], 43
    jne .error
    jmp .main
.judge2:
    cmp BYTE[eax], 42
    jne .error
    jmp .main

.main:
    mov ebx, result
    add ebx, 255         ;ebx指向结果
    mov BYTE[ebx], 10    ;结果的最后加一个换行符   

    mov edx, first_num
    cmp BYTE[edx], 0
    jz .error
    mov esi, edx
    call str_len
    add esi, edx
    sub esi, 1    ;esi,变址寄存器,指向first_num的最后一位
    mov eax, len_first_num
    mov BYTE[eax], dl

    mov edx, second_num
    cmp BYTE[edx], 0
    jz .error
    mov edi, edx
    call str_len
    add edi, edx
    sub edi, 1    ;edi,变址寄存器,指向second_num的最后一位
    mov eax, len_second_num
    mov BYTE[eax], dl
    mov ecx,0    ;进位
  
    mov eax, sign
    cmp BYTE[eax], 43
    jz .add
    cmp BYTE[eax], 42
    jz .mul
    
.add:    
    mov eax, first_sign
    mov edx, second_sign
    mov al, BYTE[eax]
    mov dl, BYTE[edx]
    cmp al, dl
    jne .subtraction
    cmp al, 45    
    jne .mov_eax_0   ;如果是正数，直接开始加法
    mov eax, result_sign
    mov BYTE[eax], 45
.mov_eax_0:
    mov eax, 0   
.add_loop:
    cmp esi, first_num
    jl .handle_second_num
    cmp edi, second_num
    jl .handle_first_num
    mov eax, 0
    add al, BYTE[esi]
    add al, BYTE[edi]
    add al, cl
    sub al, 48
    mov ecx, 0    ;进位置为0
    sub esi, 1
    sub edi, 1
    sub ebx, 1
    mov BYTE[ebx], al
    cmp al, 58
    jl .add_loop  ;如果没有进位，直接跳转
    sub al, 10
    mov ecx, 1    ;进位置为1
    mov BYTE[ebx], al
    jmp .add_loop

.handle_second_num:
    cmp edi, second_num
    jl .judge_whether_have_last_carry
    mov eax, 0
    add al, BYTE[edi]
    add al, cl
    mov ecx, 0
    sub edi, 1
    sub ebx, 1
    mov BYTE[ebx], al
    cmp al, 58
    jl .handle_second_num
    sub al, 10
    mov ecx, 1
    mov BYTE[ebx], al
    jmp .handle_second_num

.handle_first_num:
    cmp esi, first_num
    jl .judge_whether_have_last_carry
    mov eax, 0
    add al, BYTE[esi]
    add al, cl
    mov ecx, 0
    sub esi, 1
    sub ebx, 1
    mov BYTE[ebx], al
    cmp al, 58
    jl .handle_first_num
    sub al, 10
    mov ecx, 1
    mov BYTE[ebx], al
    jmp .handle_first_num

.judge_whether_have_last_carry:
    cmp ecx, 1
    jz .add_last_carry
    jmp .end_of_one_loop

.add_last_carry:
    sub ebx, 1
    mov BYTE[ebx], 49
    jmp .end_of_one_loop

   ;减法开始,两个数字一正一负
.subtraction:
    mov edx, first_num
    call str_len
    mov eax, edx ;eax第一个数字的长度
    mov edx, second_num
    call str_len
    mov ecx, edx ;ecx第二个数字的长度
   ;先判断哪个长
    cmp eax, ecx
    jg .first_second
    cmp eax, ecx
    jl .second_first

    mov eax, first_num
    mov ecx, second_num
    mov edx, 0

.cmp_loop: ;长度一样的情况下，再比较大小
    mov dh, BYTE[eax] ;dh第一个数字
    mov dl, BYTE[ecx] ;dl第二个数字
    cmp dh, 0 ;说明已经比较完，两个数字完全相同
    jz .set_result_zero
    cmp dh, dl
    jg .first_second
    cmp dh, dl
    jl .second_first
    add eax, 1
    add ecx, 1
    jmp .cmp_loop

   ;第一个数减去第二个数esi-edi ebx指向结果
.first_second:
    mov eax, first_sign
    mov al, BYTE[eax]
    mov ecx, result_sign
    mov BYTE[ecx], al
.sub_loop1:
    cmp BYTE[esi], 0
    jz .eliminate_zero
    cmp BYTE[edi], 0
    jne .continue_sub_loop1
    add BYTE[edi], 48 ;短的数字补0
.continue_sub_loop1:
    mov eax, 0
    mov ecx, 0
    mov al, BYTE[esi]
    mov cl, BYTE[edi]
    cmp al, cl
    jl .borrow1
    ; 不需要借位
    sub al, cl
    add al, 48 ;转化为数字
    sub ebx, 1
    mov BYTE[ebx], al
    sub esi, 1
    sub edi, 1
    jmp .sub_loop1

.borrow1:
    add al, 10 ;借位
    sub al, cl
    add al, 48;
    sub ebx, 1
    mov BYTE[ebx], al
    mov eax, esi
.borrow_loop1:
    sub eax, 1
    cmp BYTE[eax], 48
    jz .borrow_loop_continue1
    sub BYTE[eax], 1
    sub esi, 1
    sub edi, 1
    jmp .sub_loop1
.borrow_loop_continue1: ;如果被借位的数字是0,则变成9,继续向前借位
    add BYTE[eax], 9
    jmp .borrow_loop1

   ;消除多余的0
.eliminate_zero:
    cmp BYTE[ebx], 48
    jne .end_of_one_loop
    add ebx, 1
    jmp .eliminate_zero

   ;第二个数减去第一个数edi-esi
.second_first:
    mov eax, second_sign
    mov al, BYTE[eax]
    mov ecx, result_sign
    mov BYTE[ecx], al
.sub_loop2:
    cmp BYTE[edi], 0
    jz .eliminate_zero
    cmp BYTE[esi], 0
    jne .continue_sub_loop2
    add BYTE[esi], 48 ;短的数字补0
.continue_sub_loop2:
    mov eax, 0
    mov ecx, 0
    mov al, BYTE[edi]
    mov cl, BYTE[esi]
    cmp al, cl
    jl .borrow2
    ; 不需要借位
    sub al, cl
    add al, 48 ;转化为数字
    sub ebx, 1
    mov BYTE[ebx], al
    sub esi, 1
    sub edi, 1
    jmp .sub_loop2

.borrow2:
    add al, 10 ;借位
    sub al, cl
    add al, 48;
    sub ebx, 1
    mov BYTE[ebx], al
    mov eax, edi
.borrow_loop2:
    sub eax, 1
    cmp BYTE[eax], 48
    jz .borrow_loop_continue2
    sub BYTE[eax], 1
    sub esi, 1
    sub edi, 1
    jmp .sub_loop2
.borrow_loop_continue2: ;如果被借位的数字是0,则变成9,继续向前借位
    add BYTE[eax], 9
    jmp .borrow_loop2

   
.set_result_zero:
    sub ebx, 1
    mov BYTE[ebx], 48
    jmp .end_of_one_loop
;乘法开始
.mul:
    ;首先判断结果符号
    mov eax, first_sign
    mov edx, second_sign
    mov al, BYTE[eax]
    mov dl, BYTE[edx]
    cmp al, dl
    jz .continue
    mov eax, result_sign
    mov BYTE[eax], 45
.continue:
    ;判断有没有乘0
    mov eax, first_num
    mov edx, second_num
    cmp BYTE[eax], 48
    jz .set_result_zero
    cmp BYTE[edx], 48
    jz .set_result_zero

.out_loop: 
    cmp edi, second_num
    jl .cal_result
    sub BYTE[edi], 48

.inner_loop:
    cmp esi, first_num
    jl .rest_of_out_loop
    sub ebx, 1
    sub BYTE[esi], 48
    mov eax, 0
    mov al, BYTE[esi]
    mul BYTE[edi]    ;eax中存储两位数相乘的结果
    add al, cl   ;加上进位
    add al, BYTE[ebx]
    mov ecx, 0
    cmp eax, 9
    jg .more_than_ten_loop   ;判断两位数相加是否有进位
.return_inner_loop:
    mov BYTE[ebx], al 
    add BYTE[esi], 48 
    sub esi, 1
    jmp .inner_loop

.rest_of_out_loop:
    cmp cl, 0
    jne .add_carry_out_loop   ;判断一行内，最后一个数是否有进位
    mov eax, len_first_num
    mov edx, 0
    mov dl, BYTE[eax]
    add ebx, edx
    sub ebx, 1
.return_rest_of_out_loop:
    add esi, edx
    sub edi, 1
    jmp .out_loop

.add_carry_out_loop:
    sub ebx, 1
    mov BYTE[ebx], cl
    mov ecx, 0
    mov eax, len_first_num
    mov edx, 0
    mov dl, BYTE[eax]
    add ebx, edx
    jmp .return_rest_of_out_loop

.more_than_ten_loop:
    cmp al, 10
    jl .return_inner_loop
    sub al, 10
    add cl,1
    jmp .more_than_ten_loop

.cal_result:
    mov eax, len_first_num
    mov edx, 0
    mov dl, BYTE[eax]
    sub ebx, edx
    mov eax, 0    ;保存结果字符串的长度
    cmp BYTE[ebx], 0
    jne .turn_to_number_loop
    add ebx, 1
.turn_to_number_loop:
    cmp BYTE[ebx], 10
    jz .out
    add BYTE[ebx], 48
    add ebx, 1
    add eax, 1
    jmp .turn_to_number_loop
.out:
    sub ebx, eax

.end_of_one_loop:    ;显示结果并跳转
    mov eax, 0
    mov al, BYTE[ebx]
    cmp al, 48
    jne .print_result
    mov ecx, result_sign
    mov BYTE[ecx], 0 ;如果结果为0,去除符号
.print_result:
    mov eax, result_sign
    call print_sign
    mov eax, ebx
    call print
    call _start

.finish:    ;循环结束
    mov eax, 1
    mov ebx, 0
    int 80h

.error:
    mov eax, 4
    mov ebx, 1
    mov ecx, error_message
    mov edx, msg_len
    int 80h    
    call _start






















































