ROOT := $(CURDIR)
KERNEL_BUILD ?=
CROSS_COMPILE ?= aarch64-linux-gnu-
CC := $(CROSS_COMPILE)gcc

# 编译选项，外部可传入 ENV=xxx TRACE=y/n
ENC ?= hevc
DEC ?= g2dec
ENC_ENV ?= arm64_linux
DEC_ENV ?= arm_pclinux
TRACE ?= n
USE_MODEL_SIMULATION ?= n
USE_VCMD ?= n
STATIC ?= y

BUILD := $(ROOT)/build
GPU_DRIVER_DIR := $(ROOT)/driver
GPU_USERSPACE_DIR := $(ROOT)/userspace
GPU_LAUNCHER := $(BUILD)/launch_kernel
GPU_MODULE := $(GPU_DRIVER_DIR)/r52_gpu.ko

VPU_DRIVER_DIR := $(ROOT)/driver_vpu/vcodec
MEM_DRIVER_DIR := $(ROOT)/driver_vpu/memalloc
VC9000E_USERSPACE_DIR := $(ROOT)/userspace_vpu/VC9000E
ENC_USERSPACE_DIR := $(VC9000E_USERSPACE_DIR)/software
DEC_USERSPACE_DIR := $(ROOT)/userspace_vpu/VC9000D
H264_LAUNCHER := $(BUILD)/h264_testenc
HEVC_LAUNCHER := $(BUILD)/hevc_testenc
DEC_LAUNCHER := $(BUILD)/g2dec
VPU_MODULE := $(VPU_DRIVER_DIR)/vcodec.ko
MEM_MODULE := $(MEM_DRIVER_DIR)/memalloc.ko

.PHONY: all gpu_driver gpu_userspace mem_driver vpu_driver vpu_userspace clean help

all: gpu_driver gpu_userspace mem_driver vpu_driver vpu_userspace

help:
	@echo "Build module + runtime:"
	@echo "  make KERNEL_BUILD=/path/to/arm64/kernel/build"
	@echo "Build gpu_userspace only:"
	@echo "  make gpu_userspace"

gpu_driver:
	@test -n "$(KERNEL_BUILD)" -a -f "$(KERNEL_BUILD)/Makefile" || { \
		echo "error: set KERNEL_BUILD to configured arm64 kernel build tree"; exit 1; }
	$(MAKE) -C $(KERNEL_BUILD) M=$(GPU_DRIVER_DIR) ARCH=arm64 \
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


vpu_userspace: $(H264_LAUNCHER) $(HEVC_LAUNCHER) $(DEC_LAUNCHER)

# ---------------- NEW: hevc / h264 独立目标，仅编译encoder子目录 ----------------
$(H264_LAUNCHER): | $(BUILD)
	$(MAKE) -C $(ENC_USERSPACE_DIR) \
		CROSS_COMPILE=$(CROSS_COMPILE) \
		CC=$(CC) \
		BUILD_OUT=$(BUILD) \
		h264 ENV=$(ENC_ENV) TRACE=$(TRACE) STATIC=$(STATIC)
		cp -rf  $(VC9000E_USERSPACE_DIR)/build/$(ENC_ENV)/h264_testenc $(H264_LAUNCHER)

$(HEVC_LAUNCHER): | $(BUILD)
	$(MAKE) -C $(ENC_USERSPACE_DIR) \
		CROSS_COMPILE=$(CROSS_COMPILE) \
		CC=$(CC) \
		BUILD_OUT=$(BUILD) \
		hevc ENV=$(ENC_ENV) TRACE=$(TRACE) STATIC=$(STATIC)
		cp -rf   $(VC9000E_USERSPACE_DIR)/build/$(ENC_ENV)/hevc_testenc $(HEVC_LAUNCHER)

# ---------------- NEW: g2dec 顶层目标，调用decoder子Makefile ----------------
$(DEC_LAUNCHER): | $(BUILD)
	$(MAKE) -C $(DEC_USERSPACE_DIR) \
		CROSS_COMPILE=$(CROSS_COMPILE) \
		CC=$(CC) \
		BUILD_OUT=$(BUILD) \
		g2dec ENV=$(DEC_ENV) TRACE=$(TRACE) USE_MODEL_SIMULATION=$(USE_MODEL_SIMULATION) USE_VCMD=$(USE_VCMD) STATIC=$(STATIC)
		cp -rf $(DEC_USERSPACE_DIR)/build/$(DEC_ENV)/debug/g2dec $(DEC_LAUNCHER)
		@if [ "$(STATIC)" = "y" ]; then \
			echo ">>> static link mode"; \
		else \
			echo ">>> dynamic link mode"; \
		    cp -rf $(DEC_USERSPACE_DIR)/build/$(DEC_ENV)/debug/*.so $(BUILD); \
		fi


gpu_userspace: $(GPU_LAUNCHER)

$(GPU_LAUNCHER): $(GPU_USERSPACE_DIR)/launch_kernel.c \
		$(GPU_USERSPACE_DIR)/r52_runtime.c \
		$(GPU_USERSPACE_DIR)/r52_runtime.h \
		$(GPU_DRIVER_DIR)/r52_gpu_uapi.h | $(BUILD)
	$(CC) -O2 -static -Wall -Wextra -Werror \
		$(GPU_USERSPACE_DIR)/launch_kernel.c \
		$(GPU_USERSPACE_DIR)/r52_runtime.c -o $@

$(BUILD):
	mkdir -p $@

clean:
	@if test -n "$(KERNEL_BUILD)" -a -f "$(KERNEL_BUILD)/Makefile"; then \
		$(MAKE) -C $(KERNEL_BUILD) M=$(GPU_DRIVER_DIR) clean; \
		$(MAKE) -C $(KERNEL_BUILD) M=$(MEM_DRIVER_DIR) clean; \
		$(MAKE) -C $(KERNEL_BUILD) M=$(VPU_DRIVER_DIR) clean; \
	fi
	$(MAKE) -C $(ENC_USERSPACE_DIR) clean
	$(MAKE) -C $(DEC_USERSPACE_DIR) clean
	$(RM) -r $(BUILD)
