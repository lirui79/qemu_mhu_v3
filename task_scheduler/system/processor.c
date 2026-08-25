#include <stdio.h>
#include <stdint.h>
#include "system.h"
#include "ts_protocol.h"
#include "mhu_davarae.h"

/* command buffer */
static uint32_t g_cmd_buf[TS_PACKET_LEN_MAX];
static uint32_t g_rsp_buf[TS_PACKET_LEN_MAX];

/* user queue */
static user_queue g_user_queue;

#define TS_CONTROL_FASTCHANNEL       0

/**
 * process event
 */
static void process_event(uint32_t event)
{
    ts_printf("CP: got event 0x%x\n", event);
    if (event == g_user_queue.notify_idx) {
        process_aql_queue(&g_user_queue);
    }
}

/**
 * process command received from fifo
 * cmd_buf points to raw dword buffer (g_cmd_buf), cmd_buf[0] = header
 */
static void process_command(uint32_t *cmd_buf)
{
    uint32_t ret;
    uint32_t code = TS_HDR_CODE(cmd_buf[0]);

    ts_printf("CP: got command 0x%x\n", code);
    switch (code) {
        case TS_CMD_CODE_QUERY: {
            ts_cmd_query *cmd = (ts_cmd_query*)cmd_buf;
            uint8_t qtype = (uint8_t)(cmd->type_param & 0xFF);
            switch (qtype) {
                case TS_QUERY_TYPE_HW: {
                    ts_data_hw_info *hw_info = (ts_data_hw_info*)g_rsp_buf;
                    hw_info->header        = TS_HDR_MAKE(TS_DATA_CODE_HW_INFO, 0,
                                                         TS_PACKET_LEN(ts_data_hw_info));
                    hw_info->vendor_device = (0x5678u << 16) | 0x1234u;
                    hw_info->sm_config     = (32u << 24) | (16u << 16) | (4u << 8) | 3u;
                    hw_info->smem_size     = 256 * 1024;
                    hw_info->l2_cache_size = 0;
                    hw_info->l1_dcache_size = 0;
                    hw_info->l1_icache_size = 0;
                    ts_printf("CP: SENDING HW_INFO rsp (len=%u)...\n",
                              (uint32_t)sizeof(ts_data_hw_info));
                    ret = mhu_send_data(1, hw_info, sizeof(ts_data_hw_info));
                    ts_printf("CP: HW_INFO rsp sent, ret=%u\n", ret);
                    if (ret != sizeof(ts_data_hw_info)) {
                        ts_printf("CP: fail to send rsp\n");
                    }
                    break;
                }
                case TS_QUERY_TYPE_SW: {
                    ts_data_sw_info *sw_info = (ts_data_sw_info*)g_rsp_buf;
                    sw_info->header        = TS_HDR_MAKE(TS_DATA_CODE_SW_INFO, 0,
                                                         TS_PACKET_LEN(ts_data_sw_info));
                    sw_info->ts_fw_version = 0x00010002;
                    ret = mhu_send_data(1, sw_info, sizeof(ts_data_sw_info));
                    if (ret != sizeof(ts_data_sw_info)) {
                        ts_printf("CP: fail to send rsp\n");
                    }
                    break;
                }
                default: {
                    ts_printf("CP: unsupport query 0x%x\n", qtype);
                    break;
                }
            }
            break;
        }
        case TS_CMD_CODE_CREATE_PROCESS: {
            ts_cmd_create_process *cmd = (ts_cmd_create_process*)cmd_buf;
            ts_printf("CP: create process: pasid=0x%x vmid=0x%x\n",
                      (uint16_t)(cmd->pasid_vmid & 0xFFFF),
                      (uint16_t)(cmd->pasid_vmid >> 16));
            break;
        }
        case TS_CMD_CODE_DESTROY_PROCESS: {
            ts_cmd_destroy_process *cmd = (ts_cmd_destroy_process*)cmd_buf;
            ts_printf("CP: destroy process: pasid=0x%x\n",
                      (uint16_t)(cmd->pasid_rsvd & 0xFFFF));
            break;
        }
        case TS_CMD_CODE_CREATE_QUEUE: {
            ts_cmd_create_queue *cmd = (ts_cmd_create_queue*)cmd_buf;
            user_queue *uq = (user_queue*)&g_user_queue;
            uq->queue_id     = (uint16_t)(cmd->queue_pasid & 0xFFFF);
            uq->pasid        = (uint16_t)(cmd->queue_pasid >> 16);
            uq->type         = (uint8_t)(cmd->type_pri_db & 0xFF);
            uq->priority     = (uint8_t)((cmd->type_pri_db >> 8) & 0xFF);
            uq->notify_idx   = (uint16_t)(cmd->type_pri_db >> 16);
            uq->ringbuf_base = ((uint64_t)cmd->ringbuf_base_hi << 32) |
                                (uint64_t)cmd->ringbuf_base_lo;
            uq->ringbuf_size = cmd->ringbuf_size;
            uq->rptr = ((uint64_t)cmd->rptr_hi << 32) | (uint64_t)cmd->rptr_lo;
            uq->wptr = ((uint64_t)cmd->wptr_hi << 32) | (uint64_t)cmd->wptr_lo;
            ts_printf("CP: create queue: queue_id 0x%x pasid 0x%x type %u pri %u notify %u\n",
                      uq->queue_id, uq->pasid, uq->type, uq->priority, uq->notify_idx);
            ts_printf("  : ringbuf 0x%X (%u) rptr 0x%X wptr 0x%x\n",
                      uq->ringbuf_base, uq->ringbuf_size, uq->rptr, uq->wptr);
            break;
        }
        case TS_CMD_CODE_DESTROY_QUEUE: {
            ts_cmd_destroy_queue *cmd = (ts_cmd_destroy_queue*)cmd_buf;
            ts_printf("CP: destroy queue: queue_id 0x%x pasid 0x%x\n",
                      (uint16_t)(cmd->queue_pasid & 0xFFFF),
                      (uint16_t)(cmd->queue_pasid >> 16));
            break;
        }
        default:
            ts_printf("CP: unsupport cmd 0x%x\n", code);
            break;
    }
}

