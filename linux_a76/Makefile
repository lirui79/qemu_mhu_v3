ROOT := $(CURDIR)
BUILD := $(ROOT)/build
HOST_LINUX_DRIVER := $(ROOT)/../runtime_demo
BIN_DIR := $(ROOT)/bins
BOOT_BUILD := $(BUILD)/boot
KERNEL_SRC ?= $(ROOT)/../linux-6.8
KERNEL_BUILD ?= $(BUILD)/kernel
KERNEL_IMAGE ?= $(KERNEL_BUILD)/arch/arm64/boot/Image
CROSS_COMPILE ?= aarch64-linux-gnu-
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy
DTC ?= dtc

ENC ?= hevc
DEC ?= g2dec
ENC_ENV ?= arm64_linux
DEC_ENV ?= arm_pclinux
TRACE ?= n
USE_MODEL_SIMULATION ?= n
USE_VCMD ?= n
STATIC ?= y

BOOT_BIN := $(BOOT_BUILD)/boot.bin
DTB := $(BUILD)/cortex-r52-a76-vp.dtb
INIT := $(BUILD)/init
ROOTFS := $(ROOT)/rootfs.tar.gz

LAUNCHER := $(BIN_DIR)/launch_kernel
LAUNCHER_IMAGE ?= $(LAUNCHER)
MODULE := $(HOST_LINUX_DRIVER)/driver/r52_gpu.ko
INITRAMFS := $(BUILD)/initramfs.cpio
PAYLOAD := $(BUILD)/linux_payload.bin
INITRAMFS_OVERLAY ?=
INIT_AUTORUN ?= 0

VPU_DRIVER_DIR := $(HOST_LINUX_DRIVER)/driver_vpu/vcodec
MEM_DRIVER_DIR := $(HOST_LINUX_DRIVER)/driver_vpu/memalloc
VC9000E_USERSPACE_DIR := $(HOST_LINUX_DRIVER)/userspace_vpu/VC9000E
ENC_USERSPACE_DIR := $(VC9000E_USERSPACE_DIR)/software
DEC_USERSPACE_DIR := $(HOST_LINUX_DRIVER)/userspace_vpu/VC9000D

H264_LAUNCHER := $(BIN_DIR)/h264_testenc
HEVC_LAUNCHER := $(BIN_DIR)/hevc_testenc
DEC_LAUNCHER := $(BIN_DIR)/g2dec

VPU_MODULE := $(VPU_DRIVER_DIR)/vcodec.ko
MEM_MODULE := $(MEM_DRIVER_DIR)/memalloc.ko

.PHONY: all tools kernel-config kernel-image driver initramfs payload clean help

all: payload

help:
	@echo "Build host-independent pieces: make tools"
	@echo "Configure kernel:              make KERNEL_SRC=/path/to/linux kernel-config"
	@echo "Build complete payload:        make KERNEL_SRC=/path/to/linux all"
	@echo "Run after R52 build:           ../simulation/run_linux.sh"

tools: $(BOOT_BIN) $(DTB) $(INIT) $(LAUNCHER) $(H264_LAUNCHER) $(HEVC_LAUNCHER) $(DEC_LAUNCHER)

$(BOOT_BUILD)/boot.o: boot/boot.S | $(BOOT_BUILD)
	$(CC) -c -march=armv8-a -ffreestanding -nostdlib $< -o $@

$(BOOT_BUILD)/boot.elf: $(BOOT_BUILD)/boot.o boot/boot.ld
	$(LD) -T boot/boot.ld -o $@ $<

$(BOOT_BIN): $(BOOT_BUILD)/boot.elf
	$(OBJCOPY) -O binary $< $@

$(DTB): dts/cortex-r52-a76-vp.dts | $(BUILD)
	$(DTC) -I dts -O dtb -o $@ $<

$(INIT): initramfs/init.c | $(BUILD)
	$(CC) -O2 -static -Wall -Wextra -Werror $< -o $@

