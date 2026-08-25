# 中断映射汇总（DMA / UART / MHU）

来源：SoC 中断表。GIC 约定：`INT_ID = 32 + SPI 编号`（SPI 从 0 起）。

| 范围 | 类型 | 说明 |
|------|------|------|
| INT_ID 0–15 | SGI | 软件中断 |
| INT_ID 16–31 | PPI | 核私有硬件中断 |
| INT_ID 32+ | SPI | 共享外设中断 |

---

## 1. DMA（`ts_dma`）

| INT_ID | SPI | 信号名 | 含义 |
|--------|-----|--------|------|
| 32 | SPI[0] | `dma_intr` | 任意通道或公共寄存器有中断时置位（汇总） |
| 33–48 | SPI[1]–SPI[16] | `dma_intr_ch[0]`–`dma_intr_ch[15]` | 通道 0–15 独立中断（共 16 路） |
| 49 | SPI[17] | `dma_intr_cmnreq` | 公共寄存器中断 |

**小结：** DMA 占用 SPI[0]–SPI[17]（INT_ID 32–49）。常用汇总线为 **SPI[0] / INT_ID 32**（`dma_intr`）。

---

## 2. 串口 UART（`ts_uart0` / `ts_uart1`）

| INT_ID | SPI | 信号名 | 模块 | 含义 |
|--------|-----|--------|------|------|
| 50 | SPI[18] | `uart0_intr` | `ts_uart0` | UART0 中断，原因读 IIR |
| 51 | SPI[19] | `uart1_intr` | `ts_uart1` | UART1 中断，原因读 IIR |

**小结：** 两路串口各一根 SPI；中断源需读各自 IIR。

---

## 3. MHU

MHU 分为发送侧 Postbox（`ts_mhus`）与接收侧 Mailbox（`ts_mhur`），各约 4 通道。

### 3.1 MHU Sender / Postbox（`ts_mhus`）— INT_ID 52–78

| INT_ID | SPI | 信号名 | 含义 |
|--------|-----|--------|------|
| 52–55 | SPI[20]–SPI[23] | `mhus_db_ack_int` | Doorbell 应答中断，每通道一根（×4） |
| 56–59 | SPI[24]–SPI[27] | `mhus_fifo_ack_int` | FIFO channel 应答中断，每通道一根（×4） |
| 60–63 | SPI[28]–SPI[31] | `mhus_fifo_low_int` | 发送 FIFO 低水线中断（×4） |
| 64–67 | SPI[32]–SPI[35] | `mhus_fifo_high_int` | 发送 FIFO 高水线中断（×4） |
| 68–71 | SPI[36]–SPI[39] | `mhus_fifo_flush_int` | Flush 请求中断（×4） |
| 72–75 | SPI[40]–SPI[43] | `mhus_fifo_int` | ack/low/high/flush 的或中断（×4） |
| 76 | SPI[44] | `mhus_err_int` | 错误中断 |
| 77 | SPI[45] | `mhus_fault_int` | 故障中断 |
| 78 | SPI[46] | `mhus_pbx_int` | Postbox 组合中断 |

### 3.2 MHU Receiver / Mailbox（`ts_mhur`）— INT_ID 79–110

| INT_ID | SPI | 信号名 | 含义 |
|--------|-----|--------|------|
| 79–82 | SPI[47]–SPI[50] | `mhur_db_tfr_int` | Doorbell 传输中断，每通道一根（×4） |
| 83 | SPI[51] | `mhur_fst_tfr_int` | Fastchannel 通道中断（128 路汇聚成一根） |
| 84–87 | SPI[52]–SPI[55] | `mhur_fst_tfr_grp_int` | Fast channel group 通道中断（×4） |
| 88–91 | SPI[56]–SPI[59] | `mhur_fifo_tfr_int` | FIFO channel 传输中断（×4） |
| 92–95 | SPI[60]–SPI[63] | `mhur_fifo_low_int` | 接收 FIFO 低水线中断（×4） |
| 96–99 | SPI[64]–SPI[67] | `mhur_fifo_high_int` | 接收 FIFO 高水线中断（×4） |
| 100–103 | SPI[68]–SPI[71] | `mhur_fifo_flush_int` | Flush 中断（×4） |
| 104–107 | SPI[72]–SPI[75] | `mhur_fifo_int` | ack/low/high/flush 的或中断（×4） |
| 108 | SPI[76] | `mhur_err_int` | 错误中断 |
| 109 | SPI[77] | `mhur_fault_int` | 故障中断 |
| 110 | SPI[78] | `mhur_mbx_int` | Mailbox 组合中断 |

