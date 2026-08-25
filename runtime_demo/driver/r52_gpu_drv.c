// SPDX-License-Identifier: GPL-2.0
/*
 * Linux-side driver for the Cortex-A76/R52 virtual platform demo.
 * The first version deliberately combines the MHU transport and GPU client;
 * a production design should expose MHU through the mailbox framework.
 */

#include <linux/atomic.h>
#include <linux/compat.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "r52_gpu_uapi.h"

#define DRV_NAME                         "r52_gpu"
#define R52_GPU_SHARED_SIZE              PAGE_SIZE

#define MHU_DB_STARTUP                   BIT(0)
#define MHU_DB_KERNEL_COMPLETE           BIT(31)

#define MHU_PBX_DBCW_SET(ch)             (0x100c + 32 * (ch))
#define MHU_PBX_FFCW_PAY(ch)             (0x2000 + 64 * (ch))
#define MHU_PBX_FFCW_FLG(ch)             (0x2008 + 64 * (ch))
#define MHU_PBX_FFCW_ST(ch)              (0x2024 + 64 * (ch))
#define MHU_PBX_FCH_PAY32(ch)            (0x3000 + 4 * (ch))
#define MHU_PBX_IIDR                     0x0fc8

#define MHU_MBX_DBCH_CFG0                0x0020
#define MHU_MBX_FFCH_CFG0                0x0030
#define MHU_MBX_FCH_CFG0                 0x0040
#define MHU_MBX_FCH_CTRL                 0x0140
#define MHU_MBX_FCG_INT_EN               0x0144
#define MHU_MBX_DBCH_INT_ST(ch)          (0x0400 + 4 * (ch))
#define MHU_MBX_FFCH_INT_ST(ch)          (0x0410 + 4 * (ch))
#define MHU_MBX_FCG_INT_ST               0x0470
#define MHU_MBX_FCH_GRP_INT_ST(ch)       (0x0480 + 4 * (ch))
#define MHU_MBX_IIDR                     0x0fc8
#define MHU_MBX_DBCW_ST(ch)              (0x1000 + 32 * (ch))
#define MHU_MBX_DBCW_INT_CLR(ch)         (0x1014 + 32 * (ch))
#define MHU_MBX_DBCW_INT_EN(ch)          (0x1018 + 32 * (ch))
#define MHU_MBX_FFCW_PAY(ch)             (0x2000 + 64 * (ch))
#define MHU_MBX_FFCW_FLG(ch)             (0x2008 + 64 * (ch))
#define MHU_MBX_FFCW_INT_ST(ch)          (0x2010 + 64 * (ch))
#define MHU_MBX_FFCW_INT_CLR(ch)         (0x2014 + 64 * (ch))
#define MHU_MBX_FFCW_INT_EN(ch)          (0x2018 + 64 * (ch))
#define MHU_MBX_FFCW_CTRL(ch)            (0x2020 + 64 * (ch))
#define MHU_MBX_FFCW_ST(ch)              (0x2024 + 64 * (ch))
#define MHU_MBX_FCH_PAY32(ch)            (0x3000 + 4 * (ch))
#define MHU_FF_CTRL_RA_EN                 BIT(1)
#define MHU_FF_INT_ACK                   BIT(0)
#define MHU_FCH_CTRL_INT_EN              BIT(2)

#define TS_CMD_QUERY                     0x01
#define TS_CMD_CREATE_PROCESS            0x02
#define TS_CMD_DESTROY_PROCESS           0x03
#define TS_CMD_CREATE_QUEUE              0x04
#define TS_CMD_DESTROY_QUEUE             0x05
#define TS_QUERY_HW                      0x01
#define TS_QUERY_SW                      0x02
#define TS_QUEUE_COMPUTE_AQL             0x01

#define HSA_PACKET_KERNEL_DISPATCH       2
#define HSA_FENCE_SCOPE_SYSTEM           2
#define HSA_ACQUIRE_SHIFT                9
#define HSA_RELEASE_SHIFT                11

#define DEMO_PASID                       8
#define DEMO_VMID                        1
#define DEMO_QUEUE_ID                    5
#define DEMO_CONTROL_FASTCHANNEL         0
#define DEMO_QUEUE_SEND_FASTCHANNEL      2

