/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef R52_GPU_UAPI_H
#define R52_GPU_UAPI_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define R52_GPU_ABI_VERSION  1
#define R52_GPU_IOC_MAGIC    'R'

#define R52_GPU_MMAP_SHARED  0x00000
#define R52_GPU_MMAP_PBX     0x10000
#define R52_GPU_MMAP_MBX     0x20000

#define R52_GPU_MHU_REGION_SIZE 0x1000

struct r52_gpu_info {
	__u32 abi_version;
	__u16 vendor_id;
	__u16 device_id;
	__u8 sm_count;
	__u8 core_per_sm;
	__u8 warp_size;
	__u8 warp_per_core;
	__u32 shared_memory_size;
	__u32 firmware_version;
};

struct r52_gpu_launch {
	__u32 grid_x;
	__u32 grid_y;
	__u32 grid_z;
	__u16 workgroup_x;
	__u16 workgroup_y;
	__u16 workgroup_z;
	__u16 reserved;
	__u64 sequence;
};

struct r52_gpu_completion {
	__u64 sequence;
	__u64 start_ns;
	__u64 end_ns;
};

struct r52_gpu_resources {
	__u32 abi_version;
	__u32 shared_memory_size;
	__u64 shared_dma;
	__u32 mhu_region_size;
	__u32 reserved;
};

#define R52_GPU_IOC_QUERY \
	_IOR(R52_GPU_IOC_MAGIC, 0x00, struct r52_gpu_info)
#define R52_GPU_IOC_SUBMIT \
	_IOWR(R52_GPU_IOC_MAGIC, 0x01, struct r52_gpu_launch)
#define R52_GPU_IOC_GET_COMPLETION \
	_IOR(R52_GPU_IOC_MAGIC, 0x02, struct r52_gpu_completion)
#define R52_GPU_IOC_GET_RESOURCES \
	_IOR(R52_GPU_IOC_MAGIC, 0x03, struct r52_gpu_resources)

#endif
