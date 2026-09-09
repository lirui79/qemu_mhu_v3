/********************************************************************************* 
**       This software is confidential and proprietary and may be used          **
**        only as expressly authorized by a licensing agreement from            **
**                                                                              **
**                            omnidimension                                     **
**                                                                              **
**                   (C) COPYRIGHT 2026 OMNIDIMENSION                           **
**                            ALL RIGHTS RESERVED                               **
**                                                                              **
**                 The entire notice above must be reproduced                   **
**                  on all copies and should not be removed.                    **
**                                                                              **
**********************************************************************************
**                         *.c vcd vcmd source code                             **
*********************************************************************************/

#include "cmdr52_mgr.h"
#include "vcd_vcmd_cfg.h"
#include "vcx_vcmd.h"
#include "vcx_vcmd_priv.h"
#include "vcx_vcmd_irq.h"
#include "vcx_cmdbuf_obj.h"
#include "vcx_kthread.h"

#ifdef VCMD_DEBUG_INTERNAL
#include "vcx_vcmd_dbg_log.h"
#endif

#ifdef SUPPORT_DBGFS
#include "vcx_vcmd_dbgfs.h"
#endif

#ifdef AXI2TO1_SUPPORT
#include "vcx_axi2to1.h"
#endif


/**
 * @brief allocate memory pools.
 * @return int: 0: succeed; other: failed.
 */
static int vcmd_init(vcmd_mgr_t *vcmd_mgr)
{
	u8 *va;
#if  0
#ifdef PCIE_EN
	struct noncache_mem *mem_pcie = &vcmd_mgr->pcie_pool;
	u32 offset;
	unsigned long pci_ddr_base = gBaseDDRHw;
#endif
#ifdef SUPPORT_MMU
	struct kernel_addr_desc mmu_addr;

	vcmd_mgr->mmu_enable = mmu_enable;
#endif
	vcmd_klog(LOGLVL_CONFIG, "MMU %s.\n", vcmd_mgr->mmu_enable ? "ENABLE" : "DISABLE");

#ifdef PCIE_EN
	mem_pcie->size = vcmd_mgr->mem_vcmd.size +
						vcmd_mgr->mem_status.size +
						vcmd_mgr->mem_regs.size;

	vcmd_mgr->pa_trans_offset = pci_ddr_base;

	/* allocate a single pool for all noncache_mems*/
	mem_pcie->pa = pci_ddr_base + ddr_offset + VCMD_BUF_POOL_OFFSET;
	if (!request_mem_region(mem_pcie->pa,
							mem_pcie->size,
							"vcx_vcmd_driver")) {
		vcmd_klog(LOGLVL_ERROR, "%s: failed to request vcmd pool memory region .\n", __func__);
		return -1;
	}
	vcmd_klog(LOGLVL_CONFIG, "Init: pcie pool pa=0x%llx.\n", (unsigned long long)mem_pcie->pa);
#if (KERNEL_VERSION(4, 17, 0) > LINUX_VERSION_CODE)
	mem_pcie->va = (void __force *)ioremap_nocache(mem_pcie->pa, mem_pcie->size);
#else
	mem_pcie->va = (void __force *)ioremap(mem_pcie->pa, mem_pcie->size);
#endif

	if (!mem_pcie->va) {
		vcmd_klog(LOGLVL_ERROR, "Init: failed to ioremap.\n");
		return -1;
	}
	vcmd_klog(LOGLVL_CONFIG, "%s: pcie pool va=0x%p.\n", __func__, mem_pcie->va);
#ifdef SUPPORT_MMU
	if (vcmd_mgr->mmu_enable) {
		mmu_addr.bus_address = mem_pcie->pa - pci_ddr_base;
		mmu_addr.size = mem_pcie->size;
		if (MMUKernelMemNodeMap(&mmu_addr, NULL) != MMU_STATUS_OK)
			return -1;
		mem_pcie->mmu_ba = mmu_addr.mmu_bus_address;
		vcmd_klog(LOGLVL_CONFIG, "%s: pool mmu_ba=0x%llx.\n", __func__,
			(unsigned long long)mem_pcie->mmu_ba);
	}
#endif

	/* layout this pool to noncache_mems*/
	offset = 0;
	va = (u8 *)mem_pcie->va;

	vcmd_mgr->mem_vcmd.pa = mem_pcie->pa + offset;
	vcmd_mgr->mem_vcmd.va = (u32 *)(va + offset);
	vcmd_mgr->mem_vcmd.mmu_ba = mem_pcie->mmu_ba + offset;
	offset += vcmd_mgr->mem_vcmd.size;

	vcmd_mgr->mem_status.pa = mem_pcie->pa + offset;
	vcmd_mgr->mem_status.va = (u32 *)(va + offset);
	vcmd_mgr->mem_status.mmu_ba = mem_pcie->mmu_ba + offset;
	offset += vcmd_mgr->mem_status.size;

	vcmd_mgr->mem_regs.pa = mem_pcie->pa + offset;
	vcmd_mgr->mem_regs.va = (u32 *)(va + offset);
	vcmd_mgr->mem_regs.mmu_ba = mem_pcie->mmu_ba + offset;


#else //PCIE_EN
	/* use dma_alloc_coherent for non-pcie env */
#if 0
	pci_ddr_base = alloc_base;
	g_vcmd_base_len = alloc_size * 1024 * 1024;
#else

	if (_vcmd_alloc_mem(vcmd_mgr, &vcmd_mgr->mem_vcmd))
		return -1;
	if (_vcmd_alloc_mem(vcmd_mgr, &vcmd_mgr->mem_status))
		return -1;
	if (_vcmd_alloc_mem(vcmd_mgr, &vcmd_mgr->mem_regs))
		return -1;
#endif
#endif //PCIE_EN
#endif

	return 0;
}