struct r52_kernel_descriptor {
	u32 group_segment_fixed_size;
	u32 private_segment_fixed_size;
	u32 kernarg_size;
	u8 reserved0[4];
	s64 kernel_code_entry_byte_offset;
	u8 reserved1[20];
	u32 compute_pgm_rsrc3;
	u32 compute_pgm_rsrc1;
	u32 compute_pgm_rsrc2;
	u16 kernel_code_properties;
	u8 reserved2[6];
};

struct r52_dispatch_packet {
	u16 header;
	u16 setup;
	u16 workgroup_size_x;
	u16 workgroup_size_y;
	u16 workgroup_size_z;
	u16 reserved0;
	u32 grid_size_x;
	u32 grid_size_y;
	u32 grid_size_z;
	u32 private_segment_size;
	u32 group_segment_size;
	u64 kernel_object;
	u64 kernarg_address;
	u64 reserved2;
	u64 completion_signal;
};

struct r52_signal {
	s64 kind;
	s64 value;
	u64 event_mailbox_ptr;
	u32 event_id;
	u32 reserved1;
	u64 start_ts;
	u64 end_ts;
	u64 queue_ptr;
	u32 reserved3[2];
};

struct r52_gpu_shared {
	u64 read_dispatch_id;
	u64 write_dispatch_id;
	struct r52_kernel_descriptor descriptor __aligned(64);
	struct r52_dispatch_packet packet __aligned(64);
	struct r52_signal signal __aligned(64);
};

struct r52_gpu {
	struct device *dev;
	void __iomem *pbx;
	void __iomem *mbx;
	phys_addr_t pbx_phys;
	phys_addr_t mbx_phys;
	resource_size_t pbx_size;
	resource_size_t mbx_size;
	int rx_irq;
	struct miscdevice miscdev;
	struct mutex operation_lock;
	struct completion fifo_completion;
	wait_queue_head_t completion_wait;
	void *shared_cpu;
	dma_addr_t shared_dma;
	phys_addr_t shared_phys;
	atomic64_t submitted;
	atomic64_t completed;
	atomic_t opened;
	bool queue_created;
	bool irq_debug_done;
};

struct r52_gpu_file {
	struct r52_gpu *gpu;
	u64 submitted_sequence;
	u64 seen_completed;
};

struct ts_hw_response {
	u32 header;
	u32 ids;
	u32 topology;
	u32 smem_size;
	u32 l2_cache_size;
	u32 l1_dcache_size;
	u32 l1_icache_size;
};

static u32 ts_header(u8 code, u16 words)
{
	return code | ((u32)words << 16);
}

static void r52_gpu_send_doorbell(struct r52_gpu *gpu, u32 bits)
{
	wmb();
	writel(bits, gpu->pbx + MHU_PBX_DBCW_SET(0));
}

static void r52_gpu_send_fastchannel(struct r52_gpu *gpu, u32 channel, u32 value)
{
	wmb();
	writel(value, gpu->pbx + MHU_PBX_FCH_PAY32(channel));
}

static void r52_gpu_send_words(struct r52_gpu *gpu, const u32 *words,
			       size_t count)
{
	size_t index;

	for (index = 0; index < count; index++) {
		u32 flags = 0;

		if (!index)
			flags |= 0x02;
		if (index == count - 1)
			flags |= 0x05;
		writel(flags, gpu->pbx + MHU_PBX_FFCW_FLG(0));
		writel(words[index], gpu->pbx + MHU_PBX_FFCW_PAY(0));
	}
	r52_gpu_send_fastchannel(gpu, DEMO_CONTROL_FASTCHANNEL, count);
}

static size_t r52_gpu_receive_words(struct r52_gpu *gpu, u32 *words,
				    size_t capacity)
{
	u32 fill = readl(gpu->mbx + MHU_MBX_FFCW_ST(0)) & 0xffff;
	size_t count = min_t(size_t, fill / sizeof(u32), capacity);
	size_t index;

	for (index = 0; index < count; index++) {
		words[index] = readl(gpu->mbx + MHU_MBX_FFCW_PAY(0));
		readl(gpu->mbx + MHU_MBX_FFCW_FLG(0));
	}
	return count;
}

static size_t r52_gpu_wait_receive_words(struct r52_gpu *gpu, u32 *words,
					 size_t capacity,
					 unsigned int timeout_ms)
{
	size_t count;

	while (capacity) {
		unsigned long timeout = msecs_to_jiffies(timeout_ms);

		if (!timeout)
			timeout = 1;
		if (!wait_for_completion_timeout(&gpu->fifo_completion, timeout))
			break;
		count = r52_gpu_receive_words(gpu, words, capacity);
		if (count)
			return count;
	}

	return r52_gpu_receive_words(gpu, words, capacity);
}

