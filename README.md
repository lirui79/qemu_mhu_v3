# Task Scheduler Test

## 内容说明

本项目包含 Cortex A76 和双核 Cortex R52 的仿真运行环境和固件源码. 
A76 和 R52 之间共享 DDR 内存, 并支持通过 MHU-DavarAE 进行通信.

目前代码主要用于 BiGPU `runtime <--> KMD driver <--> task scheduler <--> GPU Core` 流程测试, 支持两种固件环境:
1. 裸机环境: A76 运行裸机固件 + R52 运行裸机固件
2. Linux 环境: A76 运行 Linux 固件 + R52 运行裸机固件

裸机环境相关源码:
- `task_scheduler` : R52 裸机固件源码, 使用 `arm-none-eabi-gcc` 编译
- `host_demo` : A76 裸机固件源码, 使用 `aarch64-none-elf-gcc` 编译

Linux环境相关源码:
- `task_scheduler` : R52 裸机固件源码, 使用 `arm-none-eabi-gcc` 编译
- `linux_a76` : A76 Linux 固件相关配置和脚本, 使用 `aarch64-linux-gnu-gcc` 编译
- `linux-6.8` : Linux 内核源码, 使用 `tar xvf linux-6.8.tar.xz` 生成
- `runtime_demo` : runtime 示例内核驱动和用户程序, 使用 `aarch64-linux-gnu-gcc` 编译

其他目录:
- `simulation` : 包含仿真程序, 库文件, CModel 模型和测试脚本
- `log` : 运行测试后生成, 包含 R52 的串口日志 (`ts_core0/1.log`), A76 的串口打印输出到主机终端

### 支持平台

已验证主机平台:
- Ubuntu 22.04 x86_64
- Ubuntu 24.04 x86_64

### 安装依赖

- 安装基础工具

```bash
sudo apt-get update
sudo apt-get install -y build-essential gcc-arm-none-eabi curl ca-certificates xz-utils psmisc libzip4 libsndio7.0
```

- (可选) 安装 `aarch64-none-elf` 工具链 (如不需要运行 A76 裸机固件可跳过)

```bash
version=15.2.rel1
archive="arm-gnu-toolchain-${version}-x86_64-aarch64-none-elf.tar.xz"
url="https://developer.arm.com/-/media/Files/downloads/gnu/${version}/binrel/${archive}"

curl -fL --retry 3 -o "/tmp/${archive}" "${url}"
echo "66f7ce7c1bf662f589a4caf440812375f3cd8000a033ccf0971127a0726d6921  /tmp/${archive}" \
  | sha256sum -c -
sudo tar -xJf "/tmp/${archive}" -C /opt
sudo ln -sf "/opt/arm-gnu-toolchain-${version}-x86_64-aarch64-none-elf/bin/"aarch64-none-elf-* \
  /usr/local/bin/
```
- (可选) 安装 `aarch64-linux-gnu-gcc` 工具链 (如不需要运行 A76 Linux 固件可跳过)

```bash
sudo apt-get install gcc-aarch64-linux-gnu device-tree-compiler cpio
```

- (可选) 安装 `gdb-multiarch` 工具 (如不需要使用 gdb 调试可跳过)

```bash
sudo apt-get install -y gdb-multiarch
```
---

## 固件说明

### task_scheduler

`task_scheduler` 是 Cortex-R52 双核任务调度器固件:

- core 0 入口函数为 `main`, 使用 UART0 输出日志并运行命令处理器 `command_processor`
- core 1 入口函数为 `main_core1`, 使用 UART1 输出日志并运行自己的任务(如 `core1_demo_task`)
- `ts_printf` 会根据 CPU ID 自动选择对应 UART

AMP(每核独立 FreeRTOS 实例):

- 双核共享 16MB 本地 RAM, 采用 "共享代码文本 + 每核独立内核状态" 的方式
  (见 `FreeRTOSConfig.h` 的 `configACTIVE_CORE_COUNT` 与
  `portmacro.h`/`tasks.c`/`heap_4.c`/`portASM.S` 中的 per-core 状态)。
- 每个核拥有完全独立的调度器、任务链表、tick 计数、临界嵌套计数和堆
  (每核 64KB), 由当前核 ID(MPIDR 低位)索引, 两核互不干扰。
- 每个核各自调用 `vTaskStartScheduler()` 启动自己的调度器; 共享队列/信号量
  不应跨核使用(队列的阻塞链表只由创建/等待它的核维护)。
