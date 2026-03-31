; KernalOS — GRUB Multiboot2 entry point
; GRUB loads us at 0x100000 in 32-bit protected mode:
;   eax = 0x36d76289 (Multiboot2 magic)
;   ebx = physical address of multiboot2 info struct

bits 32

; ── Multiboot2 header ──────────────────────────────────────────────────────
section .multiboot2
align 8
mb2_start:
    dd 0xE85250D6                               ; magic
    dd 0                                        ; arch: 32-bit protected mode
    dd mb2_end - mb2_start                      ; header length
    dd -(0xE85250D6 + 0 + (mb2_end - mb2_start)); checksum
    ; End tag
    dw 0
    dw 0
    dd 8
mb2_end:

; ── BSS: kernel stack (16 KB) ─────────────────────────────────────────────
section .bss
align 16
stack_bottom:
    resb 16384
stack_top:

; ── Read-only data: our own GDT ──────────────────────────────────────────
section .rodata
align 8
gdt_start:
    ; Null descriptor (required)
    dq 0
gdt_code:
    ; Code segment: base=0, limit=4G, 32-bit, ring 0
    dw 0xFFFF       ; limit low
    dw 0x0000       ; base low
    db 0x00         ; base mid
    db 10011010b    ; access: present, ring0, code, readable
    db 11001111b    ; flags: 4K granularity, 32-bit + limit high
    db 0x00         ; base high
gdt_data:
    ; Data segment: base=0, limit=4G, 32-bit, ring 0
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b    ; access: present, ring0, data, writable
    db 11001111b
    db 0x00
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1   ; GDT size
    dd gdt_start                  ; GDT address

; ── Kernel entry point ────────────────────────────────────────────────────
section .text
global _start
extern kernel_main

_start:
    ; Install our own GDT (GRUB's is temporary per Multiboot2 spec)
    lgdt [gdt_descriptor]

    ; Reload segment registers with our GDT selectors
    mov  ax, 0x10           ; data segment selector (offset 16)
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax
    mov  ss, ax

    ; Far jump to reload CS with code segment selector (offset 8)
    jmp  0x08:.reload_cs

.reload_cs:
    ; Set up stack (GRUB may not have done this)
    mov  esp, stack_top
    and  esp, -16           ; 16-byte align for GCC calling convention

    ; eax = multiboot magic, ebx = mbi pointer (currently unused)
    call kernel_main

    ; Keep interrupts enabled so keyboard IRQ1 still fires
    sti
.hang:
    hlt
    jmp .hang
