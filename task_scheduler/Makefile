
# Project setup
PROJ_ROOT    := .
PROJ_OUT     := $(PROJ_ROOT)/build
OBJ_DIR      := $(PROJ_OUT)/obj
BIN_DIR      := $(PROJ_OUT)/bin
TARGET       := task_sched
ELF_TARGET   := $(BIN_DIR)/$(TARGET).elf
BIN_TARGET   := $(BIN_DIR)/$(TARGET).bin
LINKER_FILE  := $(PROJ_ROOT)/link.ld
MAPFILE      := $(BIN_DIR)/$(TARGET).map

# Machine type
MACH_TYPE := -mcpu=cortex-r52 -mfloat-abi=soft

# Code type: arm, thumb
# R52 本地 RAM 已扩到 16MB(conf.lua ram_0),代码+数据+堆+栈全部放本地 RAM,
# 不再受 96KB/88KB 限制。BQueue/CQueue 大数据区放共享 DDR(utils/ddr_mem.c,
# link.ld .ddr_buf 段 @0x80900000),减轻本地堆压力。
# 向量表 startup.S 与 portASM.S 强制 .arm,异常入口 T 位会被清零(ARM 态),
# 与应用层 Thumb 混合经链接器 veneer 自动互操作。
CODE_TYPE ?= thumb

# Optimization level: 0~3, g
# 用 -Os 压缩代码体积以在 96KB 本地 RAM 内放下代码+数据镜像。如需完整调试信息可临时改回 1
OPT_LEVEL ?= 1

# Debug option
#DEBUG_FLAGS ?=
DEBUG_FLAGS ?= -g

# Tool setup
#TOOLCHAIN        := /home/stone/tools/arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi/bin/arm-none-eabi
TOOLCHAIN        := arm-none-eabi

AS           	 := $(TOOLCHAIN)-as
CC           	 := $(TOOLCHAIN)-gcc
LD           	 := $(TOOLCHAIN)-ld
OBJCOPY      	 := $(TOOLCHAIN)-objcopy
DISSAMBLER   	 := $(TOOLCHAIN)-objdump


# ==============================================================================
# Include Paths
# ==============================================================================
KERNEL_DIR     := FreeRTOS-Kernel
PORTABLE_DIR   := $(KERNEL_DIR)/portable
MEM_MANG_DIR   := $(PORTABLE_DIR)/MemMang
GCC_PORT_DIR   := $(PORTABLE_DIR)/GCC/ARM_CR52

INCLUDES       := \
	-I$(PROJ_ROOT)/arch \
	-I$(PROJ_ROOT)/driver \
	-I$(PROJ_ROOT)/include \
	-I$(PROJ_ROOT)/mailbox \
	-I$(PROJ_ROOT)/system \
	-I$(PROJ_ROOT)/utils \
	-I$(PROJ_ROOT)/vcodec \
	-I$(KERNEL_DIR)/include \
	-I$(GCC_PORT_DIR)


#
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
    $(PROJ_ROOT)/system/syscall.c \
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

# 5. 【修复核心】启动汇编,提供 Start 入口
# 注: 裸机 arch/exception_handlers.S(EL1_IRQ_Handler)已不再链接——
# EL1 IRQ/SVC 统一由 FreeRTOS ARM_CR52 移植层(portASM.S)的
# FreeRTOS_IRQ_Handler / FreeRTOS_SWI_Handler 接管(向量表 startup.S 已指向它们)
STARTUP_ASM    := \
    $(PROJ_ROOT)/arch/startup.S \
	$(PROJ_ROOT)/arch/generictimer.S \
	$(PROJ_ROOT)/arch/v8rgicv3cpuif.S