$(LAUNCHER): $(HOST_LINUX_DRIVER)/userspace/launch_kernel.c \
		$(HOST_LINUX_DRIVER)/userspace/r52_runtime.c \
		$(HOST_LINUX_DRIVER)/userspace/r52_runtime.h \
		$(HOST_LINUX_DRIVER)/driver/r52_gpu_uapi.h | $(BIN_DIR)
	$(CC) -O2 -static -Wall -Wextra -Werror \
		$(HOST_LINUX_DRIVER)/userspace/launch_kernel.c \
		$(HOST_LINUX_DRIVER)/userspace/r52_runtime.c -o $@

$(H264_LAUNCHER): | $(BIN_DIR)
	$(MAKE) -C $(ENC_USERSPACE_DIR) \
		CROSS_COMPILE=$(CROSS_COMPILE) \
		CC=$(CC) \
		BUILD_OUT=$(BIN_DIR) \
		h264 ENV=$(ENC_ENV) TRACE=$(TRACE) STATIC=$(STATIC)
		cp -rf  $(VC9000E_USERSPACE_DIR)/build/$(ENC_ENV)/h264_testenc $(H264_LAUNCHER)

$(HEVC_LAUNCHER): | $(BIN_DIR)
	$(MAKE) -C $(ENC_USERSPACE_DIR) \
		CROSS_COMPILE=$(CROSS_COMPILE) \
		CC=$(CC) \
		BUILD_OUT=$(BIN_DIR) \
		hevc ENV=$(ENC_ENV) TRACE=$(TRACE) STATIC=$(STATIC)
		cp -rf   $(VC9000E_USERSPACE_DIR)/build/$(ENC_ENV)/hevc_testenc $(HEVC_LAUNCHER)

