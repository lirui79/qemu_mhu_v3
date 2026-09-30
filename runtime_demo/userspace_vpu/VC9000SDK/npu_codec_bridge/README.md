# npu_codec_bridge

<div align="center">

**VastAI NPU 硬件解码器桥接层 —— 一行代码，为 torchcodec 注入 NPU 加速**

[![Python](https://img.shields.io/badge/Python-3.9%2B-blue)](https://python.org)
[![PyTorch](https://img.shields.io/badge/PyTorch-2.0%2B-ee4c2c)](https://pytorch.org)
[![torchcodec](https://img.shields.io/badge/torchcodec-0.11%2B-green)](https://github.com/pytorch/torchcodec)
[![License](https://img.shields.io/badge/License-BSD--2--Clause-orange)](LICENSE)

</div>

---

## 📖 项目概述

`npu_codec_bridge` 是一个 Python/C++ 混合桥接层，通过 **Monkey Patching** 技术将 VastAI NPU 硬件视频解码能力无缝注入 Meta 的 [torchcodec](https://github.com/pytorch/torchcodec) 库。

**对用户而言，只需在代码顶部添加一行 `import npu_codec_bridge`，然后将 `device='npu'` 传入 `VideoDecoder` 构造函数，即可获得 NPU 硬件加速，其余 torchcodec API 完全不变。**

```
┌──────────────────────────────────────────────────────────────┐
│                                                              │
│   用户代码                     底层实现                        │
│                                                              │
│   import npu_codec_bridge      ① dlopen libvaccrt.so         │
│       │                        ② open /dev/vastai_videoX     │
│       ▼                        ③ vmppDecSendStream           │
│   dec = VideoDecoder(          ④ vmppDecReceiveFrame         │
│       "video.mp4",             ⑤ NV12 → RGB 转换              │
│       device='npu'   ←──→     ⑥ torch::from_blob (零拷贝)     │
│   )                                                          │
│   frame = dec[0]                                              │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## ✨ 核心特性

| 特性 | 说明 |
|------|------|
| 🔌 **一行集成** | `import npu_codec_bridge` — 零侵入，torchcodec 源码无修改 |
| 🧠 **100% API 兼容** | `get_frame_at()`、`get_frames_at()`、`metadata`、切片索引等全部支持 |
| ⚡ **硬件加速** | NPU 硬解码 + DMA 传输，相比 CPU 软解码吞吐量大幅提升 |
| 🔄 **零拷贝 Tensor** | `torch::from_blob` 直接包装 NPU 输出内存，无中间拷贝 |
| 🧵 **异步解码** | 并行 SendStream / ReceiveFrame，输入输出双线程解耦 |
| 📦 **PyTorch DataLoader 就绪** | 提供 `NPUDecoderVideoDataset`，可直接接入训练流水线 |
| 🎯 **NV12 → RGB 转换** | 内置纯 torch 实现的色彩空间转换，可微分、可 GPU 加速 |
| 🛡️ **STUB 模式** | 无需 NPU 硬件即可开发和测试 API 集成 |

---

## 📋 环境要求

| 依赖 | 版本 | 说明 |
|------|------|------|
| **VastAI NPU SDK** | `libvmpp-dec.so` + `libvaccrt.so` | 硬件解码器运行时库 |
| **NPU 驱动** | PCIe 驱动 + VDMCU 固件 | 设备节点 `/dev/vastai_video*` |
| **PyTorch** | ≥ 2.0.0 | 含 C++ API (`libtorch`) |
| **torchcodec** | ≥ 0.1.0 | Meta 官方视频解码库 |
| **Python** | ≥ 3.9 | |
| **CMake** | ≥ 3.18 | C++ 扩展编译 |
| **GCC** | ≥ 9.0 | C++17 支持 |
| **pybind11** | ≥ 2.11.0 | 仅编译时依赖 |

### 验证环境

```bash
# 检查 NPU 设备
ls /dev/vastai_video*

# 检查 SDK 库
ldconfig -p | grep -E "libvmpp|libvaccrt"

# 检查 Python 依赖
python3 -c "import torch; print('torch', torch.__version__)"
python3 -c "import torchcodec; print('torchcodec', torchcodec.__version__)"
```

---

## 🔧 安装

### 方式一：开发/测试模式（STUB 模式，无需 NPU 硬件）

```bash
cd npu_codec_bridge
pip install --no-build-isolation -e .
```

此模式编译一个 mock C++ 解码器，返回随机生成的 NV12 帧。所有 Python API 均可正常调用，适合：
- 集成测试
- CI/CD 流水线
- API 原型开发

### 方式二：生产模式（LIVE 模式，需要 NPU 硬件）

```bash
cd npu_codec_bridge
NPU_LIVE_MODE=1 pip install --no-build-isolation -e .
```

> **注意：** 必须使用 `--no-build-isolation` 标志，因为 PyTorch 和 pybind11 在构建时需要被 CMake 找到，但它们不在 pip 的隔离构建环境中。`--no-build-isolation` 告诉 pip 使用当前环境中已安装的包。

此模式链接真实的 VastAI NPU SDK (`libvmpp-dec.so`, `libvaccrt.so`)，直接操作硬件。

### 验证安装

```bash
python3 -c "
from npu_codec_bridge._C import NPUDecoder, DecoderConfig
print('Mode:', __import__('npu_codec_bridge._C').__mode__)
print('Version:', __import__('npu_codec_bridge._C').__version__)
"
# STUB 模式输出: Mode: stub, Version: 0.1.0-stub
# LIVE 模式输出: Mode: live, Version: 0.1.0-live
```

---

## 🚀 快速开始

```python
# ============================================================
# 步骤 1: 导入 npu_codec_bridge (必须在 torchcodec 之前)
# ============================================================
import npu_codec_bridge          # ← 这一行触发魔法

# ============================================================
# 步骤 2: 像往常一样使用 torchcodec
# ============================================================
from torchcodec.decoders import VideoDecoder

# NPU 硬件解码
dec = VideoDecoder("video.mp4", device="npu")

# CPU 解码（原始路径，不受影响）
# dec = VideoDecoder("video.mp4", device="cpu")

# ============================================================
# 步骤 3: 使用标准 torchcodec API
# ============================================================
# 元数据
print(dec.metadata.width, dec.metadata.height)
print(dec.metadata.average_fps, dec.metadata.codec)

# 单帧 (返回 torch.Tensor [C, H, W] uint8)
first_frame = dec[0]
mid_frame   = dec.get_frame_at(100)
time_frame  = dec.get_frame_played_at(3.5)

# 批量帧
batch = dec.get_frames_at([10, 20, 30])  # FrameBatch
frames = dec.get_frames_in_range(0, 100, step=2)
seg = dec.get_frames_played_in_range(2.0, 5.0)

# 切片
clip = dec[50:150]              # → Tensor[N, C, H, W]
```

### DataLoader 集成（训练流水线）

```python
from npu_codec_bridge import NPUDecoderVideoDataset
from torch.utils.data import DataLoader

dataset = NPUDecoderVideoDataset(
    "train_clip.mp4",
    device="npu",
    transforms=my_transforms,    # 可选的标准化/增强
)
loader = DataLoader(dataset, batch_size=32, num_workers=0)

for batch in loader:
    frames = batch["data"]       # [B, C, H, W]
    metas  = batch["meta"]       # 每帧的索引和时间戳
    output = model(frames)
```

---

## 🖥️ 示例运行

项目提供了三个完整的示例脚本，位于 `samples/` 目录下。

### 01_basic_usage.py — 基础解码 + 帧保存

```bash
python samples/01_basic_usage.py /tmp/test_crossval.mp4 --frame 0
```

**运行输出：**

```
Video: /tmp/test_crossval.mp4
Frame index: 0

────────────────────────────────────────────────────────────
  NPU Hardware Decoder (device='npu')
────────────────────────────────────────────────────────────
  Codec      : h264
  Resolution : 1920 x 1080
  FPS        : 30.00
  Duration   : 1.13s
  Frames     : 34

  Decoding frame 0...
  Frame data:
    shape          : [3, 1080, 1920]  (C, H, W)
    dtype          : torch.uint8
    device         : cpu
    pts_seconds    : 0.0000
    duration_seconds: 0.0333
    pixel min/mean/max: 0 / 113.3 / 255
  Saved: frame_npu.png  (5,587,249 bytes)

────────────────────────────────────────────────────────────
  CPU Decoder (device='cpu') — for comparison
────────────────────────────────────────────────────────────
  Codec      : hevc
  Resolution : 1920 x 1080
  FPS        : 25.00
  Frame 0: shape [3, 1080, 1920], dtype torch.uint8
  Saved: frame_cpu.png  (1,115,170 bytes)
```

### 02_performance_benchmark.py — CPU vs NPU 性能基准

```bash
python samples/02_performance_benchmark.py /tmp/test_crossval.mp4 --frames 20
```

**运行输出：**

```
============================================================
  Decoding Performance Benchmark
============================================================
  Video:   /tmp/test_crossval.mp4
  Frames:  20
  Mode:    STUB

[1/2] Benchmarking NPU decoder (device='npu')...
      Resolution: 1920x1080, 34 frames @ 30.0 fps
  ──────────────────────────────────────────────────
  NPU (VastAI)
  ──────────────────────────────────────────────────
    Frames decoded  : 20
    Wall time       : 0.781s
    Throughput      : 25.6 fps
    Latency (avg)   : 39059 µs
    Latency (p50)   : 41293 µs
    Latency (p95)   : 43766 µs
    Latency (p99)   : 43766 µs
    Latency (min)   : 6 µs
    Latency (max)   : 43766 µs
```

### 03_dataloader_integration.py — PyTorch DataLoader 训练集成

```bash
python samples/03_dataloader_integration.py /tmp/test_crossval.mp4 --num-frames 12 --batch-size 4
```

**运行输出：**

```
============================================================
  NPU-Accelerated Video DataLoader Demo
============================================================
  Video:    /tmp/test_crossval.mp4
  Mode:     STUB (bridge overhead only)
  Device:   npu
  Batch:    4
  Frames:   12  (stride 1)

[1] Creating NPUDecoderVideoDataset...
    NPUDecoderVideoDataset(video='test_crossval.mp4',
      frames=[0:12:1], n=12, device='npu', shuffle=False)
    Source: 1920x1080, 30.0fps, h264

[2] Creating DataLoader (batch_size=4)...

[3] Iterating through DataLoader (simulating training epoch)...

    Batch   1: data=[4, 3, 224, 224], range=-2.118..2.640, indices=[0, 1, 2, 3]
    Batch   2: data=[4, 3, 224, 224], range=-2.118..2.640, indices=[4, 5, 6, 7]
    Batch   3: data=[4, 3, 224, 224], range=-2.118..2.640, indices=[8, 9, 10, 11]

============================================================
  Pipeline Summary
============================================================
  Batches        : 3
  Total frames   : 12
  Elapsed        : 0.618s
  Throughput     : 19.4 fps
  Avg/batch      : 205.9 ms
```

---

## ⚙️ 构建配置参数

所有硬编码路径已替换为可配置的 CMake 变量。通过环境变量或 CMake 参数传入：

### CMake 参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `NPU_LIVE_MODE` | `OFF` | `ON`=真实 NPU SDK，`OFF`=Mock STUB |
| `NPU_SDK_ROOT` | `${CMAKE_SOURCE_DIR}/..` | VastAI SDK 根路径 (`vastai_video_sdk`) |
| `CMAKE_PREFIX_PATH` | *自动检测* | Torch CMake 配置路径 |
| `LIBDRM_INCLUDE_DIR` | *自动检测* | DRM 头文件路径（零拷贝 mmap） |
| `TORCH_LIB_DIR` | *自动检测* | Torch C++ 库路径（含 `libtorch_python.so`） |

### 使用方式

```bash
# 基本安装（STUB 模式）
pip install --no-build-isolation -e .

# LIVE 模式 + 自定义 SDK 路径
NPU_LIVE_MODE=1 \
  CMAKE_ARGS="-DNPU_SDK_ROOT=/path/to/vastai_video_sdk" \
  pip install --no-build-isolation -e .

# 自定义 Torch 路径（非标准安装）
CMAKE_PREFIX_PATH=/custom/torch/share/cmake \
  pip install --no-build-isolation -e .

# 完整自定义
NPU_LIVE_MODE=1 \
  CMAKE_ARGS="-DNPU_SDK_ROOT=/opt/vastai/vastai_video_sdk \
              -DCMAKE_PREFIX_PATH=/opt/torch/share/cmake \
              -DLIBDRM_INCLUDE_DIR=/usr/include/libdrm" \
  pip install --no-build-isolation -e .
```

### 自动检测逻辑

CMakeLists.txt 在构建时自动检测以下路径（无需手动配置）：

1. **Torch 库目录** — 遍历 `${TORCH_LIBRARIES}` 查找 `libtorch_python.so` 所在目录，回退到 `find_library`
2. **DRM 头文件** — 搜索 `/usr/include/libdrm`、`/usr/include/drm`
3. **Python 开发头文件** — 通过 `find_package(Python)` 自动获取
4. **pybind11** — 在 STUB 模式下由 `setup.py` 从 Python 环境传入

---

## 🏗️ 架构

### 分层架构

```
┌─────────────────────────────────────────────────────────────┐
│                       Python 层                              │
│                                                              │
│   npu_codec_bridge/__init__.py                               │
│   ├── _install_npu_patch()        Monkey-Patch 注入          │
│   │   └── _PatchedVideoDecoder    拦截 device='npu'          │
│   ├── NPUVideoDecoder             torchcodec API 兼容层      │
│   │   ├── AnnexBStreamReader      码流解析 (Start Code)      │
│   │   ├── _nv12_to_rgb_nchw()    NV12→RGB 色彩转换           │
│   │   └── _decode_up_to()        惰性解码 + 帧缓存           │
│   └── NPUDecoderVideoDataset     PyTorch IterableDataset     │
│                                                              │
├─────────────────────────────────────────────────────────────┤
│                     C++ 桥接层 (pybind11)                     │
│                                                              │
│   src/pybind_module.cpp                                      │
│   ├── PyNPUDecoder                Python 绑定                │
│   │   ├── decode()               → vmppDecSendStream         │
│   │   ├── get_frame()            → vmppDecReceiveFrame       │
│   │   ├── release_frame()        → vmppDecReleaseFrame       │
│   │   └── flush/reset/probe      补充操作                    │
│   ├── StubNPUDecoder (STUB)      Mock 实现，无硬件依赖        │
│   └── LiveNPUDecoder (LIVE)      真实 NPU SDK 封装            │
│                                                              │
├─────────────────────────────────────────────────────────────┤
│                     VastAI NPU SDK                           │
│                                                              │
│   libvmpp-dec.so                低层解码 API                 │
│   ├── vmppDecSendStream()        推送压缩码流                │
│   ├── vmppDecReceiveFrame()      拉取解码帧                  │
│   ├── vmppDecReleaseFrame()      归还 Buffer                 │
│   └── vmppDecTransferFrame()     DEVICE→HOST 传输            │
│                                                              │
│   libvaccrt.so                  NPU 运行时                   │
│   ├── vaccrt_init()             芯片初始化                   │
│   ├── vaccrt_malloc64()         分配设备内存                 │
│   └── vaccrt_free64()           释放设备内存                 │
│                                                              │
├─────────────────────────────────────────────────────────────┤
│                       NPU 硬件                                │
│                                                              │
│   /dev/vastai_video*             设备节点                     │
│   PCIe 驱动 + VDMCU 固件                                      │
└─────────────────────────────────────────────────────────────┘
```

### Monkey-Patch 工作流程

```
import npu_codec_bridge
  │
  ├── _install_npu_patch()
  │     │
  │     ├── import torchcodec.decoders
  │     ├── 保存原始 VideoDecoder
  │     └── 替换为 _PatchedVideoDecoder
  │
  │
from torchcodec.decoders import VideoDecoder
  │
  │  VideoDecoder 实际指向 _PatchedVideoDecoder
  │
  │
dec = VideoDecoder("video.mp4", device="npu")
  │
  ├── _PatchedVideoDecoder.__init__()
  │     │
  │     ├── device="npu" ?
  │     │   YES → 创建 NPUVideoDecoder (NPU 硬解码)
  │     │   NO  → super().__init__()   (原始 torchcodec)
  │     │
  │     └── 所有方法 (__getitem__, get_frame_at, metadata ...)
  │         通过 self._npu_instance 代理到 NPU 实现
```

### 零拷贝内存路径

```
路径 A (默认): NPU DDR →[SDK DMA]→ Host Buffer → torch::from_blob
              1 次拷贝 (SDK 内部)，tensor 共享 Host Buffer

路径 B (高性能): NPU DDR →[mmap]→ 用户态地址 → torch::from_blob
              0 次拷贝，tensor 直接读取 NPU 物理内存
```

---

## 🔧 故障排查

### `ModuleNotFoundError: No module named 'npu_codec_bridge._C'`

C++ 扩展未编译。需要运行：

```bash
cd npu_codec_bridge
pip install -e .
```

### `ImportError: libvaccrt.so: cannot open shared object file`

NPU 运行时库不在动态链接搜索路径中：

```bash
# 添加到 LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/opt/vastai/vaststream/lib:$LD_LIBRARY_PATH

# 或添加到 ldconfig
echo "/opt/vastai/vaststream/lib" | sudo tee /etc/ld.so.conf.d/vastai.conf
sudo ldconfig

# 验证
ldd npu_codec_bridge/_C*.so | grep vaccrt
```

### `Cannot open DRM render node for device /dev/vastai_video0`

设备节点不存在或权限不足：

```bash
# 检查设备
ls -la /dev/vastai_video*

# 检查驱动
lsmod | grep vastai

# 添加用户到 video 组
sudo usermod -a -G video $USER
# 重新登录后生效

# 临时测试 (root)
sudo python3 -c "open('/dev/vastai_video0', 'rb').close()"
```

### `vmppDecCreateChannel failed` 或 `ERR_NO_BUFFER`

解码器资源耗尽：

```bash
# 检查是否有残留进程
lsof /dev/vastai_video*

# 增加 extra_buffer_number (在 DecoderConfig 中)
config.extra_buffer_number = 32  # 默认 20
```

### `CMake Error: Could not find a package configuration file provided by "Torch"` (LIVE 模式构建失败)

`NPU_LIVE_MODE=1 pip install -e .` 时报错找不到 Torch：

```bash
# ❌ 错误用法 (默认使用构建隔离，Torch 不在隔离环境中)
NPU_LIVE_MODE=1 pip install -e .

# ✅ 正确用法 (使用系统已安装的包)
NPU_LIVE_MODE=1 pip install --no-build-isolation -e .
```

`--no-build-isolation` 标志告诉 pip 不要创建临时虚拟环境，而是使用当前 Python 环境中已安装的 PyTorch 和 pybind11。

### `Device busy` 或 `ERR_ALLOC_CHANNEL`

同一设备节点被其他进程占用：

```bash
# 查看占用进程
sudo fuser -v /dev/vastai_video0

# 终止占用进程
kill -9 <PID>

# 或使用其他 die
config.device_path = "/dev/vastai_video1"
```

### STUB 模式下的预期行为

在 STUB 模式下：
- 解码帧内容是随机生成的（非真实视频内容）—— **图像显示为彩色雪花是正常现象**
- 每次 `decode()` 调用产生一个新随机帧（帧间无时间连贯性）
- MSE 跨验证测试会自动跳过
- CPU-vs-NPU 比较无意义（STUB 数据是随机的）
- 示例脚本 `01_basic_usage.py` 会自动检测 STUB 模式并显示警告

切换到 LIVE 模式需要 NPU 硬件：
```bash
NPU_LIVE_MODE=1 pip install --no-build-isolation -e .
```

### torchcodec 版本兼容性

如果遇到 `AttributeError: property 'stream_index' has no setter` 或类似错误，请使用以下组合：

| torchcodec | 状态 |
|------------|------|
| ≥ 0.10.0 | ✅ 推荐 |
| 0.5.0 – 0.9.x | ✅ 支持 |
| < 0.5.0 | ⚠️ 需测试 |

---

## 📁 项目结构

```
npu_codec_bridge/
├── README.md                          # 本文件
├── pyproject.toml                     # Python 包元数据
├── setup.py                           # 构建脚本 (CMake)
├── CMakeLists.txt                     # C++ 扩展 CMake 配置
├── IMPLEMENTATION_NOTES.md            # 开发者实现笔记
│
├── include/
│   └── npu_decoder_wrapper.h          # C++ RAII 包装器头文件
│
├── src/
│   ├── pybind_module.cpp              # Pybind11 绑定 (STUB + LIVE)
│   └── npu_decoder_zero_copy.cpp      # 零拷贝内存映射实现
│
├── npu_codec_bridge/
│   └── __init__.py                    # Monkey-Patch + NPUVideoDecoder
│
├── samples/
│   ├── 01_basic_usage.py              # 基础用法演示
│   ├── 02_performance_benchmark.py    # CPU vs NPU 性能对比
│   └── 03_dataloader_integration.py   # DataLoader 训练集成
│
├── tests/
│   ├── conftest.py                    # 测试 fixtures
│   └── test_npu_decoder.py            # 70 个测试用例
│
└── demo_monkey_patch.py               # Monkey-Patch 演示
```

## 📊 测试覆盖

```bash
cd npu_codec_bridge
python3 -m pytest tests/ -v
```

70 个测试用例覆盖：Monkey-Patch 注入、基础解码、张量属性、元数据、边界情况、跨验证、并发压力、EOF 处理、快速 Seek 和自一致性。

---

## 🤝 贡献

欢迎提交 Issue 和 Pull Request。大型变更请先开启 Issue 讨论设计思路。

开发模式安装：

```bash
cd npu_codec_bridge
pip install -e ".[dev]"
python3 -m pytest tests/ -v
```
