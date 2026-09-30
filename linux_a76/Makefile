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

# VC9000SDK(vmpp)：直接调用 userspace_vpu/VC9000SDK/autobuild.sh
VPU_SDK_DIR := $(HOST_LINUX_DRIVER)/userspace_vpu/VC9000SDK
VPU_SDK_AUTOBUILD := $(VPU_SDK_DIR)/autobuild.sh
# 安装到 bins/vmpp，可直接作为 INITRAMFS_OVERLAY 打包进 guest
VPU_SDK_OUT ?= $(BIN_DIR)
# -t: 0:dec+enc  1:dec  2:enc
SDK_TARGET ?= 0
# -r: 0:debug  1:release
SDK_RELEASE ?= 0
# -s: 0:off  1:ASan  2:TSan
SDK_SANITIZER ?= 0
# -d: dynamic resolution
SDK_DYNRES ?= 0
# -a: 本 Makefile 面向 arm64 guest，默认 1
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

# SDK_ARM64=1 时导出交叉编译器给 autobuild.sh 里的 cmake / 子 Makefile
# SDK 的 cmake 同时 enable C 和 CXX，所以 g++ 也必须存在
SDK_CC ?= $(CROSS_COMPILE)gcc
SDK_CXX ?= $(CROSS_COMPILE)g++
ifeq ($(strip $(SDK_ARM64)),1)
SDK_ENV := CC=$(SDK_CC) CXX=$(SDK_CXX)
else
SDK_ENV := env -u CC -u CXX
endif

.PHONY: all tools kernel-config kernel-image driver initramfs payload \
	vpu_sdk vpu_sdk_clean clean help

all: payload

help:
	@echo "Build host-independent pieces: make tools"
	@echo "Configure kernel:              make KERNEL_SRC=/path/to/linux kernel-config"
	@echo "Build complete payload:        make KERNEL_SRC=/path/to/linux all"
	@echo "Build VC9000SDK(vmpp) only:    make vpu_sdk [SDK_TARGET=0|1|2] [SDK_STATIC=1]"
	@echo "Build SDK and bake into guest: make vpu_sdk payload INITRAMFS_OVERLAY=$(VPU_SDK_OUT)"
	@echo "Run after R52 build:           ../simulation/run_linux.sh"

tools: $(BOOT_BIN) $(DTB) $(INIT) $(LAUNCHER) $(H264_LAUNCHER) $(HEVC_LAUNCHER) $(DEC_LAUNCHER) vpu_sdk

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

# ---------------- NEW: VC9000SDK(vmpp) 顶层目标 ----------------
# autobuild.sh 内部使用相对路径 build/ lib/ debug/|release/，必须先 cd 到 SDK 目录执行
vpu_sdk: | $(BIN_DIR)
	test -f $(VPU_SDK_AUTOBUILD) || { echo "error: $(VPU_SDK_AUTOBUILD) not found"; exit 1; }
	cd $(VPU_SDK_DIR) && $(SDK_ENV) sh ./autobuild.sh $(SDK_FLAGS)
	@for f in $(SDK_EXPECT_LIBS); do \
		test -f "$(VPU_SDK_DIR)/lib/$$f" || \
			{ echo "error: $$f not built, autobuild.sh failed"; exit 1; }; \
	done
#	mkdir -p $(VPU_SDK_OUT)/lib $(VPU_SDK_OUT)/include $(VPU_SDK_OUT)/bin
#	cp -f $(VPU_SDK_DIR)/lib/*.so $(VPU_SDK_OUT)/
#	@if ls $(VPU_SDK_DIR)/lib/*.a >/dev/null 2>&1; then \
		cp -f $(VPU_SDK_DIR)/lib/*.a $(VPU_SDK_OUT)/; \
	fi
#	cp -rf $(VPU_SDK_DIR)/inc/. $(VPU_SDK_OUT)/include/
	@if [ -d "$(VPU_SDK_DIR)/sample/build/out" ]; then \
		find $(VPU_SDK_DIR)/sample/build/out -maxdepth 1 -type f -executable \
			-exec cp -f {} $(VPU_SDK_OUT)/ \; ; \
	fi
	@echo ">>> vmpp installed to $(VPU_SDK_OUT)"
	@echo ">>> package into guest: make payload INITRAMFS_OVERLAY=$(VPU_SDK_OUT)"

vpu_sdk_clean:
	$(RM) -r $(VPU_SDK_DIR)/build $(VPU_SDK_DIR)/lib \
		$(VPU_SDK_DIR)/debug $(VPU_SDK_DIR)/release
	$(RM) -r $(VPU_SDK_OUT)



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

# mkinitramfs.sh 直接把 $(BIN_DIR) 下的样本装进镜像，所以样本重建后必须重新打包，
# 否则 guest 里跑的还是旧产物（典型症状：明明换了静态链接，还是 "not found"）。
# $(BIN_DIR)/vmpp 只是宿主机二次开发用的 .a/.so/头文件，不进 initramfs，故排除。
GUEST_BINS := $(filter-out $(BIN_DIR)/vmpp,$(wildcard $(BIN_DIR)/*))

$(INITRAMFS): $(INIT) $(LAUNCHER_IMAGE) $(MODULE) scripts/mkinitramfs.sh $(GUEST_BINS)
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
	$(MAKE) vpu_sdk_clean
	$(RM) -rf $(BUILD) $(BIN_DIR)