static irqreturn_t r52_gpu_irq(int irq, void *data)
{
	struct r52_gpu *gpu = data;
	u32 db_int = readl(gpu->mbx + MHU_MBX_DBCH_INT_ST(0));
	u32 ff_int = readl(gpu->mbx + MHU_MBX_FFCH_INT_ST(0));
	u32 fc_int = readl(gpu->mbx + MHU_MBX_FCH_GRP_INT_ST(0));
	u32 status = 0;

	if (db_int & BIT(0)) {
		status = readl(gpu->mbx + MHU_MBX_DBCW_ST(0));
		if (status)
			writel(status, gpu->mbx + MHU_MBX_DBCW_INT_CLR(0));
	}

	if (ff_int & BIT(0)) {
		writel(readl(gpu->mbx + MHU_MBX_FFCW_INT_ST(0)),
		       gpu->mbx + MHU_MBX_FFCW_INT_CLR(0));
		complete_all(&gpu->fifo_completion);
		wake_up_interruptible(&gpu->completion_wait);
	}

	if (fc_int & BIT(DEMO_CONTROL_FASTCHANNEL)) {
		readl(gpu->mbx + MHU_MBX_FCH_PAY32(DEMO_CONTROL_FASTCHANNEL));
		complete_all(&gpu->fifo_completion);
		wake_up_interruptible(&gpu->completion_wait);
	}

	if (status & MHU_DB_KERNEL_COMPLETE) {
		atomic64_inc(&gpu->completed);
		wake_up_interruptible(&gpu->completion_wait);
	}

	return (status || ff_int || fc_int) ? IRQ_HANDLED : IRQ_NONE;
}

static int r52_gpu_query(struct r52_gpu *gpu, struct r52_gpu_info *info)
{
	struct ts_hw_response response;
	u32 sw_response[2];
	u32 command[2] = {
		ts_header(TS_CMD_QUERY, 2),
		TS_QUERY_HW,
	};
	size_t words;

	reinit_completion(&gpu->fifo_completion);
	r52_gpu_send_words(gpu, command, ARRAY_SIZE(command));
	words = r52_gpu_wait_receive_words(gpu, (u32 *)&response,
					    sizeof(response) / sizeof(u32),
					    10000);
	if (words != sizeof(response) / sizeof(u32))
		return words ? -EPROTO : -ETIMEDOUT;
	if ((response.header & 0xff) != 0x01 ||
	    (response.header >> 16) != sizeof(response) / sizeof(u32))
		return -EPROTO;

	memset(info, 0, sizeof(*info));
	info->abi_version = R52_GPU_ABI_VERSION;
	info->vendor_id = response.ids & 0xffff;
	info->device_id = response.ids >> 16;
	info->sm_count = response.topology & 0xff;
	info->core_per_sm = (response.topology >> 8) & 0xff;
	info->warp_size = (response.topology >> 16) & 0xff;
	info->warp_per_core = response.topology >> 24;
	info->shared_memory_size = R52_GPU_SHARED_SIZE;

	command[1] = TS_QUERY_SW;
	reinit_completion(&gpu->fifo_completion);
	r52_gpu_send_words(gpu, command, ARRAY_SIZE(command));
	words = r52_gpu_wait_receive_words(gpu, sw_response,
					    ARRAY_SIZE(sw_response), 10000);
	if (words != ARRAY_SIZE(sw_response))
		return words ? -EPROTO : -ETIMEDOUT;
	if ((sw_response[0] & 0xff) != 0x02 ||
	    (sw_response[0] >> 16) != ARRAY_SIZE(sw_response))
		return -EPROTO;
	info->firmware_version = sw_response[1];
	return 0;
}