static void drain_command_fifo(void)
{
    volatile uint32_t* rec = (volatile uint32_t*)((uint32_t)MHU_REC_DATA);
    uint32_t  fc_val, off, dwlen, total_len;
    uint32_t  i;

    while (1) {
        /* 原子消费 FC0 通知:取元数据、清 fc_stat bit0、清数据 全部在
         * 关中断下完成。禁止「处理完再无条件 g_mbx_fc_data_0[0]=0」:
         * A76 收到 FC0 ACK 后立即发下一命令,其 FC 边沿可能在本包
         * ACK 打印完成前到达,ISR 刚把新命令元数据存入 g_mbx_fc_data_0[0]
         * 就被无条件清 0 覆盖 → 窗口有数据但 fc=0 → drain 无法解析、
         * A76 等不到 FC0 ACK 死锁(实测 SW query 场景)。现在仅在
         * fc_stat bit0 置位时取-清,关中断保证与 ISR 保存互斥;ISR 在
         * 取-清之后保存的新包会重新置位 bit0,由下一轮循环处理。 */
        {
            uint32_t flags = arch_local_irq_save();
            uint32_t ev0   = g_mbx_fc_stat_0 & (1u << TS_CONTROL_FASTCHANNEL);
            fc_val = 0;
            if (ev0) {
                fc_val = g_mbx_fc_data_0[0];
                g_mbx_fc_stat_0 &= ~(1u << TS_CONTROL_FASTCHANNEL);
                g_mbx_fc_data_0[0] = 0;
            }
            arch_local_irq_restore(flags);
        }

        off    = (fc_val >> 16) & 0xFFFFu;
        dwlen  = fc_val & 0xFFFFu;
        total_len = dwlen * 4;

        if (!dwlen)
            break;   /* 无待处理包(仅 FF 通知先到、FC 元数据未到等) */

        ts_printf("DRAIN: fc=0x%x off=%u dwlen=%u, rec[0]=0x%x rec[1]=0x%x\n",
                  fc_val, off, dwlen,
                  rec[(off / 4)], rec[(off / 4) + 1]);

        /* minimum packet: 1 dword header */
        if (total_len < sizeof(uint32_t)) {
            ts_printf("CP: packet too small: %u\n", total_len);
            break;
        }

        /* Wait for KICK_DATA DMA to complete: FC may arrive before data transfer.
         * Poll the first word at the target offset until it becomes non-zero. */
        {
            volatile uint32_t *wp = &rec[(off / 4)];
            int timeout = 1000000;
            while (*wp == 0 && timeout-- > 0) {
                __asm volatile ("dmb sy" ::: "memory");
            }
            if (timeout <= 0) {
                ts_printf("CP: data timeout at off=%u, skip stale FC\n", off);
                /* 数据未按时到达:放弃本包并退出,避免无条件清 0 误杀
                 * ISR 可能已保存的新命令元数据。ff_stat 由本函数末尾
                 * 统一清除;若 A76 重发,新 FC 边沿会再次唤醒处理。 */
                break;
            }
        }
        __asm volatile ("dmb sy" ::: "memory");

        /* read first raw dword to extract header fields */
        {
            uint32_t raw_hdr = rec[(off / 4)];
            uint32_t code   = raw_hdr & 0xFFu;
            uint32_t flag   = (raw_hdr >> 8) & 0xFFu;
            uint32_t length = (raw_hdr >> 16) & 0xFFFFu;

            ts_printf("DRAIN: raw_hdr=0x%x code=%u flag=%u len=%u\n",
                      raw_hdr, code, flag, length);

            if (!length || (length > TS_PACKET_LEN_MAX)) {
                ts_printf("CP: invalid cmd len: %u\n", length);
                break;
            }

            if (total_len < (length * sizeof(uint32_t))) {
                ts_printf("CP: packet truncated: %u < %u\n", total_len, length * 4);
                break;
            }

            /* copy full packet into cmd buffer as raw dwords */
            for (i = 0; i < length; i++)
                g_cmd_buf[i] = rec[(off / 4) + i];

            /* Consume-acknowledge: data is now safely in the local buffer,
             * clear the shared-window region so the sender can safely send
             * the next packet without overwriting unread data. */
            __asm volatile ("dmb sy" ::: "memory");
            for (i = 0; i < length; i++)
                rec[(off / 4) + i] = 0;
            __asm volatile ("dmb sy" ::: "memory");

            /* Sanity: ensure the first dword in the buffer matches what
             * we parsed from the receive window (may differ if DMA
             * copies lazily, but the poll above guarantees data is present). */
            if (g_cmd_buf[0] != raw_hdr) {
                ts_printf("DRAIN: hdr mismatch buf[0]=0x%x raw=0x%x\n",
                          g_cmd_buf[0], raw_hdr);
            }
        }

        /* process command (code extracted from cmd_buf[0] by TS_HDR_CODE) */
        process_command(g_cmd_buf);

        /* Consume-acknowledge via fast channel 0: the sender (A76) waits for
         * this magic before sending the next packet to the same window.
         * The doorbell IRQ/bit-clear on this model are unreliable (bit30
         * latch persists and produces false positives on the A76), while the
         * fast channel is reliable both ways. R52's PBX FC0 (send direction)
         * is a different register frame than its MBX FC0 (recv direction),
         * so this cannot collide with incoming command metas. */
        mhu_send_fast_event(0, MHU_FC0_ACK_VALUE);
        ts_printf("CP: FC0 ACK(0x41434B00) sent\n");

        /* 循环继续:若 ISR 已保存后续命令(bit0 重新置位),则再处理;
         * 不在末尾无条件清 g_mbx_fc_data_0[0](见循环顶部注释)。 */
    }

    ts_printf("DRAIN: done\n");
    /* Atomically clear ff_stat so command_processor can sleep until next
     * KICK_DATA. Disable IRQs to prevent a race: if the ISR sets
     * g_mbx_ff_stat_0 between our read and write, we'd lose the event. */
    {
        unsigned long irq_flags = arch_local_irq_save();
        g_mbx_ff_stat_0 = 0;
        arch_local_irq_restore(irq_flags);
    }
}

