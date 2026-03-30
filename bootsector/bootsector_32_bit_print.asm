[bits 32]

VIDEO_MEMORY equ 0xb8000
WHITE_ON_BLACK equ 0x0f

print_string_pm:
    pusha
    mov edx, VIDEO_MEMORY

print_string_pm_loop:
    mov al, [ebx]           ; load next character
    mov ah, WHITE_ON_BLACK  ; colour attribute
    cmp al, 0               ; null terminator = end of string
    je print_string_pm_done
    mov [edx], ax           ; write char + colour to VGA memory
    add ebx, 1              ; next character
    add edx, 2              ; next VGA cell (2 bytes: char + colour)
    jmp print_string_pm_loop

print_string_pm_done:
    popa
    ret
