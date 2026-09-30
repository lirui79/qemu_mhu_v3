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

# VC9000SDK(vmpp)：直接调用 userspace_vpu/VC9000SDK/autobuild.sh
VPU_SDK_DIR := $(ROOT)/userspace_vpu/VC9000SDK
VPU_SDK_AUTOBUILD := $(VPU_SDK_DIR)/autobuild.sh
# -t: 0:dec+enc  1:dec  2:enc
SDK_TARGET ?= 0
# -r: 0:debug  1:release
SDK_RELEASE ?= 0
# -s: 0:off  1:ASan  2:TSan
SDK_SANITIZER ?= 0
# -d: dynamic resolution
SDK_DYNRES ?= 0
# -a: 1 表示按 arm64 交叉编译
SDK_ARM64 ?= 1
# -U: unit test
SDK_UT ?= 0
# -F: rebuild ffmpeg for samples
SDK_REBUILD_FFMPEG ?= 0
# -S: 1 产出自包含 libvmpp-*.a，且 sample 静态链接
SDK_STATIC ?= 1

SDK_FLAGS := -t $(SDK_TARGET) -r $(SDK_RELEASE) -s $(SDK_SANITIZER) \
             -d $(SDK_DYNRES) -a $(SDK_ARM64) -U $(SDK_UT) \
             -F $(SDK_REBUILD_FFMPEG) -S $(SDK_STATIC)

# autobuild.sh 失败时是裸 exit（退出码可能为 0），故在 make 层复核产物
ifeq ($(strip $(SDK_TARGET)),1)
SDK_EXPECT_LIBS := libvmpp-dec.so
else ifeq ($(strip $(SDK_TARGET)),2)
SDK_EXPECT_LIBS := libvmpp-enc.so
else
SDK_EXPECT_LIBS := libvmpp-dec.so libvmpp-enc.so
endif
ifeq ($(strip $(SDK_STATIC)),1)
SDK_EXPECT_LIBS += $(SDK_EXPECT_LIBS:.so=.a)
endif

# x86 构建时用宿主机默认编译器（屏蔽外部传入的交叉 CC/CXX）；arm64 时导出交叉编译器给 cmake
# SDK 的 cmake 同时 enable C 和 CXX，所以 g++ 也必须存在
SDK_CC ?= $(CROSS_COMPILE)gcc
SDK_CXX ?= $(CROSS_COMPILE)g++
ifeq ($(strip $(SDK_ARM64)),1)
SDK_ENV := CC=$(SDK_CC) CXX=$(SDK_CXX)
else
SDK_ENV := env -u CC -u CXX
endif

.PHONY: all gpu_driver gpu_userspace mem_driver vpu_driver vpu_userspace \
	vpu_sdk clean help

all: gpu_driver gpu_userspace mem_driver vpu_driver vpu_userspace 

help:
	@echo "Build module + runtime:"
	@echo "  make KERNEL_BUILD=/path/to/arm64/kernel/build"
	@echo "Build gpu_userspace only:"
	@echo "  make gpu_userspace"
	@echo "Build VC9000SDK(vmpp) only:"
	@echo "  make vpu_sdk [SDK_TARGET=0|1|2] [SDK_RELEASE=0|1] [SDK_ARM64=0|1]"

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


vpu_userspace: $(H264_LAUNCHER) $(HEVC_LAUNCHER) $(DEC_LAUNCHER) vpu_sdk

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


# ---------------- NEW: VC9000SDK(vmpp) 顶层目标 ----------------
# autobuild.sh 内部使用相对路径 build/ lib/ debug/|release/，必须先 cd 到 SDK 目录执行
vpu_sdk: | $(BUILD)
	@test -f $(VPU_SDK_AUTOBUILD) || { echo "error: $(VPU_SDK_AUTOBUILD) not found"; exit 1; }
	cd $(VPU_SDK_DIR) && $(SDK_ENV) sh ./autobuild.sh $(SDK_FLAGS)
	@for f in $(SDK_EXPECT_LIBS); do \
		test -f "$(VPU_SDK_DIR)/lib/$$f" || \
			{ echo "error: $$f not built, autobuild.sh failed"; exit 1; }; \
	done
	mkdir -p $(BUILD)/lib $(BUILD)/inc
	cp -f $(VPU_SDK_DIR)/lib/*.so $(BUILD)/lib/
	cp -rf $(VPU_SDK_DIR)/inc/. $(BUILD)/inc/
	@if [ -d "$(VPU_SDK_DIR)/sample/build" ]; then \
		find $(VPU_SDK_DIR)/sample/build -maxdepth 1 -type f -executable \
			-exec cp -f {} $(BUILD)/ \; ; \
	fi
	@echo ">>> vmpp libs/samples installed to $(BUILD)"

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
	$(RM) -r $(VPU_SDK_DIR)/build $(VPU_SDK_DIR)/lib \
		$(VPU_SDK_DIR)/debug $(VPU_SDK_DIR)/release
	$(RM) -r $(BUILD)
