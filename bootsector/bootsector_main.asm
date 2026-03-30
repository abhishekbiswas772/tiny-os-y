[bits 16]
[org 0x7c00]

KERNAL_OFFSET equ 0x1000

    mov [BOOT_DRIVE], dl
    mov bp, 0x9000
    mov sp, bp

    mov bx, MSG_REAL_MODE
    call print
    call print_nl

    call load_kernal
    call switch_to_pm
    jmp $                   ; never reached


%include "bootsector_print.asm"
%include "bootsector_print_hex.asm"
%include "bootsector_disc.asm"
%include "bootsector_32_bit_gdt.asm"
%include "bootsector_32_bit_switch.asm"
%include "bootsector_32_bit_print.asm"


[bits 16]
load_kernal:
    mov bx, MSG_LOAD_KERNAL
    call print
    call print_nl
    mov bx, KERNAL_OFFSET
    mov dh, 2
    mov dl, [BOOT_DRIVE]
    call disk_load
    ret


; ── 32-bit entry — must be BEFORE the padding so it stays inside the 512 bytes
[bits 32]
BEGIN_PM:
    mov ebx, MSG_PROT_MODE
    call print_string_pm
    call KERNAL_OFFSET      ; jump into the kernel
    jmp $


BOOT_DRIVE      db 0
MSG_REAL_MODE   db "Starting in 16-bit real mode", 0
MSG_PROT_MODE   db "Landed in 32-bit protected mode", 0
MSG_LOAD_KERNAL db "Loading kernel into memory", 0

times 510-($-$$) db 0
dw 0xaa55
