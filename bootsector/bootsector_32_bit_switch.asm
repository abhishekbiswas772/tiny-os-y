[bits 16]

switch_to_pm:
    cli                         ; 1. disable interrupts — BIOS ISRs are 16-bit, unusable in PM
    lgdt [gdt_descriptor]       ; 2. load the GDT so the CPU knows the segment layout
    mov eax, cr0
    or eax, 0x1
    mov cr0, eax                ; 3. set PE bit in cr0 — CPU is now in protected mode
    jmp CODE_SEG:init_pm        ; 4. far jump flushes the pipeline and loads CS with CODE_SEG

[bits 32]
init_pm:
    mov ax, DATA_SEG            ; 5. update all data segment registers to DATA_SEG
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov ebp, 0x90000            ; 6. move stack to top of free memory (below BIOS)
    mov esp, ebp

    call BEGIN_PM               ; 7. jump into our 32-bit code
