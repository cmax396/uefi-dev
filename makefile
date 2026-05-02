SRC_DIR    := src
KERNEL_DIR := kernel/src
INC_DIR    := include
BUILD_DIR  := build
MNT_LINK   := mnt

# UEFI application
SOURCES := $(wildcard $(SRC_DIR)/*.c)
OBJS    := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))
DEPENDS := $(OBJS:.o=.d)
TARGET  := $(BUILD_DIR)/BOOTX64.EFI

# Kernel
KERNEL_SRCS    := $(wildcard $(KERNEL_DIR)/*.c)
KERNEL_OBJS    := $(patsubst $(KERNEL_DIR)/%.c,$(BUILD_DIR)/kernel/%.o,$(KERNEL_SRCS))
KERNEL_DEPENDS := $(KERNEL_OBJS:.o=.d)
KERNEL_ELF     := $(BUILD_DIR)/kernel.elf

IMG          := $(BUILD_DIR)/OS.img
TMP_PART      = /tmp/part.img
FIRMWARE_BIN  = bios64.bin
QEMU_LOG      = qemu.log

CC      = clang -target x86_64-unknown-windows
LDFLAGS = \
	-nostdlib \
	-target x86_64-unknown-windows \
	-fuse-ld=lld-link \
	-Wl,-subsystem:efi_application \
	-Wl,-entry:UefiEntry

CFLAGS = \
	-std=c17 \
	-Wall -Wextra -Wpedantic \
	-mno-red-zone \
	-MMD \
	-ffreestanding \
	-I$(INC_DIR)

KCC      = clang
KCFLAGS  = \
	-ffreestanding -fno-pie -fno-pic -fno-stack-protector \
	-mno-red-zone -nostdlib -O2 -Wall -Wextra \
	-MMD \
	-I$(INC_DIR)

KLD      = ld.lld
KLDFLAGS = -T kernel/linker.ld --no-dynamic-linker -z noexecstack

.PHONY: run clean image mount unmount remount

run: $(IMG)
	@echo "Running QEMU with $<..."
	qemu-system-x86_64 \
		-drive format=raw,file=$< \
		-bios $(FIRMWARE_BIN) \
		-m 256M \
		-machine q35 \
		-net none \
		-serial file:$(QEMU_LOG)

image: $(IMG)

$(IMG): $(TARGET) $(KERNEL_ELF)
	@echo "Creating image $@..."
	dd if=/dev/zero of=$@ bs=512 count=93750
	parted $@ -s -a minimal mklabel gpt
	parted $@ -s -a minimal mkpart EFI FAT32 2048s 93716s
	parted $@ -s -a minimal toggle 1 boot
	dd if=/dev/zero of=$(TMP_PART) bs=512 count=91669
	mformat -i $(TMP_PART) -h 32 -t 32 -n 64 -c 1
	mmd -i $(TMP_PART) ::/EFI
	mmd -i $(TMP_PART) ::/EFI/BOOT
	mcopy -i $(TMP_PART) $< ::/EFI/BOOT/$(notdir $<)
	mcopy -i $(TMP_PART) $(KERNEL_ELF) ::kernel.elf
	dd if=$(TMP_PART) of=$@ bs=512 count=91669 seek=2048 conv=notrunc

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $^ -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL_ELF): $(KERNEL_OBJS) kernel/linker.ld
	$(KLD) $(KLDFLAGS) -o $@ $(KERNEL_OBJS)

$(BUILD_DIR)/kernel/%.o: $(KERNEL_DIR)/%.c | $(BUILD_DIR)/kernel
	$(KCC) $(KCFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/kernel: | $(BUILD_DIR)
	mkdir -p $@

-include $(DEPENDS)
-include $(KERNEL_DEPENDS)

mount: $(IMG)
	./mount.sh $(IMG) $(MNT_LINK)

unmount:
	./unmount.sh $(IMG) && rm -f $(MNT_LINK)

remount: unmount mount

clean: unmount
	rm -rf *.log *.tmp $(BUILD_DIR)