- **每个核都必须调用 `interrupt_init()`**:GIC 的 CPU interface
  (`ICC_PMR`/`ICC_IGRPEN0/1_EL1`)是 per-core 状态。若 core1 跳过该初始化,
  Group1 的 PPI30(tick)虽然已在 RD 上使能,但不会被 CPU interface 投递,
  表现为 core1 收不到任何中断、`vTaskDelay` 永久挂起。
- PPI30(物理定时器,1ms)作为各核 FreeRTOS tick。为避免每 tick 的
  `ts_printf("IRQ:30")` 刷屏(双核 1ms tick 会以每秒数千行、逐字符交错的方式
  淹没日志),`vApplicationIRQHandler` 只打印非 tick 中断;tick 分支直接走
  `FreeRTOS_Tick_Handler()`。

地址空间:

| 起始地址 | 结束地址 | 大小 | 类型 | 访问权限 | 用途 |
| --- | --- | --- | --- | --- | --- |
| `0x00000000` | `0x00FFFFFF` | 16 MB | RAM | 读写 | 代码/数据/栈/堆(每核独立 64KB FreeRTOS 堆) |
| `0x2F000000` | `0x2FFFFFFF` | 16 MB | Peripheral | 读写 | 设备寄存器 |
| `0x80000000` | `0x9FFFFFFF` | 512 MB | DDR | 读写 | DDR 内存(A76 与 R52 共享) |
| `0x80900000` | `0x80AFFFFF` | 2 MB | DDR | 读写 | R52 共享数据缓冲(BQueue/CQueue 数据区, 由 `ddr_mem` bump 分配) |

MPU 配置参考 `task_scheduler/arch/startup.S` 中的 `MPU Configuration` 代码.

共享 DDR 数据缓冲说明:

- R52 的 BQueue/CQueue 大数据区(如 `BQueueCreate(1024,128)` 的 128KB 数据区
  及两个 512KB 的 CQueue 数据区)不再从本地 RAM 的 FreeRTOS 堆分配, 而是经
  `utils/ddr_mem.c` 的 `ddr_alloc()` 从共享 DDR `0x80900000` 起的 2MB 缓冲段
  (`link.ld` 的 `.ddr_buf` 段)分配, 避开 A76 固件区(`0x80100000..0x80500000`)
  与 A76 动态区(`0x81000000` 起)。
- `ddr_alloc` 为简单 bump 分配器, `ddr_free` 仅支持按分配逆序释放,
  与 BQueueCreate/BQueueDelete 的创建/销毁顺序一致。

### host_demo

`host_demo` 是 Cortex-A76 模拟 HOST CPU 处理逻辑裸机固件.
它通过 MHU 与 R52 TS 通信, 支持查询 BiGPU 相关信息, 模拟创建进程、创建 AQL 队列, 提交 Kernel Dispatch 请求等操作.

地址空间:
| 起始地址 | 结束地址 | 大小 | 类型 | 访问权限 | 用途 |
| --- | --- | --- | --- | --- | --- |
| `0x80000000` | `0x800FFFFF` | 1 MB | DDR | - | 保留 |
| `0x80100000` | `0x804FFFFF` | 4 MB | DDR | 只读 | 代码 |
| `0x80500000` | `0x808FFFFF` | 4 MB | DDR | 读写 | 数据和栈 |
| `0x81000000` | `0x9FFFFFFF` | 486 MB | DDR | 读写 | A76 和 R52 共享内存 |

测试参数和 AQL 包构造逻辑示例位于 `host_demo/test/launch_kernel_case_1.c`

### linux_a76

`linux_a76` 包含 Cortex-A76 Linux 固件相关文件, 配置和构造脚本.

地址空间:

| 起始地址 | 内容 |
|---|---|
| `0x80100000` | boot shim |
| `0x80110000` | DTB |
| `0x80200000` | arm64 Linux `Image` |
| `0x84100000` | initramfs，最大 16 MB |
| `0x90000000` | Linux/R52 non-cacheable IPC，1 MB |

详细请参考 `linux_a76/README.md` 文件.

### runtime_demo

`runtime_demo` 包含 BiGPU runtime 程序和驱动代码, 目的是模拟真实场景 runtime 解析 ELF 编译文件, 创建队列和下发计算任务等逻辑。
- `driver` : 模拟驱动逻辑和用户态接口, 生成驱动文件 `r52_gpu.ko`
- `userspace` : 模拟 runtime 处理逻辑, 生成可执行文件 `launch_kernel`