### 3.3 MHU 小结

| 侧 | 模块 | SPI 范围 | INT_ID | 组合中断（常用） |
|----|------|----------|--------|------------------|
| 发送 (PBX) | `ts_mhus` | SPI[20]–SPI[46] | 52–78 | `mhus_pbx_int` = SPI[46] / INT_ID 78 |
| 接收 (MBX) | `ts_mhur` | SPI[47]–SPI[78] | 79–110 | `mhur_mbx_int` = SPI[78] / INT_ID 110 |

- Doorbell：每通道独立线；接收侧另有 Fastchannel / Fastchannel-group。
- FIFO：发送侧为 TX 水线，接收侧为 RX 水线；另有 flush 与或中断。
- 组合中断：`mhus_pbx_int` / `mhur_mbx_int` 适合作为软件默认入口。

### 3.4 MHU 地址映射（SoC）

MHU 在系统地址空间中连续占用 `0x2FC0_0000`–`0x2FC5_FFFF`，共 **384 KB**，按发送 / 接收各拆成 **data** 与 **reg** 两窗：

| 接口 | 起始 | 结束 | 大小 | 说明 |
|------|------|------|------|------|
| `master_if_mhu_snd_data` | `0x2FC0_0000` | `0x2FC0_FFFF` | 64 KB | 发送侧数据窗 |
| `master_if_mhu_snd_reg` | `0x2FC1_0000` | `0x2FC2_FFFF` | 128 KB | 发送侧寄存器窗（Postbox / 控制） |
| `master_if_mhu_rec_data` | `0x2FC3_0000` | `0x2FC3_FFFF` | 64 KB | 接收侧数据窗 |
| `master_if_mhu_rec_reg` | `0x2FC4_0000` | `0x2FC5_FFFF` | 128 KB | 接收侧寄存器窗（Mailbox / 中断状态与清除） |

布局要点：

- 发送侧：`0x2FC0_0000` 起，先 64 KB data，再紧跟 128 KB reg。
- 接收侧：`0x2FC3_0000` 起，同样 data（64 KB）+ reg（128 KB）。
- 中断相关控制/状态寄存器落在 **reg** 窗；组合中断入口仍为 `mhus_pbx_int` / `mhur_mbx_int`。

**本平台 VP 落址（`conf.lua`）：** `*_reg` 各 128 KB 拆成两个 64 KiB PBX/MBX 帧：

| 角色 | 基址 | SoC 窗口 |
|------|------|----------|
| R52 PBX | `0x2FC1_0000` | `snd_reg` 前半 |
| A76 PBX | `0x2FC2_0000` | `snd_reg` 后半 |
| R52 MBX | `0x2FC4_0000` | `rec_reg` 前半 |
| A76 MBX | `0x2FC5_0000` | `rec_reg` 后半 |

`snd_data` / `rec_data` 由 `mhu_davarae` 模型直接映射（非独立 `gs_memory`）：

- **PAY 流式**：写 `PFFCW_PAY` → 对端 FIFO；读 `MFFCW_PAY`（`RA_EN`）弹出。
- **KICK_DATA 批量**：软件填 `snd_data[off..]`，写 `DATA_OFF/LEN` + `CTRL.KICK_DATA` → 推入对端 FIFO，并镜像到 `rec_data[off..]`。
- FIFO 窗在寄存器帧 `+0x2000`（每通道 64B，×4）；水位/ack 并入现有 `mhus_pbx_int` / `mhur_mbx_int` 组合中断。

---

## 4. 本平台 VP 接线（与 SoC 表对照）

固件 / `conf.lua` 当前简化接线（非全量 SoC 映射）：

| 用途 | VP SPI | INT_ID | 对应 SoC 信号 |
|------|--------|--------|---------------|
| DMA 汇总 | SPI[0] | 32 | `dma_intr`（与 SoC 一致） |
| UART（PL011） | SPI[1] | 33 | SoC 上为 SPI[18]/`uart0_intr`，VP 重映射 |
| IRQ test | SPI[2] | 34 | VP 专用 |
| MHU Mailbox 组合 | SPI[78] | 110 | `mhur_mbx_int`（与 SoC 一致） |

R52 / A76 两侧均将 MHU MBX 组合中断接到 **SPI[78] / INT_ID 110**。
