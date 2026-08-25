#ifndef __TEST_H__
#define __TEST_H__

int create_process(uint16_t pasid, uint16_t vmid);
int destroy_process(uint16_t pasid);
int create_queue(uint16_t queue_id, uint16_t pasid, uint8_t type,
                 uint8_t priority, uint16_t doorbell_idx,
                 uint64_t ringbuf_addr, uint32_t ringbuf_size,
                 uint64_t rptr, uint64_t wptr);
int destroy_queue(uint16_t queue_id, uint16_t pasid);

int launch_kernel_case_1(void);

#endif
