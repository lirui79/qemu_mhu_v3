#include "system.h"
#include "hsa_protocol.h"

static void kernel_dispatch(user_queue *queue, hsa_kernel_dispatch_packet_t *packet)
{
    uint64_t  start_ts, end_ts;
    amd_signal_t        *sig  = (amd_signal_t*)(uint32_t)(packet->completion_signal.handle);
    kernel_descriptor_t *desc = (kernel_descriptor_t*)(uint32_t)(packet->kernel_object);
    int64_t              code = (int64_t)packet->kernel_object + desc->kernel_code_entry_byte_offset;

    ts_printf("AQL: [Q%u] kernel dispatch: grid=%ux%ux%u wg=%ux%ux%u code=0x%X sig=0x%X\n",
              queue->queue_id, packet->grid_size_x, packet->grid_size_y, packet->grid_size_z,
              packet->workgroup_size_x, packet->workgroup_size_y, packet->workgroup_size_z,
              packet->kernel_object, packet->completion_signal.handle);
    ts_printf("AQL: [Q%u] code_entry=0x%X kernarg_size=0x%x\n",
              queue->queue_id, code, desc->kernarg_size);

    start_ts = arch_get_time_ns();
    delay_us(456); //simulate kernel execution
    end_ts = arch_get_time_ns();

    WRITE64(&sig->start_ts, start_ts);
    WRITE64(&sig->end_ts,   end_ts);
    WRITE32(&sig->value,    0);

    /* Notify an interrupt-driven Linux host after shared state is visible. */
    __asm volatile ("dmb sy" ::: "memory");
    mhu_send_event(0, MHU_DB0_EVENT_KERNEL_COMPLETE);

    ts_printf("AQL: [Q%u] kernel dispatch completed in %Uus\n",
              queue->queue_id, (end_ts - start_ts) / 1000);
}

/**
 * process packet of AQL queue
 */
void process_aql_queue(user_queue *queue)
{
    struct hsa_kernel_dispatch_packet_s packet;
    uint32_t *packet_ptr = (uint32_t*)&packet;
    uint32_t  packet_len = (sizeof(hsa_kernel_dispatch_packet_t) / sizeof(uint32_t));
    uint32_t  rbuf_addr  = (uint32_t)queue->ringbuf_base;
    uint32_t  i, wptr, rptr;
    uint8_t   pkt_type, is_barrier, acq_scope, rel_scope;

    wptr = READ32(queue->wptr);
    rptr = READ32(queue->rptr);

    ts_printf("AQL: process queue 0x%x (0x%x) wptr 0x%x rptr 0x%x\n",
              queue->queue_id, queue->pasid, wptr, rptr);
    
    if (wptr == rptr)
        return;

    rbuf_addr = rbuf_addr + (rptr * sizeof(hsa_kernel_dispatch_packet_t));
    for (i = 0; i < packet_len; i++) {
        packet_ptr[i] = READ32(rbuf_addr + (i * sizeof(uint32_t)));
    }

    rptr++;
    WRITE32(queue->rptr, rptr);

    pkt_type   = packet.header & 0xff;
    is_barrier = (packet.header >> 8) & 0x1;
    acq_scope  = (packet.header >> 9) & 0x3;
    rel_scope  = (packet.header >> 11) & 0x3;
    ts_printf("AQL: dispatch packet type %u barrier %u acq %u rel %u\n",
              pkt_type, is_barrier, acq_scope, rel_scope);
    
    if (pkt_type == HSA_PACKET_TYPE_KERNEL_DISPATCH) {
        kernel_dispatch(queue, &packet);
    }
}