---

## 编译和测试

### 裸机环境

#### 编译

编译 R52 固件:

```bash
make -C task_scheduler clean all
```

编译 A76 固件:

```bash
make -C host_demo clean all
```

生成文件位于各自的 `build` 目录：

```text
task_scheduler/build/bin/
|-- task_sched.bin
|-- task_sched.elf
`-- task_sched.map

host_demo/build/bin/
|-- host_demo.bin
|-- host_demo.elf
`-- host_demo.map
```

R52 固件支持以下编译选项：

- `CODE_TYPE`：指令类型，可配置为 `arm` 或 `thumb`，默认使用 `thumb`。
- `OPT_LEVEL`：优化等级，可配置为 `0` 到 `3`，默认为 `1`。
- `DEBUG_FLAGS`：调试参数，默认启用 `-g`。

A76 固件支持 `OPT_LEVEL` 和 `DEBUG_FLAGS`。例如：

```bash
make -C task_scheduler OPT_LEVEL=2 DEBUG_FLAGS=""
make -C host_demo OPT_LEVEL=2 DEBUG_FLAGS=""
```

#### 运行

执行脚本:

```bash
./simulation/run.sh
```

日志输出位置：
- A76 日志直接输出到当前终端
- R52 core 0 日志写入 `log/ts_core0.log`
- R52 core 1 日志写入 `log/ts_core1.log`
- 仿真脚本退出时会自动生成去除 NUL 字符后的可读副本: `log/ts_core0.clean.log` 和 `log/ts_core1.clean.log`

可在其他终端跟踪 R52 日志：

```bash
tail -f log/ts_core0.log
tail -f log/ts_core1.log
```

终止仿真：

```bash
killall cortex-r52-a76-vp
```

#### GDB 调试

执行脚本:

```bash
./simulation/run_gdb.sh
```

在其他终端连接目标：

```bash
# R52；进入 GDB 后可使用 thread 命令切换两个核心
./simulation/gdb_connect_r52.sh

# A76
./simulation/gdb_connect_a76.sh
```

### Linux 环境

#### 一键运行

Linux 环境支持一键编译并运行脚本:

```bash
./linux_a76/scripts/run_linux_demo.sh
```

生成文件位于 `linux_a76/build` 目录. 日志输出位置：
- A76 日志直接输出到当前终端
- R52 core 0 日志写入 `log/ts_core0.log`
- R52 core 1 日志写入 `log/ts_core1.log`

可在其他终端跟踪 R52 日志：

```bash
tail -f log/ts_core0.log
tail -f log/ts_core1.log
```

启动后自动进入调试终端:

```text
Welcome to Linux!
~ #
~ #
~ # uname -a
Linux (none) 6.8.0-cmodel #2 SMP PREEMPT Sat Aug  8 09:23:43 CST 2026 aarch64 GNU/Linux
~ #
~ # insmod /lib/modules/r52_gpu.ko
[   28.909493] r52_gpu: loading out-of-tree module taints kernel.
[   28.972786] r52_gpu 2fc20000.r52-gpu: mhu: rx_irq=14 pbx_iidr=0xf7043b mbx_iidr=0xf7043b dbch_cfg=0x3 ffch_cfg=0x3ff0403 fch_cfg=0x41f041f stale_db=0x1
[   29.102319] r52_gpu 2fc20000.r52-gpu: /dev/r52_gpu ready, shared memory=0x00000000846aa000
~ #
~ # launch_kernel
R52 GPU 1234:5678, 3 SM, ABI 1
sequence=1 completed in 585 us
~ #
```

#### GDB 调试

执行脚本:

```bash
./linux_a76/scripts/run_linux_gdb.sh
```

在其他终端连接目标：

```bash
# R52；进入 GDB 后可使用 thread 命令切换两个核心
./simulation/gdb_connect_r52.sh

# A76
./simulation/gdb_connect_a76.sh
```

#### 可选参数

默认使用 `ts_test/linux-6.8` 内核源码, 如需指定其他版本内核, 可使用参数：

```bash
./linux_a76/scripts/run_linux_demo.sh /your/path/to/linuxsrc
```

如需启动后自动加载 `r52_gpu.ko` 驱动并运行 `launch_kernel` 程序, 可使用参数:

```bash
INIT_AUTORUN=1 ./linux_a76/scripts/run_linux_demo.sh
```