static void r52_gpu_create_queue(struct r52_gpu *gpu)
{
	struct r52_gpu_shared *shared = gpu->shared_cpu;
	phys_addr_t packet_phys = gpu->shared_phys +
				offsetof(struct r52_gpu_shared, packet);
	phys_addr_t read_phys = gpu->shared_phys +
			      offsetof(struct r52_gpu_shared, read_dispatch_id);
	phys_addr_t write_phys = gpu->shared_phys +
			       offsetof(struct r52_gpu_shared, write_dispatch_id);
	u32 process[2] = {
		ts_header(TS_CMD_CREATE_PROCESS, 2),
		DEMO_PASID | (DEMO_VMID << 16),
	};
	u32 queue[10] = {
		ts_header(TS_CMD_CREATE_QUEUE, 10),
		DEMO_QUEUE_ID | (DEMO_PASID << 16),
		TS_QUEUE_COMPUTE_AQL | (8 << 8) |
			(DEMO_QUEUE_SEND_FASTCHANNEL << 16),
		lower_32_bits(packet_phys),
		upper_32_bits(packet_phys),
		sizeof(shared->packet),
		lower_32_bits(read_phys),
		upper_32_bits(read_phys),
		lower_32_bits(write_phys),
		upper_32_bits(write_phys),
	};

	r52_gpu_send_words(gpu, process, ARRAY_SIZE(process));
	r52_gpu_send_words(gpu, queue, ARRAY_SIZE(queue));
	gpu->queue_created = true;
}

static void r52_gpu_destroy_queue(struct r52_gpu *gpu)
{
	u32 queue[2] = {
		ts_header(TS_CMD_DESTROY_QUEUE, 2),
		DEMO_QUEUE_ID | (DEMO_PASID << 16),
	};
	u32 process[2] = {
		ts_header(TS_CMD_DESTROY_PROCESS, 2),
		DEMO_PASID,
	};

	if (!gpu->queue_created)
		return;
	r52_gpu_send_words(gpu, queue, ARRAY_SIZE(queue));
	r52_gpu_send_words(gpu, process, ARRAY_SIZE(process));
	gpu->queue_created = false;
}

static u64 r52_gpu_submit(struct r52_gpu *gpu)
{
	u64 sequence;

	if (!gpu->queue_created)
		r52_gpu_create_queue(gpu);

	sequence = atomic64_inc_return(&gpu->submitted);
	return sequence;
}

static int r52_gpu_open(struct inode *inode, struct file *file)
{
	struct miscdevice *misc = file->private_data;
	struct r52_gpu *gpu = container_of(misc, struct r52_gpu, miscdev);
	struct r52_gpu_file *ctx;
	int ret;

	if (atomic_cmpxchg(&gpu->opened, 0, 1))
		return -EBUSY;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx) {
		atomic_set(&gpu->opened, 0);
		return -ENOMEM;
	}
	ctx->gpu = gpu;
	file->private_data = ctx;
	ret = nonseekable_open(inode, file);
	if (ret) {
		kfree(ctx);
		atomic_set(&gpu->opened, 0);
	}
	return ret;
}

static int r52_gpu_release(struct inode *inode, struct file *file)
{
	struct r52_gpu_file *ctx = file->private_data;

	mutex_lock(&ctx->gpu->operation_lock);
	r52_gpu_destroy_queue(ctx->gpu);
	mutex_unlock(&ctx->gpu->operation_lock);
	atomic_set(&ctx->gpu->opened, 0);
	kfree(ctx);
	return 0;
}

static long r52_gpu_ioctl(struct file *file, unsigned int command,
			  unsigned long argument)
{
	struct r52_gpu_file *ctx = file->private_data;
	struct r52_gpu *gpu = ctx->gpu;
	void __user *argp = (void __user *)argument;
	struct r52_gpu_completion completion;
	struct r52_gpu_launch launch;
	struct r52_gpu_info info;
	struct r52_gpu_resources resources;
	struct r52_gpu_shared *shared = gpu->shared_cpu;
	int ret = 0;

	if (_IOC_TYPE(command) != R52_GPU_IOC_MAGIC)
		return -ENOTTY;

	mutex_lock(&gpu->operation_lock);
	switch (command) {
	case R52_GPU_IOC_QUERY:
		ret = r52_gpu_query(gpu, &info);
		if (!ret && copy_to_user(argp, &info, sizeof(info)))
			ret = -EFAULT;
		break;
	case R52_GPU_IOC_SUBMIT:
		if (copy_from_user(&launch, argp, sizeof(launch))) {
			ret = -EFAULT;
			break;
		}
		if (!launch.grid_x || !launch.grid_y || !launch.grid_z ||
		    !launch.workgroup_x || !launch.workgroup_y ||
		    !launch.workgroup_z || launch.reserved) {
			ret = -EINVAL;
			break;
		}
		launch.sequence = r52_gpu_submit(gpu);
		ctx->submitted_sequence = launch.sequence;
		if (copy_to_user(argp, &launch, sizeof(launch)))
			ret = -EFAULT;
		break;
	case R52_GPU_IOC_GET_COMPLETION:
		if (!ctx->submitted_sequence ||
		    atomic64_read(&gpu->completed) < ctx->submitted_sequence) {
			ret = -EAGAIN;
			break;
		}
		rmb();
		completion.sequence = ctx->submitted_sequence;
		completion.start_ns = shared->signal.start_ts;
		completion.end_ns = shared->signal.end_ts;
		if (copy_to_user(argp, &completion, sizeof(completion)))
			ret = -EFAULT;
		break;
	case R52_GPU_IOC_GET_RESOURCES:
		memset(&resources, 0, sizeof(resources));
		resources.abi_version = R52_GPU_ABI_VERSION;
		resources.shared_memory_size = R52_GPU_SHARED_SIZE;
		resources.shared_dma = gpu->shared_dma;
		resources.mhu_region_size = R52_GPU_MHU_REGION_SIZE;
		if (copy_to_user(argp, &resources, sizeof(resources)))
			ret = -EFAULT;
		break;
	default:
		ret = -ENOTTY;
		break;
	}
	mutex_unlock(&gpu->operation_lock);
	return ret;
}

