; GDT — Global Descriptor Table
; Tells the CPU how memory segments are laid out in protected mode.
; We use a flat model: both code and data cover the full 4GB address space.

gdt_start:
    ; Null descriptor — required as the first entry, CPU will fault without it
    dd 0x0
    dd 0x0

gdt_code:
    ; Code segment: base=0x0, limit=0xfffff, executable, readable
    dw 0xffff       ; limit bits 0-15
    dw 0x0          ; base  bits 0-15
    db 0x0          ; base  bits 16-23
    db 10011010b    ; access byte: present, ring-0, code, executable, readable
    db 11001111b    ; flags (4-bit): 32-bit, 4KB granularity | limit bits 16-19
    db 0x0          ; base  bits 24-31

gdt_data:
    ; Data segment: same span as code, writable
    dw 0xffff
    dw 0x0
    db 0x0
    db 10010010b    ; access byte: present, ring-0, data, writable
    db 11001111b
    db 0x0

gdt_end:

; GDT descriptor — pointer the CPU reads via lgdt
gdt_descriptor:
    dw gdt_end - gdt_start - 1  ; size (always one less than true size)
    dd gdt_start                 ; linear address of the GDT

; Segment selectors — byte offset of each descriptor in the GDT
CODE_SEG equ gdt_code - gdt_start   ; = 0x08
DATA_SEG equ gdt_data - gdt_start   ; = 0x10
