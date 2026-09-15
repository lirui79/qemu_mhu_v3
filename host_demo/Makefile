# ==============================================================================
# Project Configuration
# ==============================================================================
PROJ_ROOT    := .
PROJ_OUT     := $(PROJ_ROOT)/build
OBJ_DIR      := $(PROJ_OUT)/obj
BIN_DIR      := $(PROJ_OUT)/bin
TARGET       := host_demo

# Output Files
ELF_TARGET   := $(BIN_DIR)/$(TARGET).elf
BIN_TARGET   := $(BIN_DIR)/$(TARGET).bin
MAPFILE      := $(BIN_DIR)/$(TARGET).map
LINKER_FILE  := $(PROJ_ROOT)/link.ld

# ==============================================================================
# Toolchain Setup (修复命名歧义：仅路径+通用前缀，不带工具后缀)
# ==============================================================================
#TOOLCHAIN_PATH := /home/stone/tools/arm-gnu-toolchain-15.2.rel1-x86_64-aarch64-none-elf/bin
#CROSS_PREFIX   := $(TOOLCHAIN_PATH)/aarch64-none-elf
CROSS_PREFIX   := aarch64-none-elf

AS             := $(CROSS_PREFIX)-as
CC             := $(CROSS_PREFIX)-gcc
LD             := $(CROSS_PREFIX)-ld
OBJCOPY        := $(CROSS_PREFIX)-objcopy
OBJDUMP        := $(CROSS_PREFIX)-objdump

# ==============================================================================
# Arch Base Flags
# ==============================================================================
CPU_ARCH     := -mcpu=cortex-a76 -march=armv8.2-a
OPT_LEVEL    ?= 2
DEBUG_FLAGS  ?= -g

# ==============================================================================
# Include Paths
# ==============================================================================
KERNEL_DIR     := FreeRTOS-Kernel
PORTABLE_DIR   := $(KERNEL_DIR)/portable
MEM_MANG_DIR   := $(PORTABLE_DIR)/MemMang
GCC_PORT_DIR   := $(PORTABLE_DIR)/GCC/ARM_AARCH64

INCLUDES       := \
	-I$(PROJ_ROOT)/arch \
	-I$(PROJ_ROOT)/driver \
	-I$(PROJ_ROOT)/include \
	-I$(PROJ_ROOT)/mailbox \
	-I$(PROJ_ROOT)/system \
	-I$(PROJ_ROOT)/task \
	-I$(PROJ_ROOT)/utils \
	-I$(PROJ_ROOT)/vcodec \
	-I$(KERNEL_DIR)/include \
	-I$(GCC_PORT_DIR)

# ==============================================================================
# Source Files
# 【关键补充】必须添加 arch/start.S 启动汇编，提供 _start 入口
# ==============================================================================
# 1. 应用层C代码
APP_SRCS       := \
    $(PROJ_ROOT)/driver/gicv3_basic.c \
    $(PROJ_ROOT)/driver/mhu_davarae.c \
    $(PROJ_ROOT)/driver/uart_pl011.c \
    $(PROJ_ROOT)/system/interrupt.c \
    $(PROJ_ROOT)/system/timer.c \
    $(PROJ_ROOT)/system/debug.c \
    $(PROJ_ROOT)/system/vcodec_test.c \
    $(PROJ_ROOT)/system/main.c

# 2. FreeRTOS内核
KERNEL_SRCS    := \
    $(KERNEL_DIR)/tasks.c \
    $(KERNEL_DIR)/queue.c \
    $(KERNEL_DIR)/list.c \
    $(KERNEL_DIR)/timers.c \
    $(KERNEL_DIR)/event_groups.c \
    $(KERNEL_DIR)/stream_buffer.c \
    $(KERNEL_DIR)/croutine.c

# 3. FreeRTOS AArch64 移植层
PORT_SRCS_C    := $(GCC_PORT_DIR)/port.c
PORT_SRCS_ASM  := $(GCC_PORT_DIR)/portASM.S

# 4. 内存管理
HEAP_SRCS      := $(MEM_MANG_DIR)/heap_4.c

# 5. 【修复核心】裸机启动汇编，提供 _start 入口
STARTUP_ASM    := \
    $(PROJ_ROOT)/arch/startup.S \
	$(PROJ_ROOT)/arch/gicv3_cpuif.S

