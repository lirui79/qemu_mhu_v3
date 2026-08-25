# A76 Linux host driver/runtime

这个目录是 Linux 版本的 A76 host 侧实现。设计目标是尽量复用原
`host_demo` 的 MHU 编程模型：

```text
launch_kernel
  -> r52_runtime
  -> mmap(/dev/r52_gpu)
  -> userspace 直接访问 MHU PBX/MBX MMIO
  -> R52 command processor + AQL scheduler
```

`r52_gpu.ko` 是 platform driver，只负责：

- 根据 device tree 绑定 A76 侧 MHU PBX/MBX resource。
- 分配一页 DMA coherent shared memory，用于 AQL packet/signal。
- 通过 `/dev/r52_gpu` 把 shared memory、PBX、MBX mmap 给 userspace。
- 在 probe 阶段向 R52 发送 host-startup doorbell。

runtime 负责：

- 通过 mmap 后的 PBX FIFO 发送 `QUERY/CREATE_PROCESS/CREATE_QUEUE`。
- 通过 mmap 后的 MBX FIFO 读取 R52 response。
- 在 shared memory 中构造 AQL kernel dispatch packet。
- 通过 mmap 后的 PBX doorbell 触发 queue doorbell。
- 轮询 mmap 后的 MBX doorbell status bit31 或 shared signal 完成状态。

## mmap ABI

| mmap offset | 内容 |
|---|---|
| `R52_GPU_MMAP_SHARED` / `0x00000` | DMA coherent shared memory |
| `R52_GPU_MMAP_PBX` / `0x10000` | A76 -> R52 MHU postbox MMIO |
| `R52_GPU_MMAP_MBX` / `0x20000` | R52 -> A76 MHU mailbox MMIO |

先用 `R52_GPU_IOC_GET_RESOURCES` 获取 shared DMA 地址和 region size。这个
DMA 地址会填入 `CREATE_QUEUE`，供 R52 访问 AQL packet、read/write pointer
和 completion signal。

## 构建

```sh
cd /your/path/ts_test/runtime_demo
make KERNEL_BUILD=/your/path/ts_test/linux_a76/build/kernel
```

产物：

```text
runtime_demo/driver/r52_gpu.ko
runtime_demo/build/launch_kernel
```

完整 Linux payload 仍由 `linux_a76/Makefile` 打包；该 Makefile 会从本目录
取 `r52_gpu.ko` 和 `launch_kernel` 源码。
