#include <string.h>
#include "system.h"
#include "ts_protocol.h"
#include "hsa_protocol.h"
#include "test.h"

int launch_kernel_case_1(void)
{
    uint64_t start_ts, diff_time;
    uint16_t pasid                      = 0x8;
    uint16_t vmid                       = 0x1;
    uint16_t queue_id                   = 0x5;
    uint16_t doorbell_idx               = 2;
    uint32_t ringbuf_addr               = DDR_BASE + 0x100000;
    uint32_t ringbuf_size               = 0x1000;
    amd_queue_t                  *mqd   = (amd_queue_t*)(uint64_t)(DDR_BASE + 0x200000);
    amd_signal_t                 *sig   = (amd_signal_t*)(uint64_t)(DDR_BASE + 0x210000);
    kernel_descriptor_t          *desc  = (kernel_descriptor_t*)(uint64_t)(DDR_BASE + 0x220000);
    hsa_kernel_dispatch_packet_t *pkt   = (hsa_kernel_dispatch_packet_t*)(uint64_t)(DDR_BASE + 0x230000);
    hsa_signal_t                 _sig   = {.handle = (uint64_t)sig};
    int ret;

    ts_printf(">>> Launch Kernel Test Case 1\n\n");

    ret = create_process(pasid, vmid);
    if (ret != 0)
        goto __test_out;
    ts_printf(">>> create_queue\n\n");

    ret = create_queue(queue_id, pasid,
                       TS_QUEUE_TYPE_COMPUTE_AQL,
                       8, doorbell_idx,
                       (uint64_t)ringbuf_addr, ringbuf_size,
                       (uint64_t)&mqd->read_dispatch_id,
                       (uint64_t)&mqd->write_dispatch_id);
    if (ret != 0)
        goto __destroy_process_out;

    memset(mqd, 0, sizeof(amd_queue_t));
    memset(sig, 0, sizeof(amd_signal_t));
    memset(desc, 0, sizeof(kernel_descriptor_t));

    desc->group_segment_fixed_size      = 16 * 1024;
    desc->kernarg_size                  = 256;
    desc->kernel_code_entry_byte_offset = 0x1000;

    pkt->header                = (HSA_PACKET_TYPE_KERNEL_DISPATCH << HSA_PACKET_HEADER_TYPE) |
                                (HSA_FENCE_SCOPE_SYSTEM << HSA_PACKET_HEADER_SCACQUIRE_FENCE_SCOPE) |
                                (HSA_FENCE_SCOPE_SYSTEM << HSA_PACKET_HEADER_SCRELEASE_FENCE_SCOPE);
    pkt->setup                 = 0;
    pkt->workgroup_size_x      = 64;
    pkt->workgroup_size_y      = 64;
    pkt->workgroup_size_z      = 1;
    pkt->grid_size_x           = 1024;
    pkt->grid_size_y           = 512;
    pkt->grid_size_z           = 1;
    pkt->private_segment_size  = 2048;
    pkt->group_segment_size    = 32 * 1024;
    pkt->kernel_object         = (uint64_t)desc;
    pkt->completion_signal     = _sig;

    /* reset signal value */
    sig->value = 1;

    /* push to ringbuf */
    memcpy((void*)(uint64_t)ringbuf_addr, pkt, sizeof(hsa_kernel_dispatch_packet_t));

    /* update write pointer */
    mqd->write_dispatch_id = 1;

    ts_printf("Signal TS to dispatch kernel ...\n");
    start_ts = arch_get_time_ns();

    /* signal doorbell */
    mhu_send_event(0, (1 << doorbell_idx));

    /* wait for completion */
    mhu_wait_event(0, MHU_DB0_EVENT_KERNEL_COMPLETE);

    diff_time = (arch_get_time_ns() - start_ts) / 1000;
    ts_printf("Kernel dispatch complete in %llu us\n", diff_time);
    ts_printf("  start_ts: %llu\n", sig->start_ts);
    ts_printf("    end_ts: %llu\n", sig->end_ts);

    destroy_queue(queue_id, pasid);

__destroy_process_out:
    destroy_process(pasid);
__test_out:
    ts_printf("\n<<< Test %s\n", (!ret) ? "PASS" : "FAILED");
    return ret;
}
