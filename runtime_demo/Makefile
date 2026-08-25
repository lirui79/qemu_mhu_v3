ROOT := $(CURDIR)
KERNEL_BUILD ?=
CROSS_COMPILE ?= aarch64-linux-gnu-
CC := $(CROSS_COMPILE)gcc

DRIVER_DIR := $(ROOT)/driver
USERSPACE_DIR := $(ROOT)/userspace
BUILD := $(ROOT)/build
LAUNCHER := $(BUILD)/launch_kernel
MODULE := $(DRIVER_DIR)/r52_gpu.ko

.PHONY: all driver userspace clean help

all: driver userspace

help:
	@echo "Build module + runtime:"
	@echo "  make KERNEL_BUILD=/path/to/arm64/kernel/build"
	@echo "Build userspace only:"
	@echo "  make userspace"

driver:
	@test -n "$(KERNEL_BUILD)" -a -f "$(KERNEL_BUILD)/Makefile" || { \
		echo "error: set KERNEL_BUILD to configured arm64 kernel build tree"; exit 1; }
	$(MAKE) -C $(KERNEL_BUILD) M=$(DRIVER_DIR) ARCH=arm64 \
		CROSS_COMPILE=$(CROSS_COMPILE) W=1 modules

userspace: $(LAUNCHER)

$(LAUNCHER): $(USERSPACE_DIR)/launch_kernel.c \
		$(USERSPACE_DIR)/r52_runtime.c \
		$(USERSPACE_DIR)/r52_runtime.h \
		$(DRIVER_DIR)/r52_gpu_uapi.h | $(BUILD)
	$(CC) -O2 -static -Wall -Wextra -Werror \
		$(USERSPACE_DIR)/launch_kernel.c \
		$(USERSPACE_DIR)/r52_runtime.c -o $@

$(BUILD):
	mkdir -p $@

clean:
	@if test -n "$(KERNEL_BUILD)" -a -f "$(KERNEL_BUILD)/Makefile"; then \
		$(MAKE) -C $(KERNEL_BUILD) M=$(DRIVER_DIR) clean; \
	fi
	$(RM) -r $(BUILD)