/**
 * @brief release submodule IO resource for vcmd driver
 */
static void vcmd_release_submodule_IO(struct vcmd_subsys_info *subsys, u32 sub_mod_id)
{
	if (subsys->hwregs[sub_mod_id]) {
/*		iounmap((volatile u8 __iomem *)subsys->hwregs[sub_mod_id]);
		release_mem_region(subsys->reg_base + subsys->reg_off[sub_mod_id],
			subsys->io_size[sub_mod_id]);
		subsys->hwregs[sub_mod_id] = NULL;*/
	}
}

/**
 * @brief reserve submodule IO resource for vcmd driver
 */
static u32 vcmd_reserve_submodule_IO(struct vcmd_subsys_info *subsys, u32 sub_mod_id)
{
	ptr_t pa = subsys->reg_base + subsys->reg_off[sub_mod_id];
	size_t sz = subsys->io_size[sub_mod_id];
/*
	if (!request_mem_region(pa, sz, "vcx_vcmd_driver"))
		return -EBUSY;

#if (KERNEL_VERSION(4, 17, 0) > LINUX_VERSION_CODE)
	subsys->hwregs[sub_mod_id] = (volatile u8 __force *)ioremap_nocache(pa, sz);
#else
	subsys->hwregs[sub_mod_id] = (volatile u8 __force *)ioremap(pa, sz);
#endif*/

	return 0;
}

/**
 * @brief initialize context of all vcmd dev
 */