# 6. FreeRTOS内核
VCODEC_SRCS    := \
    $(PROJ_ROOT)/mailbox/cmdnode.c \
    $(PROJ_ROOT)/mailbox/cmda78_mgr.c \
    $(PROJ_ROOT)/mailbox/cmda78_msg.c \
    $(PROJ_ROOT)/mailbox/cmda78_proc.c \
    $(PROJ_ROOT)/mailbox/cmda78_session.c \
    $(PROJ_ROOT)/task/vcodec.c \
    $(PROJ_ROOT)/utils/kref.c \
    $(PROJ_ROOT)/utils/crc32.c \
    $(PROJ_ROOT)/utils/rbtree.c \
    $(PROJ_ROOT)/utils/bqueue.c \
    $(PROJ_ROOT)/utils/cqueue.c \
    $(PROJ_ROOT)/utils/wait_queue.c \
    $(PROJ_ROOT)/vcodec/vcx_cmdbuf_obj.c


# 汇总源码
SRC_C          := $(APP_SRCS) $(KERNEL_SRCS) $(PORT_SRCS_C) $(HEAP_SRCS) $(VCODEC_SRCS)
SRC_ASM        := $(PORT_SRCS_ASM) $(STARTUP_ASM)

# 目标文件
C_OBJS         := $(patsubst %.c, $(OBJ_DIR)/%.o, $(SRC_C))
ASM_OBJS       := $(patsubst %.S, $(OBJ_DIR)/%.o, $(SRC_ASM))
OBJS           := $(C_OBJS) $(ASM_OBJS)

# ==============================================================================
# 统一编译参数（修复重复覆盖CFLAGS问题，全部追加不覆盖）
# ==============================================================================
# C编译器参数
CFLAGS  := $(CPU_ARCH)
CFLAGS += -O$(OPT_LEVEL) $(DEBUG_FLAGS)
CFLAGS += -Wall -Wextra -Wno-unused-parameter
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -fno-strict-aliasing
# 裸机关键标记
CFLAGS += -nostdlib -nostartfiles -ffreestanding
# 禁用内置函数，避免标准库依赖
CFLAGS += -fno-builtin-printf -fno-builtin-memcpy -fno-builtin-memset
CFLAGS += -std=gnu99 -DGUEST -DQEMU -D__FREERTOS__
CFLAGS += $(INCLUDES)

# 汇编器参数（支持C预处理器 #include / #define）
ASFLAGS  := $(CPU_ARCH)
ASFLAGS  += -x assembler-with-cpp
ASFLAGS  += $(DEBUG_FLAGS) -DGUEST -D__FREERTOS__
ASFLAGS  += $(INCLUDES)

# 链接参数（统一抽取，不再硬编码）
LDFLAGS  := -T $(LINKER_FILE)
LDFLAGS  += -nostdlib -nostartfiles -ffreestanding
LDFLAGS  += -Wl,--gc-sections
# 生成内存布局Map文件
LDFLAGS  += -Wl,-Map=$(MAPFILE)
# AArch64 对齐、段保护
LDFLAGS  += -Wl,--build-id=none

# 反汇编参数
DISSAMBLER_OPTS := --disassemble-all --disassemble-zeroes -S

# ==============================================================================
# Build Rules
# ==============================================================================
.PHONY: all clean dirs disasm

all: dirs $(ELF_TARGET) $(BIN_TARGET)
	@echo "========================================"
	@echo "Build Success!"
	@echo "ELF: $(ELF_TARGET)"
	@echo "BIN: $(BIN_TARGET)"
	@echo "MAP: $(MAPFILE)"
	@echo "========================================"

# 递归创建所有obj目录
dirs:
	@mkdir -p $(OBJ_DIR) $(BIN_DIR)
	@mkdir -p $(dir $(OBJS))

# 链接规则：复用LDFLAGS，-lgcc放在所有obj文件之后（链接顺序规范）
$(ELF_TARGET): $(OBJS)
	@echo "  LD    $@"
	$(CC) $(CPU_ARCH) $(LDFLAGS) $(OBJS) -lgcc -o $@

# ELF转纯二进制镜像
$(BIN_TARGET): $(ELF_TARGET)
	@echo "  OBJCOPY  $@"
	$(OBJCOPY) -O binary $< $@

# C文件编译模板
$(OBJ_DIR)/%.o: %.c
	@echo "  CC    $<"
	$(CC) $(CFLAGS) -c $< -o $@

# 汇编.S文件编译模板
$(OBJ_DIR)/%.o: %.S
	@echo "  AS    $<"
	$(CC) $(ASFLAGS) -c $< -o $@

# 清理产物
clean:
	rm -rf $(PROJ_OUT)
	@echo "Clean build dir done"

# 完整反汇编输出
disasm: $(ELF_TARGET)
	$(OBJDUMP) $(DISSAMBLER_OPTS) $(ELF_TARGET) > $(BIN_DIR)/$(TARGET).dis
	@echo "Disasm dump: $(BIN_DIR)/$(TARGET).dis"