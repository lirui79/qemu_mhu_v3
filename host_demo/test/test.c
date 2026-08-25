#include "system.h"
#include "test.h"
#include "ts_protocol.h"

static char *get_queue_type_desc(uint8_t type)
{
    char *desc = "UNKNOWN";
    switch (type) {
        case TS_QUEUE_TYPE_COMPUTE:
            desc = "COMPUTE";
            break;
        case TS_QUEUE_TYPE_COMPUTE_AQL:
            desc = "AQL";
            break;
        case TS_QUEUE_TYPE_SDMA:
            desc = "SDMA";
            break;
        default:
            break;
    }
    return desc;
}

int create_process(uint16_t pasid, uint16_t vmid)
{
    ts_cmd_create_process cmd;
    uint32_t len;

    cmd.header      = TS_HDR_MAKE(TS_CMD_CODE_CREATE_PROCESS, 0, TS_PACKET_LEN(cmd));
    cmd.pasid_vmid  = ((uint32_t)vmid << 16) | (pasid & 0xFFFF);

    len = mhu_send_data(0, &cmd, sizeof(cmd));
    if (len != sizeof(cmd)) {
        ts_printf("Failed to send create process cmd, ret %u\n", len);
        return -1;
    }
    ts_printf("Create process success: pasid 0x%x vmid 0x%x\n", pasid, vmid);
    return 0;
}

int destroy_process(uint16_t pasid)
{
    ts_cmd_destroy_process cmd;
    uint32_t len;

    cmd.header      = TS_HDR_MAKE(TS_CMD_CODE_DESTROY_PROCESS, 0, TS_PACKET_LEN(cmd));
    cmd.pasid_rsvd  = pasid & 0xFFFF;

    len = mhu_send_data(0, &cmd, sizeof(cmd));
    if (len != sizeof(cmd)) {
        ts_printf("Failed to send destroy process cmd, ret %u\n", len);
        return -1;
    }
    ts_printf("Destroy process success: pasid 0x%x\n", pasid);
    return 0;
}

int create_queue(uint16_t queue_id, uint16_t pasid, uint8_t type,
                 uint8_t priority, uint16_t doorbell_idx,
                 uint64_t ringbuf_addr, uint32_t ringbuf_size,
                 uint64_t rptr, uint64_t wptr)
{
    ts_cmd_create_queue cmd;
    uint32_t len;

    uart_write(UART0_BASE, "[CQ1]");
    ts_printf("queue 0x%x  0x%x\n", queue_id, pasid);
    cmd.header         = TS_HDR_MAKE(TS_CMD_CODE_CREATE_QUEUE, 0, TS_PACKET_LEN(cmd));
    uart_write(UART0_BASE, "[CQ2]");
    cmd.queue_pasid    = ((uint32_t)pasid << 16) | (queue_id & 0xFFFF);
    cmd.type_pri_db    = ((uint32_t)doorbell_idx << 16) |
                         ((uint32_t)priority << 8) |
                         (type & 0xFF);
    uart_write(UART0_BASE, "[CQ3]");
    cmd.ringbuf_size    = ringbuf_size;
    cmd.ringbuf_base_lo = (uint32_t)(ringbuf_addr & 0xFFFFFFFF);
    cmd.ringbuf_base_hi = (uint32_t)((ringbuf_addr >> 32) & 0xFFFFFFFF);
    uart_write(UART0_BASE, "[CQ4]");
    cmd.rptr_lo         = (uint32_t)(rptr & 0xFFFFFFFF);
    cmd.rptr_hi         = (uint32_t)((rptr >> 32) & 0xFFFFFFFF);
    uart_write(UART0_BASE, "[CQ5]");
    cmd.wptr_lo         = (uint32_t)(wptr & 0xFFFFFFFF);
    cmd.wptr_hi         = (uint32_t)((wptr >> 32) & 0xFFFFFFFF);
    uart_write(UART0_BASE, "[CQ6]");
    ts_printf("Create queue: sz=%u @%p\n", (uint32_t)sizeof(cmd), &cmd);
    uart_write(UART0_BASE, "[CQ7]");
    len = mhu_send_data(0, &cmd, sizeof(cmd));
    if (len != sizeof(cmd)) {
        ts_printf("Failed to send create queue cmd, ret %u\n", len);
        return -1;
    }
    ts_printf("Create queue success: queue_id 0x%x pasid 0x%x type %s pri %u db %u\n",
              queue_id, pasid, get_queue_type_desc(type), priority, doorbell_idx);
    ts_printf("                    : ringbuf 0x%llx (%u) rptr 0x%llx wptr 0x%llx\n",
              ringbuf_addr, ringbuf_size, rptr, wptr);
    return 0;
}

int destroy_queue(uint16_t queue_id, uint16_t pasid)
{
    ts_cmd_destroy_queue cmd;
    uint32_t len;

    cmd.header         = TS_HDR_MAKE(TS_CMD_CODE_DESTROY_QUEUE, 0, TS_PACKET_LEN(cmd));
    cmd.queue_pasid    = ((uint32_t)pasid << 16) | (queue_id & 0xFFFF);

    len = mhu_send_data(0, &cmd, sizeof(cmd));
    if (len != sizeof(cmd)) {
        ts_printf("Failed to send create queue cmd, ret %u\n", len);
        return -1;
    }
    ts_printf("Destroy queue success: queue_id 0x%x pasid 0x%x\n", queue_id, pasid);
    return 0;
}

int run_test(void)
{
    int ret;

    ts_printf("\n================================\n");
    ts_printf("          TEST START\n");
    ts_printf("================================\n\n");

    ret = launch_kernel_case_1();
    if (ret != 0)
        goto __test_out;

__test_out:
    ts_printf("\n================================\n");
    ts_printf("          TEST %s\n", (!ret) ? "PASS" : "FAILED");
    ts_printf("================================\n\n");

    return ret;
}
