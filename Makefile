CROSS ?= $(shell if command -v aarch64-elf-gcc >/dev/null 2>&1; then echo aarch64-elf-; \
    elif command -v aarch64-linux-gnu-gcc >/dev/null 2>&1; then echo aarch64-linux-gnu-; fi)
ifeq ($(CROSS),)
$(error Set CROSS to your AArch64 cross toolchain prefix)
endif

CC := $(CROSS)gcc
AR := $(CROSS)ar
LD := $(CROSS)ld
OBJCOPY := $(CROSS)objcopy

CFLAGS := -march=armv8-a -ffreestanding -fno-stack-protector -fno-pic -Wall -Wextra -std=gnu11 -O2
CFLAGS += -Iinclude
ASFLAGS := $(CFLAGS)
LDFLAGS := -nostdlib -static -T linker.ld

BUILD := build
TARGET_ELF := $(BUILD)/kernel.elf
TARGET_BIN := $(BUILD)/kernel8.img

TAP_IFACE ?= qemu-lan
NET_MAC ?= 02:00:00:00:00:01
NETDEV_ID ?= network-lan

C_SRCS := $(shell find src -name '*.c')
S_SRCS := $(shell find src -name '*.S')
OBJS := $(C_SRCS:%.c=$(BUILD)/%.o) $(S_SRCS:%.S=$(BUILD)/%.o)

all: $(TARGET_BIN)

$(BUILD)/%.o: %.c | $(BUILD)/src
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.S | $(BUILD)/src
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

$(TARGET_ELF): linker.ld $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

$(TARGET_BIN): $(TARGET_ELF)
	$(OBJCOPY) -O binary $< $@

run: $(TARGET_BIN)
	qemu-system-aarch64 -M virt -cpu cortex-a53 -display none \
	    -kernel $(TARGET_BIN) \
	    -monitor none \
	    -serial none \
	    -chardev stdio,id=virtiocon0,signal=off \
	    -device virtio-serial-device,id=virtio-serial0 \
	    -device virtconsole,chardev=virtiocon0,id=virtio-console0,bus=virtio-serial0.0 \
	    -netdev tap,id=$(NETDEV_ID),ifname=$(TAP_IFACE),script=no,downscript=no \
	    -device virtio-net-device,netdev=$(NETDEV_ID),mac=$(NET_MAC)

clean:
	rm -rf $(BUILD)

$(BUILD)/src:
	@mkdir -p $(BUILD)/src

.PHONY: all clean run