static __poll_t r52_gpu_poll(struct file *file, poll_table *wait)
{
	struct r52_gpu_file *ctx = file->private_data;
	struct r52_gpu *gpu = ctx->gpu;
	u64 completed;
	__poll_t mask = 0;

	poll_wait(file, &gpu->completion_wait, wait);
	if (readl(gpu->mbx + MHU_MBX_FFCW_ST(0)) & 0xffff)
		mask |= EPOLLIN | EPOLLRDNORM;
	completed = atomic64_read(&gpu->completed);
	if (completed > ctx->seen_completed) {
		ctx->seen_completed = completed;
		mask |= EPOLLIN | EPOLLRDNORM;
	}
	return mask;
}

static int r52_gpu_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct r52_gpu_file *ctx = file->private_data;
	struct r52_gpu *gpu = ctx->gpu;
	unsigned long size = vma->vm_end - vma->vm_start;
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
	phys_addr_t phys;

	if (!size)
		return -EINVAL;
	vm_flags_set(vma, VM_DONTEXPAND | VM_DONTDUMP);
	switch (offset) {
	case R52_GPU_MMAP_SHARED:
		if (size > R52_GPU_SHARED_SIZE)
			return -EINVAL;
		return dma_mmap_coherent(gpu->dev, vma, gpu->shared_cpu,
					 gpu->shared_dma, size);
	case R52_GPU_MMAP_PBX:
		if (size > R52_GPU_MHU_REGION_SIZE ||
		    MHU_PBX_FCH_PAY32(0) + size > gpu->pbx_size)
			return -EINVAL;
		phys = gpu->pbx_phys + MHU_PBX_FCH_PAY32(0);
		break;
	default:
		return -EINVAL;
	}

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	return io_remap_pfn_range(vma, vma->vm_start, phys >> PAGE_SHIFT,
				  size, vma->vm_page_prot);
}

static const struct file_operations r52_gpu_fops = {
	.owner = THIS_MODULE,
	.open = r52_gpu_open,
	.release = r52_gpu_release,
	.unlocked_ioctl = r52_gpu_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = compat_ptr_ioctl,
#endif
	.poll = r52_gpu_poll,
	.mmap = r52_gpu_mmap,
	.llseek = no_llseek,
};

