AS = nasm
CC = i386-elf-gcc
LD = i386-elf-ld
OBJCOPY = objcopy
HOSTCC = gcc

CFLAGS = -march=i386 -m32 -ffreestanding -fno-pie -fno-stack-protector -fno-builtin -Wall -Wextra -g
LDFLAGS = -m elf_i386 -T linker.ld

SRC_DIR = src
BUILD_DIR = build
IMG = loaf.img

MBR_BIN = $(BUILD_DIR)/mbr.bin
LOAD_BIN = $(BUILD_DIR)/load.bin
KERNEL_ELF = $(BUILD_DIR)/kernel.elf
KERNEL_BIN = $(BUILD_DIR)/kernel.bin
ENTRY_OBJ = $(BUILD_DIR)/entry.o
KSIZE_INC = $(BUILD_DIR)/kernel_size.inc

C_EXCLUDE = $(shell find $(SRC_DIR)/init -name '*.c')
C_SRCS = $(filter-out $(C_EXCLUDE), $(shell find $(SRC_DIR) -name '*.c'))
C_OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(C_SRCS))

ASM_EXCLUDE = $(SRC_DIR)/mbr.asm $(SRC_DIR)/load.asm $(SRC_DIR)/entry.asm
ASM_SRCS = $(filter-out $(ASM_EXCLUDE),$(shell find $(SRC_DIR) -name '*.asm'))
ASM_OBJS = $(patsubst $(SRC_DIR)/%.asm,$(BUILD_DIR)/%.o,$(ASM_SRCS))

KERNEL_OBJS = $(ENTRY_OBJ) $(C_OBJS) $(ASM_OBJS)

MKEXE = $(BUILD_DIR)/mkexe
INIT_C = $(SRC_DIR)/init/init.c
INIT_ELF = $(BUILD_DIR)/init.elf
INIT_FLAT = $(BUILD_DIR)/init_flat.bin
INIT_BIN = $(BUILD_DIR)/init.bin
INIT_LD = init.ld
CODE_VADDR = 0x00400000

.PHONY: all clean run

all: $(IMG)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(MBR_BIN): $(SRC_DIR)/mbr.asm | $(BUILD_DIR)
	$(AS) $< -f bin -o $@

$(LOAD_BIN): $(SRC_DIR)/load.asm | $(BUILD_DIR)
	$(AS) -I $(BUILD_DIR)/ $< -f bin -o $@

$(ENTRY_OBJ): $(SRC_DIR)/entry.asm | $(BUILD_DIR)
	$(AS) -f elf32 $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.asm
	@mkdir -p $(dir $@)
	$(AS) -f elf32 $< -o $@

$(KERNEL_ELF): $(KERNEL_OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(KERNEL_OBJS)

$(KERNEL_BIN): $(KERNEL_ELF)
	$(OBJCOPY) -O binary $< $@

$(MKEXE): tools/mkexe.c | $(BUILD_DIR)
	$(HOSTCC) -o $@ $<

$(BUILD_DIR)/init/%.o: $(SRC_DIR)/init/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(INIT_ELF): $(BUILD_DIR)/init/init.o $(INIT_LD)
	$(LD) -m elf_i386 -T $(INIT_LD) -o $@ $<

$(INIT_FLAT): $(INIT_ELF)
	$(OBJCOPY) -O binary $< $@

$(INIT_BIN): $(INIT_FLAT) $(MKEXE)
	$(MKEXE) $(INIT_FLAT) $(CODE_VADDR) $@

$(IMG): $(MBR_BIN) $(LOAD_BIN) $(KERNEL_BIN) hello.txt $(INIT_BIN)
	dd if=/dev/zero of=$(IMG) bs=512 count=2880
	mkfs.fat -F 12 -R 8 -n LOAFOS $(IMG)
	mcopy -i $(IMG) $(KERNEL_BIN) ::/KERNEL.BIN
	mcopy -i $(IMG) hello.txt ::/HELLO.TXT
	mcopy -i $(IMG) $(INIT_BIN) ::/INIT.BIN
	dd if=$(MBR_BIN)  of=$(IMG) bs=1 count=3 seek=0 conv=notrunc
	dd if=$(MBR_BIN)  of=$(IMG) bs=1 skip=62 seek=62 count=448 conv=notrunc
	dd if=$(LOAD_BIN) of=$(IMG) bs=512 seek=1 conv=notrunc

run: $(IMG)
	qemu-system-i386 -hda $(IMG) \
		-m 16M \
		-rtc base=localtime \
		-d int,cpu_reset -no-reboot -no-shutdown -D qemu.log

clean:
	rm -rf $(BUILD_DIR) $(IMG)