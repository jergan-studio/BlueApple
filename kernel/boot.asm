; BlueApple Multiboot 1 entry point
BITS 32

section .multiboot
align 4
    dd 0x1BADB002
    dd 0x00000007
    dd -(0x1BADB002 + 0x00000007)
    dd 0                   ; Request linear graphics mode
    dd 1024                ; Preferred width
    dd 768                 ; Preferred height
    dd 32                  ; Preferred color depth

section .text
global _start
extern kernel_main

_start:
    cli
    mov esp, stack_top
    push ebx                ; Multiboot information pointer
    push eax                ; Multiboot boot magic
    call kernel_main
    add esp, 8

.hang:
    hlt
    jmp .hang

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:
