# Cortex-A76 Linux Driver and Runtime

这个目录把原来的 A76 裸机 host 替换为 arm64 Linux 启动链，同时保留 R52
task scheduler 固件。目标链路是：

```text
launch_kernel
  -> libr52 runtime API
  -> /dev/r52_gpu GET_RESOURCES + mmap
  -> userspace 填 shared AQL queue，并写 mmap 后的 send fastchannel
  -> R52 command processor + AQL scheduler
  -> completion event
  -> Linux driver IRQ + poll 唤醒 userspace
```

当前 `task_scheduler/system/aql_proc.c` 用固定延时模拟 kernel execution，因此
这里验证的是 runtime、Linux driver、MHU、R52 firmware 和 AQL completion 的
完整控制路径，不会执行真实 GPU 指令。

## 目录

```text
linux_a76/
|-- boot/           arm64 Linux boot protocol 跳板
|-- bins/           要打包到 guest `/bin` 的运行程序，构建生成或手工替换
|-- dts/            A76、GICv3、timer、PL011、MHU 和共享内存
|-- initramfs/      最小 PID 1，加载驱动并进入手工命令模式
|-- scripts/        initramfs 和 VP payload 打包
|-- kernel.fragment Linux 所需配置
`-- Makefile
```

Linux host 侧 driver/runtime 源码位于
`runtime_demo/`。`linux_a76/Makefile` 只负责 Linux boot payload
打包，会从 host 侧目录编译并拷贝 `r52_gpu.ko` 和 `launch_kernel`。

## 地址布局

VP 将 `linux_payload.bin` 加载到 `0x80100000`：

| 物理地址 | 内容 |
|---|---|
| `0x80100000` | boot shim |
| `0x80110000` | DTB |
| `0x80200000` | arm64 Linux `Image` |
| `0x84100000` | initramfs，最大 16 MB |
| `0x90000000` | Linux/R52 non-cacheable IPC，1 MB |

boot shim 按 arm64 boot protocol 设置 `x0=DTB`，然后进入 Linux `Image`。

## 依赖

Ubuntu 主机安装：

```sh
sudo apt-get install gcc-aarch64-linux-gnu device-tree-compiler cpio
```

需要一份 Linux 6.8 arm64 源码。主机运行内核版本和目标内核可以相同，但
目标 `Image` 和 `.ko` 必须用 `ARCH=arm64` 构建。

## 构建

### 推荐流程

第一次使用时先配置一次 arm64 Linux kernel：

```sh
cd /your/path/ts_test
make -C linux_a76 KERNEL_SRC=/your/path/linux-6.8 kernel-config
```

之后如果只是修改 R52 firmware、Linux driver 或 runtime，可以用下面的脚本
一键重新构建并启动仿真：

```sh
cd /your/path/ts_test
./linux_a76/scripts/run_linux_demo.sh /your/path/linux-6.8
```

默认是手工模式。进入 guest Linux 的 `linux_a76#` 提示符后，手工执行：

```sh
/bin/launch_kernel
```

当前设计不会在 guest 启动后自动执行 `/bin/launch_kernel`。这样可以在
`r52_gpu.ko` 已加载、`/dev/r52_gpu` 已创建后，手工替换测试步骤或重复执行
runtime 程序。

也可以通过环境变量指定 kernel 源码路径：

```sh
KERNEL_SRC=/your/path/linux-6.8 ./linux_a76/scripts/run_linux_demo.sh
```

如果希望恢复之前的自动流程，也就是 guest 启动后无需手工输入
`/bin/launch_kernel`，构建时打开 `INIT_AUTORUN=1`：

```sh
cd /your/path/ts_test
INIT_AUTORUN=1 ./linux_a76/scripts/run_linux_demo.sh /your/path/linux-6.8
```

自动模式会执行：

1. 编译 R52 `task_scheduler`。
2. 构建 Linux `Image`、`r52_gpu.ko`、`launch_kernel`、initramfs 和 payload。
3. 启动 VP。
4. guest `/init` 加载 `r52_gpu.ko`。
5. 等待 `/dev/r52_gpu` 创建。
6. 自动执行一次 `/bin/launch_kernel`。
7. 执行完成后仍进入 `linux_a76#`，方便继续手工调试。

### 分步执行

先编译不依赖内核源码的 boot、DTB 和静态用户程序：

```sh
cd /your/path/ts_test/linux_a76
make tools
```

配置 arm64 内核。`KERNEL_SRC` 是源码目录，构建结果默认放在
`linux_a76/build/kernel`：

```sh
make KERNEL_SRC=/path/to/linux-6.8 kernel-config
```

检查 `.config` 至少包含 `kernel.fragment` 中的选项，然后生成 Image、模块、
initramfs 和最终 payload：

```sh
make KERNEL_SRC=/path/to/linux-6.8 all
```

分步构建时也可以打开自动执行模式：

