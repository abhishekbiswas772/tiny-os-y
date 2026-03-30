export TARGET = i386-elf
export PREFIX = $(HOME)/opt/i386elfgcc
export PATH  := $(PREFIX)/bin:$(PATH)

# ── Tool paths ────────────────────────────────────────────────────────────────
ASM   = nasm
QEMU  = qemu-system-i386
CC    = $(PREFIX)/bin/$(TARGET)-gcc
LD    = $(PREFIX)/bin/$(TARGET)-ld
GDB   = $(shell command -v i386-elf-gdb 2>/dev/null || command -v gdb 2>/dev/null || echo "gdb")
GAWK  = $(shell command -v gawk 2>/dev/null)
NPROC = $(shell sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)

# Apple Silicon: GCC 4.9.1 cannot build natively on arm64.
# Run the entire cross-compiler build under Rosetta 2 (x86_64 emulation).
IS_ARM64 := $(shell [ "$$(uname -m)" = "arm64" ] && echo "1")
ifdef IS_ARM64
  ARCH_RUN  := arch -x86_64
  GMP_PREFIX  ?= /usr/local/opt/gmp
  MPFR_PREFIX ?= /usr/local/opt/mpfr
  MPC_PREFIX  ?= /usr/local/opt/libmpc
else
  ARCH_RUN  :=
endif

