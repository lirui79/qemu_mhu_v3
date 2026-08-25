// SPDX-License-Identifier: MIT

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <poll.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "r52_runtime.h"

#define R52_GPU_DEVICE "/dev/r52_gpu"

#define MHU_PBX_FCH_PAY32(ch)            (4 * (ch))

#define HSA_PACKET_KERNEL_DISPATCH       2
#define HSA_FENCE_SCOPE_SYSTEM           2
#define HSA_ACQUIRE_SHIFT                9
#define HSA_RELEASE_SHIFT                11

#define DEMO_QUEUE_SEND_FASTCHANNEL      2

struct r52_kernel_descriptor {
	uint32_t group_segment_fixed_size;
	uint32_t private_segment_fixed_size;
	uint32_t kernarg_size;
	uint8_t reserved0[4];
	int64_t kernel_code_entry_byte_offset;
	uint8_t reserved1[20];
	uint32_t compute_pgm_rsrc3;
	uint32_t compute_pgm_rsrc1;
	uint32_t compute_pgm_rsrc2;
	uint16_t kernel_code_properties;
	uint8_t reserved2[6];
};

struct r52_dispatch_packet {
	uint16_t header;
	uint16_t setup;
	uint16_t workgroup_size_x;
	uint16_t workgroup_size_y;
	uint16_t workgroup_size_z;
	uint16_t reserved0;
	uint32_t grid_size_x;
	uint32_t grid_size_y;
	uint32_t grid_size_z;
	uint32_t private_segment_size;
	uint32_t group_segment_size;
	uint64_t kernel_object;
	uint64_t kernarg_address;
	uint64_t reserved2;
	uint64_t completion_signal;
};

struct r52_signal {
	int64_t kind;
	int64_t value;
	uint64_t event_mailbox_ptr;
	uint32_t event_id;
	uint32_t reserved1;
	uint64_t start_ts;
	uint64_t end_ts;
	uint64_t queue_ptr;
	uint32_t reserved3[2];
};

struct r52_gpu_shared {
	uint64_t read_dispatch_id;
	uint64_t write_dispatch_id;
	struct r52_kernel_descriptor descriptor __attribute__((aligned(64)));
	struct r52_dispatch_packet packet __attribute__((aligned(64)));
	struct r52_signal signal __attribute__((aligned(64)));
};

static void mmio_write(volatile uint32_t *base, uint32_t offset, uint32_t value)
{
	base[offset / sizeof(uint32_t)] = value;
}

int r52_runtime_open(struct r52_runtime *runtime)
{
	if (!runtime) {
		errno = EINVAL;
		return -1;
	}
	memset(runtime, 0, sizeof(*runtime));
	runtime->fd = -1;
	runtime->fd = open(R52_GPU_DEVICE, O_RDWR | O_CLOEXEC);
	if (runtime->fd < 0)
		return -1;
	if (ioctl(runtime->fd, R52_GPU_IOC_GET_RESOURCES,
		  &runtime->resources) < 0) {
		r52_runtime_close(runtime);
		return -1;
	}
	runtime->shared = mmap(NULL, runtime->resources.shared_memory_size,
			       PROT_READ | PROT_WRITE, MAP_SHARED,
			       runtime->fd, R52_GPU_MMAP_SHARED);
	if (runtime->shared == MAP_FAILED) {
		r52_runtime_close(runtime);
		return -1;
	}
	runtime->pbx = mmap(NULL, runtime->resources.mhu_region_size,
			    PROT_READ | PROT_WRITE, MAP_SHARED,
			    runtime->fd, R52_GPU_MMAP_PBX);
	if (runtime->pbx == MAP_FAILED) {
		r52_runtime_close(runtime);
		return -1;
	}
	return 0;
}

void r52_runtime_close(struct r52_runtime *runtime)
{
	if (!runtime)
		return;
	if (runtime->shared && runtime->shared != MAP_FAILED)
		munmap(runtime->shared, runtime->resources.shared_memory_size);
	if (runtime->pbx && runtime->pbx != MAP_FAILED)
		munmap((void *)runtime->pbx, runtime->resources.mhu_region_size);
	if (runtime->fd >= 0)
		close(runtime->fd);
	runtime->fd = -1;
	runtime->shared = NULL;
	runtime->pbx = NULL;
}

int r52_runtime_query(struct r52_runtime *runtime, struct r52_gpu_info *info)
{
	if (!runtime || runtime->fd < 0 || !info) {
		errno = EINVAL;
		return -1;
	}
	if (ioctl(runtime->fd, R52_GPU_IOC_QUERY, info) < 0)
		return -1;
	return 0;
}

static int runtime_wait_kernel_complete(struct r52_runtime *runtime,
					int timeout_ms)
{
	struct pollfd pfd = {
		.fd = runtime->fd,
		.events = POLLIN,
	};
	int ret;

	ret = poll(&pfd, 1, timeout_ms);
	if (ret < 0)
		return -1;
	if (!ret) {
		errno = ETIMEDOUT;
		return -1;
	}
	return 0;
}

int r52_runtime_launch(struct r52_runtime *runtime,
		       struct r52_gpu_launch *launch,
		       struct r52_gpu_completion *completion,
		       int timeout_ms)
{
	struct r52_gpu_shared *shared;
	uint64_t descriptor;
	uint64_t signal;

	if (!runtime || runtime->fd < 0 || !launch || !completion) {
		errno = EINVAL;
		return -1;
	}
	if (ioctl(runtime->fd, R52_GPU_IOC_SUBMIT, launch) < 0)
		return -1;

	shared = runtime->shared;
	descriptor = runtime->resources.shared_dma +
		     offsetof(struct r52_gpu_shared, descriptor);
	signal = runtime->resources.shared_dma +
		 offsetof(struct r52_gpu_shared, signal);

	memset(shared, 0, sizeof(*shared));
	shared->descriptor.group_segment_fixed_size = 16 * 1024;
	shared->descriptor.kernarg_size = 256;
	shared->descriptor.kernel_code_entry_byte_offset = 0x1000;

	shared->packet.header = HSA_PACKET_KERNEL_DISPATCH |
		(HSA_FENCE_SCOPE_SYSTEM << HSA_ACQUIRE_SHIFT) |
		(HSA_FENCE_SCOPE_SYSTEM << HSA_RELEASE_SHIFT);
	shared->packet.workgroup_size_x = launch->workgroup_x;
	shared->packet.workgroup_size_y = launch->workgroup_y;
	shared->packet.workgroup_size_z = launch->workgroup_z;
	shared->packet.grid_size_x = launch->grid_x;
	shared->packet.grid_size_y = launch->grid_y;
	shared->packet.grid_size_z = launch->grid_z;
	shared->packet.private_segment_size = 2048;
	shared->packet.group_segment_size = 32 * 1024;
	shared->packet.kernel_object = descriptor;
	shared->packet.completion_signal = signal;
	shared->signal.value = 1;
	shared->write_dispatch_id = 1;

	__sync_synchronize();
	mmio_write(runtime->pbx, MHU_PBX_FCH_PAY32(DEMO_QUEUE_SEND_FASTCHANNEL),
		   (uint32_t)launch->sequence);

	if (runtime_wait_kernel_complete(runtime, timeout_ms) < 0)
		return -1;
	if (ioctl(runtime->fd, R52_GPU_IOC_GET_COMPLETION, completion) < 0)
		return -1;
	return 0;
}
