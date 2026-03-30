[bits 32]
[extern main]   ; main() is defined in kernal_entry.c

call main       ; call the C kernel entry point
jmp $           ; halt if main ever returns