# ---------------- NEW: g2dec 顶层目标，调用decoder子Makefile ----------------
$(DEC_LAUNCHER): | $(BIN_DIR)
	$(MAKE) -C $(DEC_USERSPACE_DIR) \
		CROSS_COMPILE=$(CROSS_COMPILE) \
		CC=$(CC) \
		BUILD_OUT=$(BIN_DIR) \
		g2dec ENV=$(DEC_ENV) TRACE=$(TRACE) USE_MODEL_SIMULATION=$(USE_MODEL_SIMULATION) USE_VCMD=$(USE_VCMD) STATIC=$(STATIC)
		cp -rf $(DEC_USERSPACE_DIR)/build/$(DEC_ENV)/debug/g2dec $(DEC_LAUNCHER)
		@if [ "$(STATIC)" = "y" ]; then \
			echo ">>> static link mode"; \
		else \
			echo ">>> dynamic link mode"; \
		    cp -rf $(DEC_USERSPACE_DIR)/build/$(DEC_ENV)/debug/*.so $(BIN_DIR); \
		fi


kernel-config:
	@test -n "$(KERNEL_SRC)" -a -f "$(KERNEL_SRC)/Makefile" || { \
		echo "error: set KERNEL_SRC to an arm64 Linux source tree"; exit 1; }
	$(MAKE) -C $(KERNEL_SRC) O=$(KERNEL_BUILD) ARCH=arm64 \
		CROSS_COMPILE=$(CROSS_COMPILE) defconfig
	$(KERNEL_SRC)/scripts/kconfig/merge_config.sh -m -O $(KERNEL_BUILD) \
		$(KERNEL_BUILD)/.config $(ROOT)/kernel.fragment
	$(MAKE) -C $(KERNEL_SRC) O=$(KERNEL_BUILD) ARCH=arm64 \
		CROSS_COMPILE=$(CROSS_COMPILE) olddefconfig

kernel-image:
	@test -n "$(KERNEL_SRC)" -a -f "$(KERNEL_SRC)/Makefile" || { \
		echo "error: set KERNEL_SRC to an arm64 Linux source tree"; exit 1; }
	$(MAKE) -C $(KERNEL_SRC) O=$(KERNEL_BUILD) ARCH=arm64 \
		CROSS_COMPILE=$(CROSS_COMPILE) -j$$(nproc) Image modules

driver:
	@test -f "$(KERNEL_BUILD)/Makefile" || { \
		echo "error: set KERNEL_BUILD to the configured arm64 kernel build tree"; exit 1; }
	$(MAKE) -C $(KERNEL_BUILD) M=$(HOST_LINUX_DRIVER)/driver ARCH=arm64 \
		CROSS_COMPILE=$(CROSS_COMPILE) W=1 modules

mem_driver:
	@test -n "$(KERNEL_BUILD)" -a -f "$(KERNEL_BUILD)/Makefile" || { \
		echo "error: set KERNEL_BUILD to configured arm64 kernel build tree"; exit 1; }
	$(MAKE) -C $(KERNEL_BUILD) M=$(MEM_DRIVER_DIR) ARCH=arm64 \
		CROSS_COMPILE=$(CROSS_COMPILE) W=1 modules

vpu_driver:
	@test -n "$(KERNEL_BUILD)" -a -f "$(KERNEL_BUILD)/Makefile" || { \
		echo "error: set KERNEL_BUILD to configured arm64 kernel build tree"; exit 1; }
	$(MAKE) -C $(KERNEL_BUILD) M=$(VPU_DRIVER_DIR) ARCH=arm64 \
		CROSS_COMPILE=$(CROSS_COMPILE) W=1 modules

initramfs: tools kernel-image driver mem_driver vpu_driver $(INITRAMFS)

$(INITRAMFS): $(INIT) $(LAUNCHER_IMAGE) $(MODULE) scripts/mkinitramfs.sh
	@if test -n "$(ROOTFS)" -a -f $(ROOTFS); then \
		echo "initramfs using rootfs: $(ROOTFS)"; \
		./scripts/mkinitramfs.sh $(ROOTFS) $(LAUNCHER_IMAGE) $(MODULE) $@ \
			"$(INITRAMFS_OVERLAY)" "$(INIT_AUTORUN)"; \
	else \
		echo "initramfs using init: $(INIT)"; \
		./scripts/mkinitramfs.sh $(INIT) $(LAUNCHER_IMAGE) $(MODULE) $@ \
			"$(INITRAMFS_OVERLAY)" "$(INIT_AUTORUN)"; \
	fi

payload: tools initramfs
	@test -f "$(KERNEL_IMAGE)" || { \
		echo "error: Linux Image not found: $(KERNEL_IMAGE)"; epayloadxit 1; }
	./scripts/mkpayload.sh $(BOOT_BIN) $(DTB) $(KERNEL_IMAGE) \
		$(INITRAMFS) $(PAYLOAD)
	@echo "payload ready: $(PAYLOAD)"

vpu:  tools driver mem_driver vpu_driver $(INITRAMFS)
	@test -f "$(KERNEL_IMAGE)" || { \
		echo "error: Linux Image not found: $(KERNEL_IMAGE)"; epayloadxit 1; }
	./scripts/mkpayload.sh $(BOOT_BIN) $(DTB) $(KERNEL_IMAGE) \
		$(INITRAMFS) $(PAYLOAD)
	@echo "payload ready: $(PAYLOAD)"

$(BUILD) $(BOOT_BUILD) $(BIN_DIR):
	mkdir -p $@

clean:
	@if test -f "$(KERNEL_BUILD)/Makefile"; then \
		$(MAKE) -C $(KERNEL_BUILD) M=$(HOST_LINUX_DRIVER)/driver clean; \
		$(MAKE) -C $(KERNEL_BUILD) M=$(MEM_DRIVER_DIR) clean; \
		$(MAKE) -C $(KERNEL_BUILD) M=$(VPU_DRIVER_DIR) clean; \
	fi
	$(MAKE) -C $(ENC_USERSPACE_DIR) clean
	$(MAKE) -C $(DEC_USERSPACE_DIR) clean
	$(RM) -rf $(BUILD) $(BIN_DIR)