static void dev_ctx_init(vcmd_mgr_t *vcmd_mgr)
{
	u32 i;
	struct hantrovcmd_dev *dev;
	u32 m_type;
	struct vcmd_module_mgr *module;

	memset(vcmd_mgr->dev_ctx,
			0, sizeof(struct hantrovcmd_dev) * vcmd_mgr->subsys_num);
	for (i = 0; i < vcmd_mgr->subsys_num; i++) {
		dev = &vcmd_mgr->dev_ctx[i];

		dev->handler = (void *)vcmd_mgr;
		dev->subsys_info = &vcmd_mgr->core_array[i];
		m_type = dev->subsys_info->sub_module_type;

		dev->core_id = i;
		dev->state = VCMD_STATE_POWER_ON;
		dev->sw_cmdbuf_rdy_num = 0;
		/* the abnormal interrupts source from VCD */
		dev->abn_irq_mask = DEC_ABN_IRQ_MASK;
		dev->vcd_abn_irq_mask = DEC_ABN_IRQ_MASK;
#ifdef AXI2TO1_SUPPORT
		/* the abnormal interrupts source from AXI2TO1.
		 * currently, the axi2to1 interrupt has been connect to abnormal by HW,
		 * SW does't need to set the interrupt type as abnormal
		 */
		dev->abn_irq_mask |= AXI2TO1_ABN_IRQ_MASK;
#endif
		dev->intr_gate_mask = 0xFFFFFFFF & (~dev->abn_irq_mask);

//		dev->timeout_timer_active = 0;
//		dev->watchdog_active = 0;

		dev->arb_reset_irq = 0;
		dev->arb_err_irq = 0;
		dev->mmu_enable = vcmd_mgr->mmu_enable;
		dev->abort_mode = 0;
		dev->init_mode = VCMD_INIT_NORMAL;

		dev->spinlock = &dev->owner_lock_vcmd;
		spin_lock_init(dev->spinlock);
		spin_lock_init(&dev->abn_irq_lock);

		init_bi_list(&dev->work_list);

		atomic_set(&dev->buff_empty_waitq, 0);

		dev->reg_mem_ba = vcmd_mgr->mem_regs.pa +
							i * SLOT_SIZE_REGBUF - vcmd_mgr->pa_trans_offset;
		dev->mmu_reg_mem_ba = vcmd_mgr->mem_regs.mmu_ba +
								i * SLOT_SIZE_REGBUF;
		dev->reg_mem_va = vcmd_mgr->mem_regs.va + i * SLOT_SIZE_REGBUF / 4;
		dev->reg_mem_sz = SLOT_SIZE_REGBUF;
#ifdef VCMD_ALLOC_MEM
		memset(dev->reg_mem_va, 0, dev->reg_mem_sz);
#endif
		dev->pa_trans_offset = vcmd_mgr->pa_trans_offset;

		module = &vcmd_mgr->module_mgr[m_type];
//		if (module->num == 0)
//			sema_init(&module->sem, 1);
		dev->id_in_type = module->num;
		module->dev[module->num++] = dev;

		vcmd_klog(LOGLVL_CONFIG, "module init - vcmdcore[%d] addr =0x%llx\n",
			i, (unsigned long long)dev->subsys_info->reg_base);
	}
}

/**
 * @brief fill cmdbuf to read main module's all regs.
 */
#define VCMD_READ_CMD(cmd, n, off, addr) \
	do { \
		(cmd)->opcode = OPCODE_RREG | RREG_MODE(RREG_ADDR_INC) | \
						RREG_LEN(n) | \
						RREG_START_ADDR(off); \
		CMD_SET_ADDR(cmd, (addr)+(off)); \
		(cmd)->padding = 0; \
		cmd++; \
	} while (0)

static void create_read_all_registers_cmdbuf(vcmd_mgr_t *vcmd_mgr,
											struct exchange_parameter *param)
{
	struct hantrovcmd_dev *dev;
	struct vcmd_subsys_info *subsys;
	struct cmd_rreg_t *cmd_rreg;
	struct cmd_end_t *cmd_end;
	u8 *p_cmdbuf;
	ptr_t reg_ba;

	dev = &vcmd_mgr->dev_ctx[param->core_id];
	subsys = dev->subsys_info;

	reg_ba = dev->reg_mem_ba;
	if (dev->mmu_enable)
		reg_ba = (ptr_t)dev->mmu_reg_mem_ba;

	p_cmdbuf = (u8 *)vcmd_mgr->mem_vcmd.va + CMDBUF_OFF(param->cmdbuf_id);