static int r52_gpu_probe(struct platform_device *pdev)
{
	struct r52_gpu *gpu;
	struct resource *res;
	u32 stale;
	int ret;

	BUILD_BUG_ON(sizeof(struct r52_dispatch_packet) != 64);
	BUILD_BUG_ON(sizeof(struct r52_kernel_descriptor) != 64);
	BUILD_BUG_ON(sizeof(struct r52_signal) != 64);
	BUILD_BUG_ON(sizeof(struct r52_gpu_shared) > R52_GPU_SHARED_SIZE);

	gpu = devm_kzalloc(&pdev->dev, sizeof(*gpu), GFP_KERNEL);
	if (!gpu)
		return -ENOMEM;
	gpu->dev = &pdev->dev;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "pbx");
	if (!res)
		return -ENODEV;
	gpu->pbx_phys = res->start;
	gpu->pbx_size = resource_size(res);
	gpu->pbx = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(gpu->pbx))
		return PTR_ERR(gpu->pbx);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mbx");
	if (!res)
		return -ENODEV;
	gpu->mbx_phys = res->start;
	gpu->mbx_size = resource_size(res);
	gpu->mbx = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(gpu->mbx))
		return PTR_ERR(gpu->mbx);

	gpu->rx_irq = platform_get_irq_byname(pdev, "rx");
	if (gpu->rx_irq < 0)
		return gpu->rx_irq;

	ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
	if (ret)
		return ret;
	gpu->shared_cpu = dmam_alloc_coherent(&pdev->dev, R52_GPU_SHARED_SIZE,
					      &gpu->shared_dma, GFP_KERNEL);
	if (!gpu->shared_cpu)
		return -ENOMEM;
	gpu->shared_phys = (phys_addr_t)gpu->shared_dma;

	mutex_init(&gpu->operation_lock);
	init_completion(&gpu->fifo_completion);
	init_waitqueue_head(&gpu->completion_wait);
	atomic64_set(&gpu->submitted, 0);
	atomic64_set(&gpu->completed, 0);
	atomic_set(&gpu->opened, 0);
	platform_set_drvdata(pdev, gpu);

	writel(MHU_FF_CTRL_RA_EN, gpu->mbx + MHU_MBX_FFCW_CTRL(0));
	writel(MHU_FF_INT_ACK, gpu->mbx + MHU_MBX_FFCW_INT_EN(0));
	writel(MHU_FCH_CTRL_INT_EN, gpu->mbx + MHU_MBX_FCH_CTRL);
	writel(BIT(0), gpu->mbx + MHU_MBX_FCG_INT_EN);
	stale = readl(gpu->mbx + MHU_MBX_DBCW_ST(0));
	dev_info(&pdev->dev,
		 "mhu: rx_irq=%d pbx_iidr=0x%x mbx_iidr=0x%x dbch_cfg=0x%x ffch_cfg=0x%x fch_cfg=0x%x stale_db=0x%x\n",
		 gpu->rx_irq, readl(gpu->pbx + MHU_PBX_IIDR),
		 readl(gpu->mbx + MHU_MBX_IIDR),
		 readl(gpu->mbx + MHU_MBX_DBCH_CFG0),
		 readl(gpu->mbx + MHU_MBX_FFCH_CFG0),
		 readl(gpu->mbx + MHU_MBX_FCH_CFG0), stale);
	if (stale)
		writel(stale, gpu->mbx + MHU_MBX_DBCW_INT_CLR(0));
	writel(MHU_DB_KERNEL_COMPLETE, gpu->mbx + MHU_MBX_DBCW_INT_EN(0));

	ret = devm_request_irq(&pdev->dev, gpu->rx_irq, r52_gpu_irq, 0,
			       dev_name(&pdev->dev), gpu);
	if (ret)
		return ret;

	gpu->miscdev.minor = MISC_DYNAMIC_MINOR;
	gpu->miscdev.name = DRV_NAME;
	gpu->miscdev.fops = &r52_gpu_fops;
	gpu->miscdev.parent = &pdev->dev;
	gpu->miscdev.mode = 0600;
	ret = misc_register(&gpu->miscdev);
	if (ret)
		return ret;

	/* The R52 startup doorbell is level/state based; acknowledging is safe. */
	r52_gpu_send_doorbell(gpu, MHU_DB_STARTUP);
	dev_info(&pdev->dev, "/dev/%s ready, shared memory=%pa\n",
		 gpu->miscdev.name, &gpu->shared_phys);
	return 0;
}

static void r52_gpu_remove(struct platform_device *pdev)
{
	struct r52_gpu *gpu = platform_get_drvdata(pdev);

	misc_deregister(&gpu->miscdev);
}

static const struct of_device_id r52_gpu_of_match[] = {
	{ .compatible = "demo,r52-gpu-mhu" },
	{ }
};
MODULE_DEVICE_TABLE(of, r52_gpu_of_match);

static struct platform_driver r52_gpu_driver = {
	.probe = r52_gpu_probe,
	.remove_new = r52_gpu_remove,
	.driver = {
		.name = DRV_NAME,
		.of_match_table = r52_gpu_of_match,
	},
};
module_platform_driver(r52_gpu_driver);

MODULE_AUTHOR("Cortex R52 demo");
MODULE_DESCRIPTION("A76 Linux to R52 task scheduler demo driver");
MODULE_LICENSE("GPL");
