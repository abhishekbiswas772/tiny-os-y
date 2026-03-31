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

# ── Build directory ───────────────────────────────────────────────────────────
BUILD = build

# ── Kernel sources ────────────────────────────────────────────────────────────
C_SOURCES = $(wildcard kernal/*.c kernal/libc/*.c kernal/kfs/*.c \
                        drivers/ports/*.c drivers/screen/*.c drivers/keyboard/*.c \
                        drivers/disk/*.c drivers/mouse/*.c \
                        drivers/net/*.c drivers/vga/*.c drivers/gui/*.c \
                        cpu/idt/*.c cpu/isr/*.c cpu/timer/*.c)
C_OBJS    = $(addprefix $(BUILD)/,$(notdir $(C_SOURCES:.c=.o)))
ASM_ENTRY = $(BUILD)/kernal_entry_asm.o
ASM_INTERRUPT = $(BUILD)/interrupt_asm.o

CFLAGS = -g -ffreestanding -m32

# ── Output files ──────────────────────────────────────────────────────────────
BOOTSECT     = $(BUILD)/bootsector.bin
KERNEL       = $(BUILD)/kernal.bin
KERNEL_ELF   = $(BUILD)/kernal.elf
OS_IMAGE     = os-image.bin
ISO_IMAGE    = os-image.iso
ISO_ROOT     = $(BUILD)/iso_root
ISO_BOOT_IMG = os-image-1.44M.img
MKISOFS_BIN  = $(shell command -v xorriso >/dev/null 2>&1 && echo "xorriso" || command -v mkisofs >/dev/null 2>&1 && echo "mkisofs" || command -v genisoimage >/dev/null 2>&1 && echo "genisoimage")

# ── GRUB / KFS targets ────────────────────────────────────────────────────
KERNEL_GRUB_ELF = $(BUILD)/kernal_grub.elf
ASM_ENTRY_GRUB  = $(BUILD)/kernal_entry_grub.o
GRUB_ISO        = os-grub.iso
GRUB_DIR        = $(BUILD)/grub_iso
DISK_IMG        = disk.img
DISK_SECTORS    = 4200         # LBA 0..4131 for KFS + buffer
GRUB_MKRESCUE  := $(shell command -v i686-elf-grub-mkrescue 2>/dev/null || \
                          command -v grub-mkrescue 2>/dev/null || \
                          command -v grub2-mkrescue 2>/dev/null || echo "")

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
.PHONY: all iso run run-iso debug clean cross-compiler binutils gcc \
        grub run-grub disk

all: $(OS_IMAGE)

iso: $(ISO_IMAGE)

# Concatenate bootsector + kernel into one bootable image
$(OS_IMAGE): $(BOOTSECT) $(KERNEL)
	cat $^ > $@

# Bootable ISO (El Torito, floppy emulation) that embeds a padded raw image
$(ISO_IMAGE): $(OS_IMAGE) | $(BUILD)
	@rm -rf $(ISO_ROOT)
	@mkdir -p $(ISO_ROOT)
	dd if=$(OS_IMAGE) of=$(BUILD)/$(ISO_BOOT_IMG) bs=1474560 conv=sync status=none
	cp $(BUILD)/$(ISO_BOOT_IMG) $(ISO_ROOT)/$(ISO_BOOT_IMG)
	@if [ -z "$(MKISOFS_BIN)" ]; then \
		echo "ERROR: Need xorriso, mkisofs, or genisoimage to create $(ISO_IMAGE)."; \
		echo "Install one (e.g. brew install xorriso) and re-run make."; \
		exit 1; \
	fi
	@case "$(MKISOFS_BIN)" in \
		*xorriso*) $(MKISOFS_BIN) -as mkisofs -o $@ -V KERNAL_OS -b $(ISO_BOOT_IMG) -c boot.cat $(ISO_ROOT) ;; \
		*)         $(MKISOFS_BIN) -o $@ -V KERNAL_OS -b $(ISO_BOOT_IMG) -c boot.cat $(ISO_ROOT) ;; \
	esac

# Bootsector binary
$(BOOTSECT): bootsector/bootsector_main.asm $(BOOT_DEPS) | $(BUILD)
	$(ASM) -f bin -I bootsector/ $< -o $@

# Kernel binary (flat binary for loading at 0x1000)
$(KERNEL): $(ASM_ENTRY) $(ASM_INTERRUPT) $(C_OBJS)
	$(LD) -o $@ -Ttext 0x1000 --entry=0x1000 $^ --oformat binary

# Kernel ELF (keeps debug symbols, used by gdb)
$(KERNEL_ELF): $(ASM_ENTRY) $(ASM_INTERRUPT) $(C_OBJS)
	$(LD) -o $@ -Ttext 0x1000 --entry=0x1000 $^

# Compile ASM kernel entry with ELF output (not bin)
$(ASM_ENTRY): kernal/kernal_entry.asm | $(BUILD)
	$(ASM) $< -f elf32 -o $@

# Compile ISR stubs with ELF output
$(ASM_INTERRUPT): cpu/interrupt.asm | $(BUILD)
	$(ASM) $< -f elf32 -o $@

# Generate a flat compile rule for every C source file
define compile_rule
$(BUILD)/$(notdir $(1:.c=.o)): $(1) | $(BUILD)
	$(CC) $(CFLAGS) -c $$< -o $$@
endef
$(foreach src,$(C_SOURCES),$(eval $(call compile_rule,$(src))))

# Create build directory
$(BUILD):
	mkdir -p $(BUILD)

# ── GRUB ELF (Multiboot2, loads at 0x100000 via linker.ld) ───────────────────
$(KERNEL_GRUB_ELF): $(ASM_ENTRY_GRUB) $(ASM_INTERRUPT) $(C_OBJS)
	$(LD) -T linker.ld -o $@ $^

$(ASM_ENTRY_GRUB): kernal/kernal_entry_grub.asm | $(BUILD)
	$(ASM) $< -f elf32 -o $@

# ── GRUB ISO ──────────────────────────────────────────────────────────────────
grub: $(KERNEL_GRUB_ELF)
	@if [ -z "$(GRUB_MKRESCUE)" ]; then \
		echo "ERROR: grub-mkrescue not found."; \
		echo "  macOS: brew install i686-elf-grub xorriso"; \
		echo "  Linux: sudo apt install grub-pc-bin grub-common xorriso"; \
		exit 1; \
	fi
	@rm -rf $(GRUB_DIR)
	@mkdir -p $(GRUB_DIR)/boot/grub
	cp $(KERNEL_GRUB_ELF) $(GRUB_DIR)/boot/kernal.elf
	cp grub/grub.cfg      $(GRUB_DIR)/boot/grub/grub.cfg
	$(GRUB_MKRESCUE) -o $(GRUB_ISO) $(GRUB_DIR)
	@echo "Built $(GRUB_ISO) — run with: make run-grub"

# ── Blank KFS disk image (~2.1 MB) ───────────────────────────────────────────
disk:
	@if [ -f $(DISK_IMG) ]; then \
		echo "$(DISK_IMG) exists — delete it first for a fresh one."; \
	else \
		dd if=/dev/zero of=$(DISK_IMG) bs=512 count=$(DISK_SECTORS) status=none && \
		echo "Created $(DISK_IMG). Boot and type 'format' to initialise KFS."; \
	fi

# ── Run & Debug ───────────────────────────────────────────────────────────────
run: $(OS_IMAGE)
	$(QEMU) -drive format=raw,file=$(OS_IMAGE),if=floppy

run-iso: $(ISO_IMAGE)
	$(QEMU) -cdrom $(ISO_IMAGE) -boot d

# Boot GRUB ISO + attach KFS disk as primary ATA drive
run-grub: $(GRUB_ISO)
	@if [ ! -f $(DISK_IMG) ]; then \
		dd if=/dev/zero of=$(DISK_IMG) bs=512 count=$(DISK_SECTORS) status=none; \
	fi
	$(QEMU) -cdrom $(GRUB_ISO) -boot d \
	        -drive format=raw,file=$(DISK_IMG),if=ide,index=0 \
	        -netdev user,id=n0 -device ne2k_isa,netdev=n0,irq=9,iobase=0x300

debug: $(OS_IMAGE) $(KERNEL_ELF)
	$(QEMU) -s -drive format=raw,file=$(OS_IMAGE),if=floppy &
	sleep 1
	$(GDB) -ex "target remote localhost:1234" \
	       -ex "set architecture i386" \
	       -ex "symbol-file $(KERNEL_ELF)"

# ── Clean ─────────────────────────────────────────────────────────────────────
clean:
	rm -rf $(BUILD) $(OS_IMAGE) $(ISO_IMAGE) $(GRUB_ISO)

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