	cmd_rreg = (struct cmd_rreg_t *)(p_cmdbuf + 0);
	if (dev->hw_version_id > HW_ID_1_0_C) {
		//read vcmd executing cmdbuf id registers to ddr for balancing core load.
		VCMD_READ_CMD(cmd_rreg, 1, REG_ID_CMDBUF_EXE_ID * 4, 0);
	}
	//read main IP all registers
	VCMD_READ_CMD(cmd_rreg,
				subsys->io_size[SUB_MOD_MAIN] / 4,
				subsys->reg_off[SUB_MOD_MAIN] + 0,
				reg_ba);

#ifdef SUPPORT_AXIFE
	if (subsys->io_size[SUB_MOD_AXIFE])
		VCMD_READ_CMD(cmd_rreg, 1,
					subsys->reg_off[SUB_MOD_AXIFE] + 0,
					reg_ba);
#endif

	if (dev->hw_version_id > HW_ID_1_0_C) {
		//read vcmd registers to ddr, to compliant with user cmdbuf
		VCMD_READ_CMD(cmd_rreg, 27, 0, 0);
	}
	//end cmd
	cmd_end = (struct cmd_end_t *)cmd_rreg;
	cmd_end->opcode = OPCODE_END;
	cmd_end->padding = 0;
	cmd_end++;

	param->cmdbuf_size = (u8 *)cmd_end - p_cmdbuf;
}

/**
 * @brief config modules for vcmd driver
 */
static int vcmd_config_modules(vcmd_mgr_t *vcmd_mgr)
{
	int i;
	struct vcmd_subsys_info *subsys;
#ifdef SUPPORT_MMU
	enum MMUStatus mmu_status = MMU_STATUS_FALSE;
	int ret = 0;
#endif

	for (i = 0; i < vcmd_mgr->subsys_num; i++) {
		subsys = &vcmd_mgr->core_array[i];
		/* config AXIFE*/
#ifdef SUPPORT_AXIFE
		if (subsys->hwregs[SUB_MOD_AXIFE])
			AXIFEEnable(subsys->hwregs[SUB_MOD_AXIFE], 1);
//		if (subsys->hwregs[SUB_MOD_AXIFE1])
//			AXIFEEnable(subsys->hwregs[SUB_MOD_AXIFE1], 1);
#endif
		/* config AXI2TO1 */
#ifdef AXI2TO1_SUPPORT
		if (subsys->hwregs[SUB_MOD_AXI2TO1]) {
			if (AXI2TO1_init(subsys->hwregs[SUB_MOD_AXI2TO1]) < 0)
				return -5;
		}
#endif
	}
	return 0;
}

/**
 * @brief reserve IO resources for vcmd driver.
 */