# 6. FreeRTOS内核
VCODEC_SRCS    := \
    $(PROJ_ROOT)/mailbox/cmdr52_proc.c \
    $(PROJ_ROOT)/mailbox/cmdr52_mgr.c \
    $(PROJ_ROOT)/mailbox/cmdr52_session.c \
    $(PROJ_ROOT)/utils/kref.c \
    $(PROJ_ROOT)/utils/crc32.c \
    $(PROJ_ROOT)/utils/bqueue.c \
    $(PROJ_ROOT)/utils/cqueue.c \
    $(PROJ_ROOT)/utils/ddr_mem.c \
    $(PROJ_ROOT)/utils/wait_queue.c \
    $(PROJ_ROOT)/vcodec/bidirect_list.c \
    $(PROJ_ROOT)/vcodec/vcd_abnormal_irq.c \
    $(PROJ_ROOT)/vcodec/vcd_vcmd_pm.c \
    $(PROJ_ROOT)/vcodec/vcd_vcmd.c \
    $(PROJ_ROOT)/vcodec/vce_abnormal_irq.c \
    $(PROJ_ROOT)/vcodec/vce_vcmd_pm.c \
    $(PROJ_ROOT)/vcodec/vce_vcmd.c \
    $(PROJ_ROOT)/vcodec/vcmdswhwregisters.c \
    $(PROJ_ROOT)/vcodec/vcodec.c \
    $(PROJ_ROOT)/vcodec/vcx_axi2to1.c \
    $(PROJ_ROOT)/vcodec/vcx_axife.c \
    $(PROJ_ROOT)/vcodec/vcx_cmdbuf_obj.c \
    $(PROJ_ROOT)/vcodec/vcx_kthread.c \
    $(PROJ_ROOT)/vcodec/vcx_vcmd_dbg_log.c \
    $(PROJ_ROOT)/vcodec/vcx_vcmd_dbgfs.c \
    $(PROJ_ROOT)/vcodec/vcx_vcmd_irq.c \
    $(PROJ_ROOT)/vcodec/vcx_watchdog.c




#    $(PROJ_ROOT)/task/vcodec.c \

# 汇总源码
SRC_C          := $(APP_SRCS) $(KERNEL_SRCS) $(PORT_SRCS_C) $(HEAP_SRCS) $(VCODEC_SRCS)
SRC_ASM        := $(PORT_SRCS_ASM) $(STARTUP_ASM)

# 目标文件
C_OBJS         := $(patsubst %.c, $(OBJ_DIR)/%.o, $(SRC_C))
ASM_OBJS       := $(patsubst %.S, $(OBJ_DIR)/%.o, $(SRC_ASM))
OBJS           := $(C_OBJS) $(ASM_OBJS)



# -mfpu=vfpv4:让汇编器接受 VFP 指令,并让 gcc 为 .S 预处理定义 __ARM_FP,
# 从而激活 startup.S 中 CPACR/FPEXC 使能块(FreeRTOS ARM_CR52 port 的
# VPUSH/VPOP 需要 VFP 打开,否则首次上下文切换即 UNDEF)
ASFLAGS      	 := $(DEBUG_FLAGS) $(MACH_TYPE) -mfpu=vfpv4 -D__FREERTOS__
CFLAGS       	 := -O$(OPT_LEVEL) $(DEBUG_FLAGS) -std=gnu99 $(MACH_TYPE) -m$(CODE_TYPE) \
					-mno-thumb-interwork -mabi=aapcs -mno-unaligned-access \
					-fno-builtin-printf -fomit-frame-pointer -fno-strict-aliasing -fno-builtin-memcpy \
                    -Wall -Wextra -D__FREERTOS__
LDFLAGS          := -static --specs=nosys.specs -T $(LINKER_FILE) -Wl,-Map=$(MAPFILE)
LINKER_OPTS_ASM  :=
LINKER_FILE      :=
DISSAMBLER_OPTS	 := --disassemble-all --disassemble-zeroes

CFLAGS += $(INCLUDES)

ASFLAGS  += $(INCLUDES)

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

# FreeRTOS ARM_CR52 移植层使用 FPU 指令(FMXR/FMXR/VPUSH/VPOP 含 D16-D31),
# 而工程主体为 soft-float(-mfloat-abi=soft),FPU 指令会被汇编器拒绝,
# 因此移植层单独以 softfp/-mfpu=vfpv4(D32) 编译(指令集与全局一致,ARM 态)
$(OBJ_DIR)/FreeRTOS-Kernel/portable/GCC/ARM_CR52/port.o: $(GCC_PORT_DIR)/port.c
	@echo "  CC(FPU)  $<"
	$(CC) $(CFLAGS) -marm -mfloat-abi=softfp -mfpu=vfpv4 -c $< -o $@

# FreeRTOS ARM_CR52 移植层汇编:portASM.S 使用 FMXR/VPUSH/VPOP(含 D16-D31)等 FPU 指令,
# soft-float 下无法汇编,同样以 softfp/-mfpu=vfpv4(D32) 编译
$(OBJ_DIR)/FreeRTOS-Kernel/portable/GCC/ARM_CR52/portASM.o: $(GCC_PORT_DIR)/portASM.S
	@echo "  AS(FPU) $<"
	$(CC) $(ASFLAGS) -mfloat-abi=softfp -mfpu=vfpv4 -c $< -o $@

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