/**
 * command processor
 */
void command_processor(void)
{
    uint32_t       i, ret, events, fc_events;
    unsigned int   cpu_id = get_cpu_id();

    ts_printf("CP: started on cpu%u\n", cpu_id);

    while (1) {
        /* R52 的 GIC 不投递 MBX 组合中断(实测收不到任何 IRQ),ISR 不会运行,
         * g_mbx_*_stat_0 仅靠中断永远为 0。因此这里不能靠 WFI 等待,必须直接
         * 轮询 MHU 硬件锁存状态并入软件标志,与 mhu_wait_event 兜 doorbell
         * 竞态的做法一致。 */
        while (!MHU_DB_SIGNALED && !MHU_FF_SIGNALED && !MHU_FC_SIGNALED) {
            /* 必须轮询 MBX 硬件锁存(FC/FF/DB 边沿)并入软件标志:
             * 本平台 MBX 组合中断投递不可靠(实测有时收不到 IRQ),
             * 若注释掉此轮询,command_task 会在此空转死循环,
             * HW query 永远不会被处理,A76 等 FC0 ACK 死锁。
             * ISR 与 poll 双路径已由 mhu_poll_rx/mhu_mbx_isr 的
             * v!=0 防御 + drain 原子取-清消除双重处理竞态。 */
            mhu_poll_rx();
            arch_delay_us(10);
        }

        events = 0;
        fc_events = 0;
        if (MHU_DB_SIGNALED)
            events = mhu_wait_event(0, MHU_DB_EVENT_ALL);
        if (MHU_FC_SIGNALED) {
            /* FC0(bit0)是命令数据通道,其通知由 drain_command_fifo 在
             * 关中断下原子消费(取元数据+清位+清数据),这里只取走
             * 事件位(bit1~31)交给 process_event,避免双重消费竞态。 */
            unsigned long irq_flags = arch_local_irq_save();
            fc_events = g_mbx_fc_stat_0 & ~(1u << TS_CONTROL_FASTCHANNEL);
            g_mbx_fc_stat_0 &= (1u << TS_CONTROL_FASTCHANNEL);
            arch_local_irq_restore(irq_flags);
        }

        if (MHU_FF_SIGNALED || MHU_FC_SIGNALED || (fc_events & (1u << TS_CONTROL_FASTCHANNEL)))
            drain_command_fifo();

        ret = events & ~MHU_DB0_EVENT_HOST_STARTUP;
        if (ret) {
            for (i = 0; i < 32; i++) {
                if (ret & (1 << i))
                    process_event(i);
            }
        }

        ret = fc_events & ~(1u << TS_CONTROL_FASTCHANNEL);
        if (ret) {
            for (i = 0; i < 32; i++) {
                if (ret & (1 << i))
                    process_event(i);
            }
        }
    }
}
