CC := gcc
LD := ld
NASM := nasm
OBJCOPY := objcopy

CFLAGS := -m32 -ffreestanding -fno-stack-protector -fno-pic -Wall -Wextra -std=gnu11
CFLAGS += -Iinclude
LDFLAGS := -m elf_i386 -T linker.ld -nostdlib
NASMFLAGS := -f elf32

BUILD := build
ISO_DIR := $(BUILD)/isodir
TARGET := $(BUILD)/kernel.elf
ISO := $(BUILD)/ucosii.iso
TAP_IFACE ?= qemu-lan
NET_MAC ?= 02:00:00:00:00:01
NETDEV_ID ?= network-lan

C_SRCS := $(shell find src -name '*.c')
ASM_SRCS := $(shell find src -name '*.asm')
OBJS := $(C_SRCS:%.c=$(BUILD)/%.o) $(ASM_SRCS:%.asm=$(BUILD)/%.o)

all: $(ISO)

$(BUILD)/%.o: %.c | $(BUILD)/src
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.asm | $(BUILD)/src
	@mkdir -p $(dir $@)
	$(NASM) $(NASMFLAGS) $< -o $@

$(TARGET): linker.ld $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

$(ISO): $(TARGET) grub.cfg
	@mkdir -p $(ISO_DIR)/boot/grub
	cp $(TARGET) $(ISO_DIR)/boot/kernel.elf
	cp grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $@ $(ISO_DIR) >/dev/null 2>&1

run: $(ISO)
	qemu-system-i386 -cdrom $(ISO) -serial stdio -no-reboot -no-shutdown -display none \
	    -netdev tap,id=$(NETDEV_ID),ifname=$(TAP_IFACE),script=no,downscript=no \
	    -device virtio-net-pci,disable-modern=on,netdev=$(NETDEV_ID),mac=$(NET_MAC)

clean:
	rm -rf $(BUILD)

$(BUILD)/src:
	@mkdir -p $(BUILD)/src

.PHONY: all clean run