static int vcmd_reserve_IO(vcmd_mgr_t *vcmd_mgr)
{
	u32 hwid;
	int i, j;
	u32 found_hw = 0;
	struct hantrovcmd_dev *dev;
	struct vcmd_subsys_info *subsys;
	u32 reg_val;
	int ret, has_arbiter;

	for (i = 0; i < vcmd_mgr->subsys_num; i++) {
		dev = &vcmd_mgr->dev_ctx[i];
		subsys = dev->subsys_info;
		dev->hwregs = NULL;

		for (j = SUB_MOD_VCMD; j < SUB_MOD_MAX; j++) {
			if (subsys->reg_off[j] == 0xffff)
				continue;
			ret = vcmd_reserve_submodule_IO(subsys, j);
			if (ret < 0) {
				vcmd_klog(LOGLVL_ERROR,
						  "failed to reserve submodule[%d] HW regs for vcmd %d\n",
						  j, i);
				vcmd_klog(LOGLVL_ERROR,
							"vcmd_base_addr = 0x%llx, iosize = %d\n",
							subsys->reg_base, subsys->io_size[j]);
				continue;
			}
			if (!subsys->hwregs[j]) {
				vcmd_klog(LOGLVL_ERROR,
							"failed to ioremap submodule[%d] HW regs\n",
							j);
				//release_mem_region(
				//	subsys->reg_base, subsys->io_size[j]);
				continue;
			}
		}

		dev->hwregs = subsys->hwregs[SUB_MOD_VCMD];
		vcmd_mgr->mmu_hwregs[i][0] = subsys->hwregs[SUB_MOD_MMU];
		vcmd_mgr->mmu_hwregs[i][1] = subsys->hwregs[SUB_MOD_MMU_WR];

		/*read hwid and check validness and store it*/
		hwid = (u32)ioread32((void __iomem *)dev->hwregs + 0x0);
		vcmd_klog(LOGLVL_CONFIG, "%s: hantrovcmd [%d].hwregs=0x%p hwid=0x%08x\n\n",
					__func__, i, dev->hwregs, hwid);

		/* check for vcmd HW ID */
		if (((hwid >> 16) & 0xFFFF) != VCMD_HW_ID) {
			vcmd_klog(LOGLVL_WARNING, "HW not found at 0x%llx\n",
				(unsigned long long)subsys->reg_base);
			//iounmap((void __iomem *)dev->hwregs);
			//release_mem_region(subsys->reg_base,
			//					subsys->io_size[SUB_MOD_VCMD]);
			dev->hwregs = NULL;
			continue;
		}

		dev->hw_version_id = hwid;
		reg_val = (u32)ioread32((void __iomem *)dev->hwregs +
				   VCMD_REGISTER_HW_APB_ARBITER_MODE_OFFSET);
		has_arbiter = (u8)((reg_val >> HW_APB_ARBITER_MODE_BIT) & 0x01);
		dev->hw_feature.vcarb_ver = 0;
		if (has_arbiter)
			dev->hw_feature.vcarb_ver = hwid < HW_ID_1_5_9 ? VCARB_VERSION_2_0 :
										VCARB_VERSION_3_0;

		reg_val = (u32)ioread32((void __iomem *)dev->hwregs +
				   VCMD_REGISTER_HW_INIT_MODE_OFFSET);
		dev->hw_feature.has_init_mode = (u8)((reg_val >> HW_INIT_MODE_BIT) &
										0x01);
		dev->hw_feature.has_cmdbuf_timeout = 0;
		if (hwid >= HW_ID_1_5_10) {
			dev->hw_feature.has_cmdbuf_timeout = 1;
			dev->abn_irq_mask &= ~DEC_ABN_IRQ_MASK;
			dev->abn_irq_mask |= CORE_IP_ABN_IRQ_MASK;
			dev->vcd_abn_irq_mask = CORE_IP_ABN_IRQ_MASK;
			dev->intr_gate_mask = 0xFFFFFFFF & (~dev->abn_irq_mask);
		}
#ifdef HANTROVCMD_ENABLE_IP_SUPPORT
		if (dev->hw_feature.has_init_mode)
			dev->init_mode = DEFAULT_VCMD_INIT_MODE;
#endif

		found_hw = 1;
		vcmd_klog(LOGLVL_CONFIG, "HW at base <0x%llx> with ID <0x%08x>\n",
					(unsigned long long)subsys->reg_base, hwid);
	}

	if (found_hw == 0) {
		vcmd_klog(LOGLVL_ERROR, "NO ANY HW found!!\n");
		return -1;
	}

	return 0;
}

/**
 * @brief release IO resources of vcmd driver.
 */
static void vcmd_release_IO(vcmd_mgr_t *vcmd_mgr)
{
	u32 i;
	struct vcmd_subsys_info *subsys;

	for (i = 0; i < vcmd_mgr->subsys_num; i++) {
		subsys = &vcmd_mgr->core_array[i];
		vcmd_release_submodule_IO(subsys, SUB_MOD_VCMD);
		vcmd_mgr->dev_ctx[i].hwregs = NULL;
	}
}

/**
 * @brief reserve irq for vcmd driver.
 */