```sh
make KERNEL_SRC=/path/to/linux-6.8 INIT_AUTORUN=1 all
../simulation/run_linux.sh
```

默认运行程序会生成到：

```text
linux_a76/bins/launch_kernel
```

它会被 initramfs 打包成 guest Linux 里的：

```text
/bin/launch_kernel
```

如果要替换默认运行程序，可以直接把外部编译好的 arm64 static binary 放到：

```sh
cp /absolute/path/to/your/aarch64_static_binary \
   /your/path/ts_test/linux_a76/bins/launch_kernel

make KERNEL_SRC=/path/to/linux-6.8 initramfs payload
```

也可以不覆盖 `bins/launch_kernel`，临时使用 `LAUNCHER_IMAGE` 指定一个外部
文件打包为 guest 里的 `/bin/launch_kernel`：

```sh
make KERNEL_SRC=/path/to/linux-6.8 \
     LAUNCHER_IMAGE=/absolute/path/to/your/aarch64_static_binary \
     initramfs payload
```

要求：

- 必须是 arm64 Linux 用户态程序。
- 当前 initramfs 没有动态链接器和 libc 文件，建议使用 static link。
- guest 里仍然手工执行 `/bin/launch_kernel`。

如果要额外导入多个文件，可以准备一个 overlay 目录：

```text
overlay/
`-- bin/
    `-- my_test
```

然后构建：

```sh
make KERNEL_SRC=/path/to/linux-6.8 \
     INITRAMFS_OVERLAY=/absolute/path/to/overlay \
     initramfs payload
```

如果已经有独立的 arm64 kernel build 目录：

```sh
make KERNEL_SRC=/path/to/linux-source \
     KERNEL_BUILD=/path/to/arm64-build \
     KERNEL_IMAGE=/path/to/arm64-build/arch/arm64/boot/Image \
     all
```

最终生成：

```text
linux_a76/build/linux_payload.bin
```

## 运行

R52 固件增加了 kernel-completion doorbell，因此需要重新编译：

```sh
cd /your/path/ts_test
make -C task_scheduler clean all
./simulation/run_linux.sh
```

initramfs 会自动执行：

1. 挂载 devtmpfs、proc 和 sysfs。
2. 加载 `r52_gpu.ko`。
3. 等待 `/dev/r52_gpu` 出现。
4. 进入 `linux_a76#` 手工命令模式。

看到提示符后手工执行：

```sh
/bin/launch_kernel
```

`launch_kernel` 会执行：

1. 打开 `/dev/r52_gpu`。
2. 通过 `GET_RESOURCES + mmap` 获取 shared memory 和 A76→R52 send fastchannel window。
3. driver 通过内部 FIFO/control fastchannel 查询 R52 GPU 信息。
4. driver 创建 process/queue；runtime 填 shared AQL dispatch packet。
5. runtime 写 mmap 后的 A76→R52 send fastchannel 触发 queue。
6. Linux driver 通过 IRQ 接收 R52→A76 completion event，并用 `poll()` 唤醒 runtime。
7. 打印 R52 模拟执行时间。

预期用户态输出类似：

```text
R52 GPU 1234:5678, 3 SM, ABI 1
sequence=1 completed in 456 us
```

## Driver/runtime API

共享 UAPI 位于 `runtime_demo/driver/r52_gpu_uapi.h`：

| ioctl | 用途 |
|---|---|
| `R52_GPU_IOC_GET_RESOURCES` | 获取 ABI、shared memory DMA 地址和 MHU mmap region size |
| `R52_GPU_IOC_QUERY` | 通过 MHU 查询 R52 hardware information |
| `R52_GPU_IOC_SUBMIT` | 确保 queue 已创建并返回 sequence；AQL packet 由 userspace 填 shared queue |
| `R52_GPU_IOC_GET_COMPLETION` | 读取完成 sequence 和 R52 时间戳 |

当前推荐路径是 runtime 使用 `R52_GPU_IOC_GET_RESOURCES`，然后 `mmap()`
shared memory 和 PBX fastchannel window。FIFO control path、R52→A76
fastchannel/doorbell 接收都留在 kernel driver；userspace 只直接写
A76→R52 send fastchannel 来触发已创建的 queue。

## 当前边界

- A76 只有一个 CPU，因此不需要 PSCI/SMP bring-up。
- 没有 PCI root complex，只能使用 platform driver，不能 probe PCIe GPU。
- MHU 和 GPU client 暂时在同一个模块中，后续可拆成 Linux mailbox controller
  与 DRM driver。
- 共享队列目前限制单进程、单队列、单个 in-flight dispatch。
- CModel 没有真实 GPU execution model，R52 只模拟 dispatch 时间。
- VP 的 SystemC/QBox 运行库是预编译文件；如果 CPU 或 GIC 插件的端口定义与
  配置不一致，需要对应的模型 SDK 才能修改插件本身。
