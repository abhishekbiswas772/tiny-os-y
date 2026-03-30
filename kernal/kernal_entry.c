void main() {
    char* vga = (char*) 0xb8000;

    // Each VGA cell = 2 bytes: [char][colour]
    // Row 1 (second row) starts at offset 80*2 = 160
    vga[160] = 'H';
    vga[161] = 0x0f;    // white text on black background
}