static int vcmd_reserve_irq(vcmd_mgr_t *vcmd_mgr)
{
	u32 i;
	int result;
	struct hantrovcmd_dev *dev;
	struct vcmd_subsys_info *subsys;


	for (i = 0; i < vcmd_mgr->subsys_num; i++) {
		dev = &vcmd_mgr->dev_ctx[i];
		subsys = dev->subsys_info;

		if (!dev->hwregs)
			continue;

		if (subsys->irq == -1) {
			vcmd_klog(LOGLVL_CONFIG, "vcmd[%d]: IRQ not in use!\n", i);
			continue;
		}
/*
		result = request_irq(subsys->irq,
							hantrovcmd_isr,
#if (KERNEL_VERSION(2, 6, 18) > LINUX_VERSION_CODE)
							SA_INTERRUPT | SA_SHIRQ,
#else
							IRQF_SHARED,
#endif
							"vc8000_vcmd_driver",
							(void *)vcmd_mgr);*/

		if (result == -EINVAL) {
			vcmd_klog(LOGLVL_ERROR, "vcmd[%d]: Bad vcmd_irq number or handler.\n", i);
			return -1;
		} else if (result == -EBUSY) {
			vcmd_klog(LOGLVL_ERROR, "vcmd[%d]: IRQ <%d> is occupied!!!\n", i, subsys->irq);
			return -1;
		} else {
			vcmd_klog(LOGLVL_ERROR, "vcmd[%d]: request IRQ <%d> succeed\n", i, subsys->irq);
			vcmd_mgr->vcmd_irq_enabled = 1;
		}
	}
	return 0;
}

/**
 * @brief get a device submodule regs virtual address
 */
u32 *get_submodule_regs_va(void *_vcmd_mgr, u32 subsys_id, u32 sub_mod_id)
{
	u32 *regs_va;
	vcmd_mgr_t *vcmd_mgr = (vcmd_mgr_t *)_vcmd_mgr;
	struct hantrovcmd_dev *dev = &vcmd_mgr->dev_ctx[subsys_id];

	regs_va = dev->reg_mem_va +
				dev->subsys_info->reg_off[sub_mod_id] / 4 + 0;

	return regs_va;
}

/**
 * @brief read main module's all regs for all vcmd hw devices.
 */
static void read_main_module_all_registers(vcmd_mgr_t *vcmd_mgr)
{
	int ret;
	struct exchange_parameter param[MAX_SUBSYS_NUM];
	u16 done_id = 0;
	u32 i, irq;
	struct hantrovcmd_dev *dev;
	struct vcmd_module_mgr *module;
	u32 *main_regs_va;
	cmdr52_session_t *cmdr52_session = NULL;

	cmdr52_session = cmdr52_mgr_get_session(0x0);

	for (i = 0; i < vcmd_mgr->subsys_num; i++) {
		dev = &vcmd_mgr->dev_ctx[i];
		param[i].interrupt_ctrl = 0;//		param[i].executing_time = 0;
		param[i].input_mask = 0;
		param[i].cmdbuf_size = 0;
		param[i].module_type = dev->subsys_info->sub_module_type;
		param[i].core_mask = 1 << dev->core_id;
		param[i].core_id = dev->core_id;
		/* normal priority and last cmd is end */
		EXCH_S_BIT(param[i].input_mask, EXCH_END_CMD_BIT);
		module = &vcmd_mgr->module_mgr[param[i].module_type];

		ret = reserve_cmdbuf(vcmd_mgr, cmdr52_session, &param[i]);
		create_read_all_registers_cmdbuf(vcmd_mgr, &param[i]);
		link_and_run_cmdbuf(vcmd_mgr, cmdr52_session, &param[i]);
	}

	/* make sure vcmd can complete job, and clear irq
	 */
	if (vcmd_mgr->vcmd_irq_enabled == 0) {
	//   vTaskDelay(pdMS_TO_TICKS(100));
	}

	//	msleep(100);
	for (i = 0; i < vcmd_mgr->subsys_num; i++) {
		dev = &vcmd_mgr->dev_ctx[i];

		if (vcmd_mgr->vcmd_irq_enabled == 0) {
			irq = dev->core_id;
			hantrovcmd_isr(irq, vcmd_mgr);
		}

		vcmd_wait_cmdbuf_ready(vcmd_mgr, param[i].cmdbuf_id, &done_id);
		main_regs_va = dev->reg_mem_va +
				dev->subsys_info->reg_off[SUB_MOD_MAIN] / 4 + 0;
		vcmd_klog(LOGLVL_CONFIG, "main module register 0:0x%x\n",
			*main_regs_va);
		vcmd_klog(LOGLVL_CONFIG, "main module register 50:0x%08x\n",
			*(main_regs_va + 50));
		vcmd_klog(LOGLVL_CONFIG, "main module register 54:0x%08x\n",
			*(main_regs_va + 54));
		vcmd_klog(LOGLVL_CONFIG, "main module register 56:0x%08x\n",
			*(main_regs_va + 56));
		vcmd_klog(LOGLVL_CONFIG, "main module register 309:0x%x\n",
			*(main_regs_va + 309));

		vcmd_release_cmdbuf(vcmd_mgr, param[i].cmdbuf_id);
	}
}