# ── Kernel sources ────────────────────────────────────────────────────────────
C_SOURCES = $(wildcard kernal/*.c drivers/ports/*.c drivers/screen/*.c)
C_OBJS    = $(C_SOURCES:.c=.o)
ASM_ENTRY = kernal/kernal_entry_asm.o   # compiled separately (nasm -f elf32)

CFLAGS = -g -ffreestanding -m32

# ── Output files ──────────────────────────────────────────────────────────────
BOOTSECT = bootsector/bootsector.bin
KERNEL   = kernal/kernal.bin
KERNEL_ELF = kernal/kernal.elf
OS_IMAGE = os-image.bin

BOOT_DEPS = bootsector/bootsector_print.asm \
            bootsector/bootsector_print_hex.asm \
            bootsector/bootsector_disc.asm \
            bootsector/bootsector_32_bit_gdt.asm \
            bootsector/bootsector_32_bit_switch.asm \
            bootsector/bootsector_32_bit_print.asm

# ── Cross-compiler versions ───────────────────────────────────────────────────
GCC_VERSION      = 4.9.1
GCC_TAR          = gcc-$(GCC_VERSION).tar.bz2
GCC_URL          = https://ftp.gnu.org/gnu/gcc/gcc-$(GCC_VERSION)/$(GCC_TAR)
GCC_MARKER       = $(PREFIX)/bin/$(TARGET)-gcc

BINUTILS_VERSION = 2.24
BINUTILS_TAR     = binutils-$(BINUTILS_VERSION).tar.gz
BINUTILS_URL     = https://ftp.gnu.org/gnu/binutils/$(BINUTILS_TAR)
BINUTILS_MARKER  = $(PREFIX)/bin/$(TARGET)-ld

GMP_PREFIX  ?= /opt/homebrew/opt/gmp
MPFR_PREFIX ?= /opt/homebrew/opt/mpfr
MPC_PREFIX  ?= /opt/homebrew/opt/libmpc


# ── Default target ────────────────────────────────────────────────────────────
.PHONY: all run debug clean cross-compiler binutils gcc

all: $(OS_IMAGE)

# Concatenate bootsector + kernel into one bootable image
$(OS_IMAGE): $(BOOTSECT) $(KERNEL)
	cat $^ > $@

# Bootsector binary
$(BOOTSECT): bootsector/bootsector_main.asm $(BOOT_DEPS)
	$(ASM) -f bin -I bootsector/ $< -o $@

# Kernel binary (flat binary for loading at 0x1000)
$(KERNEL): $(ASM_ENTRY) $(C_OBJS)
	$(LD) -o $@ -Ttext 0x1000 --entry=0x1000 $^ --oformat binary

# Kernel ELF (keeps debug symbols, used by gdb)
$(KERNEL_ELF): $(ASM_ENTRY) $(C_OBJS)
	$(LD) -o $@ -Ttext 0x1000 --entry=0x1000 $^

# Compile ASM kernel entry with ELF output (not bin)
$(ASM_ENTRY): kernal/kernal_entry.asm
	$(ASM) $< -f elf32 -o $@

# Compile C sources with cross-compiler
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# ── Run & Debug ───────────────────────────────────────────────────────────────
run: $(OS_IMAGE)
	$(QEMU) -drive format=raw,file=$(OS_IMAGE),if=floppy

debug: $(OS_IMAGE) $(KERNEL_ELF)
	$(QEMU) -s -drive format=raw,file=$(OS_IMAGE),if=floppy &
	sleep 1
	$(GDB) -ex "target remote localhost:1234" \
	       -ex "set architecture i386" \
	       -ex "symbol-file $(KERNEL_ELF)"

# ── Clean ─────────────────────────────────────────────────────────────────────
clean:
	rm -f $(OS_IMAGE) $(BOOTSECT) $(KERNEL) $(KERNEL_ELF) $(ASM_ENTRY) $(C_OBJS) kernal/*.o

# ── Cross-compiler: binutils → GCC ───────────────────────────────────────────
cross-compiler: binutils gcc

binutils:
	@if [ -f "$(BINUTILS_MARKER)" ]; then \
		echo "binutils already installed, skipping."; \
	else \
		mkdir -p /tmp/src && cd /tmp/src && \
		([ -f "$(BINUTILS_TAR)" ] \
			&& echo "binutils archive exists, skipping download." \
			|| (command -v aria2c > /dev/null \
				&& aria2c -x 16 -s 16 $(BINUTILS_URL) \
				|| curl -L -o "$(BINUTILS_TAR)" $(BINUTILS_URL))) && \
		([ -f "binutils-$(BINUTILS_VERSION)/configure" ] \
			&& echo "binutils source already extracted, skipping." \
			|| tar xzf "$(BINUTILS_TAR)") && \
		mkdir -p binutils-build && cd binutils-build && \
		$(ARCH_RUN) ../binutils-$(BINUTILS_VERSION)/configure \
			--target=$(TARGET) \
			--prefix="$(PREFIX)" \
			--with-sysroot \
			--disable-nls \
			--disable-werror && \
		$(ARCH_RUN) make -j$(NPROC) && \
		$(ARCH_RUN) make install && \
		echo "binutils installed."; \
	fi

gcc:
	@if [ -z "$(GAWK)" ]; then \
		echo "ERROR: gawk is required. Run: brew install gawk"; exit 1; \
	fi
	@if [ -f "$(GCC_MARKER)" ]; then \
		echo "GCC cross-compiler already installed, skipping."; \
	else \
		mkdir -p /tmp/src && cd /tmp/src && \
		([ -f "$(GCC_TAR)" ] && tar tjf "$(GCC_TAR)" >/dev/null 2>&1 \
			&& echo "GCC archive valid, skipping download." \
			|| (rm -f "$(GCC_TAR)" && \
				(command -v aria2c > /dev/null \
					&& aria2c -x 16 -s 16 $(GCC_URL) \
					|| curl -L -o "$(GCC_TAR)" $(GCC_URL)))) && \
		([ -f "gcc-$(GCC_VERSION)/configure" ] \
			&& echo "GCC source already extracted, skipping." \
			|| tar xjf "$(GCC_TAR)") && \
		rm -rf gcc-build && mkdir -p gcc-build && cd gcc-build && \
		$(ARCH_RUN) ../gcc-$(GCC_VERSION)/configure \
			--target=$(TARGET) \
			--prefix="$(PREFIX)" \
			--disable-nls \
			--disable-libssp \
			--disable-lto \
			--enable-languages=c \
			--without-headers \
			--with-system-zlib \
			--with-gmp="$(GMP_PREFIX)" \
			--with-mpfr="$(MPFR_PREFIX)" \
			--with-mpc="$(MPC_PREFIX)" && \
		$(ARCH_RUN) make -j$(NPROC) all-gcc \
			AWK=$(GAWK) \
			CFLAGS="-w -Wno-error" \
			CXXFLAGS="-std=gnu++98 -w -Wno-error" && \
		$(ARCH_RUN) make -j$(NPROC) all-target-libgcc \
			AWK=$(GAWK) \
			CFLAGS="-w -Wno-error" \
			CXXFLAGS="-std=gnu++98 -w -Wno-error" && \
		$(ARCH_RUN) make install-gcc && \
		$(ARCH_RUN) make install-target-libgcc && \
		echo "GCC cross-compiler installed successfully."; \
	fi