/**
 * @brief convert subsys info to core array info of vcmd driver context.
 */
static void SubsysToVcmdCoreCfg(vcmd_mgr_t *vcmd_mgr)
{
	int i, k, array_sz;
	struct vcmd_subsys_info *subsys;
	struct sub_mod_cfg *mod_cfg;
	enum subsys_module_id mod_id;

	array_sz = ARRAY_SIZE(vcmd_core_array);

	/* To plug into vcx_vcmd_driver.c */
	for (i = 0; i < array_sz; i++) {
		if (vcmd_core_array[i].vcmd_base_addr) {
			subsys = &vcmd_mgr->core_array[vcmd_mgr->subsys_num];
			subsys->irq = vcmd_core_array[i].vcmd_irq;
			/* reg_base = vcmd_base_addr + reg_base_offset,
			 * but now reg_base_offset is 0, will be added later
			 */
			subsys->reg_base = vcmd_core_array[i].vcmd_base_addr +
							   /* vcmd_mgr->reg_base_offset */ 0;
			subsys->sub_module_type = vcmd_core_array[i].sub_module_type;
			subsys->vcmd_priority = vcmd_core_array[i].priority;

			for (k = 0; k < SUB_MOD_MAX; k++) {
				mod_cfg = &vcmd_core_array[i].submodule_cfg[k];
				mod_id = mod_cfg->sub_mod_id;
				if (mod_id < SUB_MOD_MAX) {
					subsys->reg_off[mod_id] = mod_cfg->io_off;
					if (mod_cfg->io_off != 0xffff) {
						subsys->io_size[mod_id] = mod_cfg->io_size;
						subsys->rreg_id[mod_id] = mod_cfg->rreg_id;
						subsys->rreg_num[mod_id] = mod_cfg->rreg_num;
					}
				} else {
					vcmd_klog(LOGLVL_ERROR, "wrong sub-module id in vcmd_core_array[%d].submodule_cfg[%d]\n",
							  i, k);
				}
			}

			vcmd_mgr->subsys_num++;
		}
	}
	vcmd_klog(LOGLVL_CONFIG, "%d VCMD cores found\n", vcmd_mgr->subsys_num);
}

/**
 * @brief vcmd driver initialization
 * @param struct subsys_config *subsys: info of sub-systems.
 * @param int subsys_num: number of sub-systems.
 * @param void *platformdev: platform device handler
 * @return void *: NULL: failed; other: vcmd driver handler.
 */
int vcd_vcmd_init(vcmd_mgr_t **_vcmd_mgr)
{
	int result = 0;
	vcmd_mgr_t *vcmd_mgr;
	struct hantrovcmd_dev *dev_ctx;

	vcmd_mgr = ddr_alloc(sizeof(vcmd_mgr_t));
	if (!vcmd_mgr)
		return -1;
	memset(vcmd_mgr, 0, sizeof(vcmd_mgr_t));

	SubsysToVcmdCoreCfg(vcmd_mgr);
	vcmd_mgr->vcmd_mgr_id = VCMD_MGR_ID_DEC;

	vcmd_mgr->mem_vcmd.size = ALIGN_4K(SLOT_NUM_CMDBUF * SLOT_SIZE_CMDBUF);
	vcmd_mgr->mem_status.size = ALIGN_4K(SLOT_NUM_CMDBUF * SLOT_SIZE_STATUSBUF);
	vcmd_mgr->mem_regs.size = ALIGN_4K(vcmd_mgr->subsys_num * SLOT_SIZE_REGBUF);
	result = vcmd_init(vcmd_mgr);
	if (result)
		goto err1;

	spin_lock_init(&vcmd_mgr->job_lock);
	init_bi_list(&vcmd_mgr->job_done_list);

	dev_ctx = ddr_alloc(sizeof(struct hantrovcmd_dev) * vcmd_mgr->subsys_num);
	if (!dev_ctx)
		goto err1;
	memset(dev_ctx, 0, sizeof(struct hantrovcmd_dev) * vcmd_mgr->subsys_num);

	vcmd_mgr->dev_ctx = dev_ctx;
	dev_ctx_init(vcmd_mgr);
#ifdef VCMD_ALLOC_MEM
	result = vcmd_reserve_IO(vcmd_mgr);
	if (result < 0)
		goto err;

	result = vcmd_config_modules(vcmd_mgr);
	if (result < 0)
		goto err;

	vcmd_reset_asic(vcmd_mgr);
#endif

#ifdef SUPPORT_DBGFS
	/* for debugfs */
	if (_dbgfs_init((void *)vcmd_mgr))
		goto err;
	_dbgfs_init_ctx((void *)vcmd_mgr, 0);
#endif

#ifdef VCMD_ALLOC_MEM
	/* get the IRQ line */
	result = vcmd_reserve_irq(vcmd_mgr);
	if (result < 0) {
		vcmd_release_IO(vcmd_mgr);
		goto err;
	}
#endif

	vcmd_init_objs(vcmd_mgr);
	vcmd_init_nodes(vcmd_mgr);

	/* create vcmd kthread, which need to be woken up */
//	_vcmd_kthread_create(vcmd_mgr);

	/* read all registers of main-module for each dev
	 * for analyzing configuration in cwl
	 */
#ifdef VCMD_ALLOC_MEM
	read_main_module_all_registers(vcmd_mgr);
#endif

	*_vcmd_mgr = (vcmd_mgr_t *)vcmd_mgr;
	vcmd_klog(LOGLVL_CONFIG, "vcd_vcmd_init\n");
	return 0;

err:
	vcmd_release_IO(vcmd_mgr);
err1:
	if (vcmd_mgr->dev_ctx)
		ddr_free(vcmd_mgr->dev_ctx);
	if (vcmd_mgr)
		ddr_free(vcmd_mgr);
	vcmd_klog(LOGLVL_ERROR, "module not inserted!\n");
	return result;
}

/**
 * @brief de-init vcmd driver.
 */
void vcd_vcmd_exit(vcmd_mgr_t *vcmd_mgr)
{
	int i = 0;
	struct hantrovcmd_dev *dev_ctx = vcmd_mgr->dev_ctx;

//	_vcmd_kthread_stop(vcmd_mgr);

	for (i = 0; i < vcmd_mgr->subsys_num; i++) {
		if (!dev_ctx[i].hwregs)
			continue;
	}

#ifdef SUPPORT_DBGFS
	_dbgfs_cleanup((void *)vcmd_mgr);
#endif
	vcmd_release_IO(vcmd_mgr);
	ddr_free(dev_ctx);

/*
	//release_vcmd_non_cachable_memory();
#ifdef PCIE_EN
	_vcmd_pcie_cleanup(vcmd_mgr);
#else
	_vcmd_free_mem(vcmd_mgr, &vcmd_mgr->mem_vcmd);
	_vcmd_free_mem(vcmd_mgr, &vcmd_mgr->mem_status);
	_vcmd_free_mem(vcmd_mgr, &vcmd_mgr->mem_regs);
#endif*/

	ddr_free(vcmd_mgr);
	vcmd_klog(LOGLVL_FLOW, "module removed\n");
}


