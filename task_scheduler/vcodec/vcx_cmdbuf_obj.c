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
**                         *.c vcx cmdbuf obj source code                       **
*********************************************************************************/


#include "cmdr52_mgr.h"
#include "vcx_vcmd_irq.h"
#include "vcx_vcmd_defs.h"
#include "vcx_cmdbuf_obj.h"
#include "vcx_vcmd_dbgfs.h"
#include "vcx_vcmd_dbg_log.h"


/*---------------------------------------------------------------
 * Macros related to sub-system config
 *---------------------------------------------------------------
 */
/* VCMD master_out_clk mode */
#define APB_CLK_ON	(1)
#define APB_CLK_OFF	(0)
#define APB_CLK_MODE	APB_CLK_ON


static int dev_remove_job(vcmd_mgr_t *vcmd_mgr, struct hantrovcmd_dev *dev, bi_list_node *node);

static int _is_abnormal_run_done(struct cmdbuf_obj *obj);

static void _abnormal_run_done_clear(struct cmdbuf_obj *obj);

/*======================= cmdbuf object management ================*/
/**
 * @brief initialize all of cmdbuf objs of vcmd driver handler.
 */
void vcmd_init_objs(vcmd_mgr_t *vcmd_mgr)
{
	u32 i;
	struct cmdbuf_obj *obj;
	struct noncache_mem *m0, *m1;

	m0 = &vcmd_mgr->mem_vcmd;
	m1 = &vcmd_mgr->mem_status;
	for (i = 0; i < SLOT_NUM_CMDBUF; i++) {
		obj = &vcmd_mgr->objs[i];
		obj->cmdbuf_id = i;
		obj->cmdbuf_size = SLOT_SIZE_CMDBUF;
		obj->cmd_va = m0->va + CMDBUF_OFF_32(i);
		obj->cmd_pa = m0->pa + CMDBUF_OFF(i);
		obj->mmu_cmd_ba = m0->mmu_ba + CMDBUF_OFF(i);

		obj->status_size = SLOT_SIZE_STATUSBUF;
		obj->status_va = m1->va + STATUSBUF_OFF_32(i);
		obj->status_pa = m1->pa + STATUSBUF_OFF(i);
		obj->mmu_status_ba = m1->mmu_ba + STATUSBUF_OFF(i);
	}
}

/**
 * @brief initialize all of cmdbuf nodes of vcmd driver handler.
 */
void vcmd_init_nodes(vcmd_mgr_t *vcmd_mgr)
{
	u32 i;

	for (i = 0; i < SLOT_NUM_CMDBUF; i++)
		vcmd_mgr->nodes[i].data = (void *)&vcmd_mgr->objs[i];
}

/**
 * @brief reset flag/status of cmdbuf obj specified by id.
 */
static void reset_cmdbuf_obj(vcmd_mgr_t *vcmd_mgr, u32 id)
{
	struct cmdbuf_obj *obj = &vcmd_mgr->objs[id];

	obj->executing_status = CMDBUF_EXE_STATUS_OK;
	obj->cmdbuf_run_done = 0;
	obj->slice_run_done = 0;
	obj->line_buffer_run_done = 0;
	obj->cmdbuf_linked = 0;
	obj->cmdbuf_need_remove = 0;
	obj->core_id = 0xFFFF;
	obj->cmdbuf_size = SLOT_SIZE_CMDBUF;

	obj->has_jmp_cmd = 1;
	obj->jmp_ie = 0;
}

/**
 * @brief reset pointer of cmdbuf node specified by id.
 */
static void reset_cmdbuf_node(vcmd_mgr_t *vcmd_mgr, u32 id)
{
	struct bi_list_node  *node;

	node = &vcmd_mgr->nodes[id];
	node->next = NULL;
	node->prev = NULL;

	node = &vcmd_mgr->po_jobs[id];
	node->next = NULL;
	node->prev = NULL;
	node->data = NULL;
}

/**
 * @brief get cmdbuf node by cmd physical addr.
 * @param ptr_t pa: the cmd physical addr.
 * @return bi_list_node *: NULL: no cmdbuf found; otherwise: cmdbuf node.
 */
bi_list_node *get_cmdbuf_node_by_addr(vcmd_mgr_t *vcmd_mgr,
					struct hantrovcmd_dev *dev, ptr_t pa)
{
	bi_list_node *node;
	struct cmdbuf_obj *obj;

	u32 id;
	ptr_t pa_base = vcmd_mgr->mem_vcmd.pa - vcmd_mgr->pa_trans_offset;

	id = (pa - pa_base) / SLOT_SIZE_CMDBUF;
	if (id >= SLOT_NUM_CMDBUF) {
		vcmd_klog(LOGLVL_ERROR, "cmdbuf_id greater than the ceiling !!\n");
		return NULL;
	}

	obj = &vcmd_mgr->objs[id];
	node = &vcmd_mgr->nodes[id];

	if (obj->core_id != dev->core_id) {
		vcmd_klog(LOGLVL_ERROR, "cmdbuf is not in dev[%d] list !!\n", dev->core_id);
		return NULL;
	}

	return node;
}

/**
 * @brief get the va of vcmdbuf obj's JMP cmd.
 */
struct cmd_jmp_t *_get_jmp_cmd(struct cmdbuf_obj *obj)
{
	u8 *p = (u8 *)obj->cmd_va;

	p += obj->cmdbuf_size - sizeof(struct cmd_jmp_t);
	return (struct cmd_jmp_t *)p;
}

/**
 * @brief get the va of vcmdbuf obj's END cmd.
 */
struct cmd_end_t *_get_end_cmd(struct cmdbuf_obj *obj)
{
	u8 *p = (u8 *)obj->cmd_va;

	p += obj->cmdbuf_size - sizeof(struct cmd_end_t);
	return (struct cmd_end_t *)p;
}

/**
 * @brief set bus-address to 2 rreg cmd with dev reg mem ba,
 * the 2 rreg cmds are the 1st & the last but one cmd of specified cmdbuf.
 */
static void _set_rreg_addr(struct hantrovcmd_dev *dev, struct cmdbuf_obj *obj)
{
	ptr_t reg_mem_ba;
	struct cmd_rreg_t *cmd_rreg;
	u8 *p;

	if (dev->hw_version_id <= HW_ID_1_0_C)
		return;

	reg_mem_ba = dev->reg_mem_ba;
	if (dev->mmu_enable)
		reg_mem_ba = (ptr_t)dev->mmu_reg_mem_ba;

	//read vcmd executing ID register into ddr memory.
	cmd_rreg = (struct cmd_rreg_t *)obj->cmd_va;
	CMD_SET_ADDR(cmd_rreg, reg_mem_ba + REG_ID2_CMDBUF_EXE_ID * 4);

	//read vcmd all registers into ddr memory.
	if (obj->has_jmp_cmd)
		p = (u8 *)_get_jmp_cmd(obj);
	else
		p = (u8 *)_get_end_cmd(obj);
	cmd_rreg = (struct cmd_rreg_t *)(p - sizeof(struct cmd_rreg_t));
	CMD_SET_ADDR(cmd_rreg, reg_mem_ba);
}

/**
 * @brief update intr-enable bit of JMP cmd in specified cmdbuf.
 */
static void _update_jmp_ie(struct hantrovcmd_dev *dev, struct cmdbuf_obj *obj)
{
	struct cmd_jmp_t *cmd_jmp;
	u32 maxworkload = VCMD_WORKLOAD_UNIT * 8;
	if (obj->module_type == VCMD_TYPE_ENCODER)
	    maxworkload = VCMD_WORKLOAD_UNIT * 1;
	if (obj->module_type == VCMD_TYPE_DECODER)
	    maxworkload = VCMD_WORKLOAD_UNIT * 8;

	if (obj->has_jmp_cmd) {
		//update dev->duration and adjust JMP_IE accordingly
		if (obj->jmp_ie) {
			dev->duration = 0;
		} else {
			dev->duration += obj->workload;
			if (dev->duration >= maxworkload) {
				cmd_jmp = _get_jmp_cmd(obj);
				obj->jmp_ie = 1;
				cmd_jmp->opcode |= JMP_IE(1);
			}
		}
	}
}

/**
 * @brief link 2 specified cmdbufs' data by JMP cmd of prev comdbuf.
 */
static void dev_link_cmdbuf(struct hantrovcmd_dev *dev,
								bi_list_node *prev_node,
								bi_list_node *next_node)
{
	struct cmdbuf_obj *next_obj, *prev_obj;
	struct cmd_jmp_t *cmd_jmp;
	ptr_t next_ba;
	u32 op;

	if (!prev_node)
		return;

	prev_obj = (struct cmdbuf_obj *)prev_node->data;

	if (prev_obj->has_jmp_cmd) {
		cmd_jmp = _get_jmp_cmd(prev_obj);
		if (!next_node) {
			// If next cmdbuf is not available, set the RDY to 0.
			op = cmd_jmp->opcode;

			cmd_jmp->opcode = OPCODE_JMP |
								JMP_RDY(0) | JMP_IE(JMP_G_IE(op)) |
								JMP_NEXT_LEN(0);
		} else {
			next_obj = (struct cmdbuf_obj *)next_node->data;
			if (dev->hw_version_id > HW_ID_1_0_C) {
				//set next cmdbuf id
				cmd_jmp->id = next_obj->cmdbuf_id;
			}
			next_ba = next_obj->cmd_pa - dev->pa_trans_offset;
			if (dev->mmu_enable)
				next_ba = (ptr_t)next_obj->mmu_cmd_ba;

			CMD_SET_ADDR(cmd_jmp, next_ba);
			op = cmd_jmp->opcode;
			cmd_jmp->opcode = OPCODE_JMP |
								JMP_RDY(1) | JMP_IE(JMP_G_IE(op)) |
								JMP_NEXT_LEN((next_obj->cmdbuf_size + 7) / 8);
		}
	}

#ifdef VCMD_DEBUG_INTERNAL
	_dbg_log_last_cmd(prev_obj);
#endif
}

/**
 * @brief return cmdbuf obj's workload to process object.
 */
static void return_process_resource(cmdr52_session_t *session, struct cmdbuf_obj *obj)
{
	if (session && obj->workload) {
		spin_lock(&session->spinlock);
		session->total_workload -= obj->workload;
		spin_unlock(&session->spinlock);
		obj->workload = 0;//xSemaphoreGive(po->resource_waitq);//wake_up_interruptible_all(&po->resource_waitq);
	}
}

/**
 * @brief add done obj to job_done_list of its po, wake-up job_waitq if needed.
 */
void proc_add_done_job(vcmd_mgr_t *vcmd_mgr, struct cmdbuf_obj *obj)
{
	u16 id = obj->cmdbuf_id;
	struct bi_list *list;
	uint32_t   flags;
	u32 is_empty, is_wait;
	struct hantrovcmd_dev *dev = NULL;

	if (!obj->session) {
		vcmd_klog(LOGLVL_BRIEF, "%s: the po and cmdbufs of this po has been released!\n",
						__func__);
		return;
	}

	list = &vcmd_mgr->job_done_list;
	dev = &vcmd_mgr->dev_ctx[obj->core_id];
	if (obj->module_type == VCMD_TYPE_DECODER) {
		atomic_inc(&dev->buff_empty_waitq);
	}

	flags = spin_lock_irqsave(&vcmd_mgr->job_lock);

	if (vcmd_mgr->po_jobs[id].data) {
		//already in job-done list, do nothing
		spin_unlock_irqrestore(&vcmd_mgr->job_lock, flags);
		return;
	}

	vcmd_mgr->po_jobs[id].data = (void *)&vcmd_mgr->objs[id];
	is_empty = (list->head == NULL);
	is_wait = vcmd_mgr->in_wait;
	vcmd_mgr->in_wait = 0;
	bi_list_insert_node_tail(list, &vcmd_mgr->po_jobs[id]);
	spin_unlock_irqrestore(&vcmd_mgr->job_lock, flags);

	{
		cmdr52_mgr_t *mgr = (cmdr52_mgr_t*) cmdr52_mgr_get();
		cmdEvtIntIrqVpu_Body_t *cmdBody = NULL;
		cmdMsg_t *cmdMsg = NULL;
		cmdMsg = BQueueDequeueFromISR(mgr->cmd_queue);
		cmdBody = (cmdEvtIntIrqVpu_Body_t *)cmdMsg->data;
		cmd_init(cmdMsg);
		cmdMsg->cmdType = CMD_EVT_INTIRQ_VCODEC;
		cmdMsg->cmdSize = CMD_MSG_MIN_SIZE + sizeof(cmdEvtIntIrqVpu_Body_t);// command size  (include cmd header and body)
		cmdMsg->sessionID = 0xFFFFFFFF;// session id
		cmdMsg->timeStamp = 0xFFFFFFFF;// time stamp, default current time in ms
		cmdMsg->seqNum    = 0x00;
		cmdBody->vcmdmgr_id  = vcmd_mgr->vcmd_mgr_id;
		cmdBody->cmdbuf_id  = obj->cmdbuf_id;
		BQueueQueueFromISR(mgr->cmd_queue, cmdMsg);
	}
}

/**
 * @brief get & remove a done obj from po's job_done_list.
 * @param struct cmdbuf_obj **pobj: *pobj==NULL: get head node from list.
 *									otherwise, get specified node from list.
 * @return int: 0: no done obj; 1: obj is done.
 */
static int proc_get_done_job(vcmd_mgr_t *vcmd_mgr, cmdr52_session_t *session, struct cmdbuf_obj **pobj)
{

	struct bi_list *list = &vcmd_mgr->job_done_list;
	bi_list_node *node;
	uint32_t   flags;
	struct cmdbuf_obj *obj = NULL;
	int is_done = 0, i = 0, run_done = 0;

	flags = spin_lock_irqsave(&vcmd_mgr->job_lock);
	node = list->head;

	if (node == NULL) {
		// job done list is empty
		vcmd_mgr->in_wait = 1;
		for (i = 0; i < SLOT_NUM_CMDBUF; i++) {
			obj = &vcmd_mgr->objs[i];
			if (obj->session == session)
				break;
		}
		if (i == SLOT_NUM_CMDBUF) {
			spin_unlock_irqrestore(&vcmd_mgr->job_lock, flags);
			return 1; /* used to drop_cmd_ids!=NULL when seek */
		}
		spin_unlock_irqrestore(&vcmd_mgr->job_lock, flags);
		return 0;
	}

	if (*pobj == NULL) {
		//any po's cmdbuf ready, return head of job_done_list
		*pobj = (struct cmdbuf_obj *)node->data;
		is_done = 1;
	} else {
		//specified cmdbuf ready?
		obj = *pobj;
		if (obj->module_type == VCMD_TYPE_ENCODER) {
			run_done = obj->cmdbuf_run_done || _is_abnormal_run_done(obj);
		}
		if (obj->module_type == VCMD_TYPE_DECODER) {
			run_done = obj->cmdbuf_run_done || obj->slice_run_done;
		}
		//if (obj->cmdbuf_run_done || _is_abnormal_run_done(obj)) {
		//if (obj->cmdbuf_run_done || obj->slice_run_done) {
		if (run_done) {
			// check if the obj is in job done list
			while (node && node->data != (void *)obj)
				node = node->next;

			if (node == NULL) {
				vcmd_klog(LOGLVL_BRIEF, "%s: cmdbuf[%d] is done, but not in done-list!\n",
						__func__, obj->cmdbuf_id);
			} else {
				vcmd_klog(LOGLVL_BRIEF, "%s: cmdbuf[%d] is done!\n", __func__, obj->cmdbuf_id);
				is_done = 1;
			}
		}
	}

	if (is_done) {
		bi_list_remove_node(list, &vcmd_mgr->po_jobs[(*pobj)->cmdbuf_id]);
		vcmd_mgr->po_jobs[(*pobj)->cmdbuf_id].data = NULL;
	} else {
		vcmd_mgr->in_wait = 1;
	}
	spin_unlock_irqrestore(&vcmd_mgr->job_lock, flags);

	return is_done;
}


static int vcmd_get_done_job(vcmd_mgr_t *vcmd_mgr, struct cmdbuf_obj **pobj)
{

	struct bi_list *list = &vcmd_mgr->job_done_list;
	bi_list_node *node;
	uint32_t   flags;
	struct cmdbuf_obj *obj = NULL;
	int is_done = 0, i = 0, run_done = 0;

	flags = spin_lock_irqsave(&vcmd_mgr->job_lock);
	node = list->head;

	if (node == NULL) {
		// job done list is empty
		vcmd_mgr->in_wait = 1;
		spin_unlock_irqrestore(&vcmd_mgr->job_lock, flags);
		return 0;
	}

	if (*pobj == NULL) {
		//any po's cmdbuf ready, return head of job_done_list
		*pobj = (struct cmdbuf_obj *)node->data;
		is_done = 1;
	} else {
		//specified cmdbuf ready?
		obj = *pobj;
		if (obj->module_type == VCMD_TYPE_ENCODER) {
			run_done = obj->cmdbuf_run_done || _is_abnormal_run_done(obj);
		}
		if (obj->module_type == VCMD_TYPE_DECODER) {
			run_done = obj->cmdbuf_run_done || obj->slice_run_done;
		}

		if (run_done) {
			// check if the obj is in job done list
			while (node && node->data != (void *)obj)
				node = node->next;

			if (node == NULL) {
				vcmd_klog(LOGLVL_BRIEF, "%s: cmdbuf[%d] is done, but not in done-list!\n",
						__func__, obj->cmdbuf_id);
			} else {
				vcmd_klog(LOGLVL_BRIEF, "%s: cmdbuf[%d] is done!\n", __func__, obj->cmdbuf_id);
				is_done = 1;
			}
		}
	}

	if (is_done) {
		bi_list_remove_node(list, &vcmd_mgr->po_jobs[(*pobj)->cmdbuf_id]);
		vcmd_mgr->po_jobs[(*pobj)->cmdbuf_id].data = NULL;
	} else {
		vcmd_mgr->in_wait = 1;
	}
	spin_unlock_irqrestore(&vcmd_mgr->job_lock, flags);

	return is_done;
}

/**
 * @brief remove a job (cmdbuf node) from device work_list,
 * and de-link from cmdbuf jmp list
 * @return int: 1: succeed; 0: failed, the node's JMP cmd is still used by hw.
 */
int dev_delink_job(struct hantrovcmd_dev *dev,
							bi_list_node *node)
{
	struct cmdbuf_obj *obj = (struct cmdbuf_obj *)node->data;

	if (obj->cmdbuf_linked == 0) {
		//already de-linked or not link into work list yet
		return 1;
	}

	if (dev->state != VCMD_STATE_WORKING ||
		obj->has_jmp_cmd == 0 || node->next) {
		//delink cmdbuf, and remove node from work list.
		dev_link_cmdbuf(dev, node->prev, node->next);
		bi_list_remove_node(&dev->work_list, node);
		obj->cmdbuf_linked = 0;
		return 1;
	}
	// Not remove the last node, its JMP cmd is needed to link new cmd.
	return 0;
}

/**
 * @brief add a job to tail of device work_list,
 *  and link to cmdbuf jmp list if needed.
 */
static int dev_add_job(struct hantrovcmd_dev *dev, bi_list_node *job_node)
{

	struct cmdbuf_obj *obj;

	obj = (struct cmdbuf_obj *)job_node->data;
	_set_rreg_addr(dev, obj);

	bi_list_insert_node_tail(&dev->work_list, job_node);
	_update_jmp_ie(dev, obj);

#ifdef SUPPORT_DBGFS
	_dbgfs_record_active_start_time(dev->dbgfs_info);
#endif

	dev_link_cmdbuf(dev, job_node->prev, job_node);
	obj->core_id = dev->core_id;
	obj->cmdbuf_linked = 1;

	return 0;
}

/**
 * @brief insert job_node prior to base_node of dev work_list,
 *  and insert to cmdbuf jmp list accordingly if needed.
 */
static int dev_insert_job(struct hantrovcmd_dev *dev,
							bi_list_node *base_node,
							bi_list_node *job_node)
{
	struct cmdbuf_obj *obj;

	obj = (struct cmdbuf_obj *)job_node->data;
	_set_rreg_addr(dev, obj);

	bi_list_insert_node_before(&dev->work_list, base_node, job_node);

#ifdef SUPPORT_DBGFS
	_dbgfs_record_active_start_time(dev->dbgfs_info);
#endif

	dev_link_cmdbuf(dev, job_node->prev, job_node);
	dev_link_cmdbuf(dev, job_node, job_node->next);
	obj->core_id = dev->core_id;
	obj->cmdbuf_linked = 1;

	return 0;
}

/**
 * @brief remove a job from dev work_list,
 *  de-link it from cmdbuf jmp list if needed, and return the cmdbuf.
 */
int dev_remove_job(vcmd_mgr_t *vcmd_mgr, struct hantrovcmd_dev *dev,
						bi_list_node *node)
{
	struct cmdbuf_obj *obj;

	obj = (struct cmdbuf_obj *)node->data;

	vcmd_klog(LOGLVL_FLOW, "Delink and remove cmdbuf [%d] from dev [%d].\n",
			obj->cmdbuf_id, dev->core_id);
	if (node->prev) {
		vcmd_klog(LOGLVL_BRIEF, "prev cmdbuf [%d].\n",
			   ((struct cmdbuf_obj *)node->prev->data)->cmdbuf_id);
	} else {
		vcmd_klog(LOGLVL_BRIEF, "NO prev cmdbuf.\n");
	}
	if (node->next) {
		vcmd_klog(LOGLVL_BRIEF, "next cmdbuf [%d].\n",
			   ((struct cmdbuf_obj *)node->next->data)->cmdbuf_id);
	} else {
		vcmd_klog(LOGLVL_BRIEF, "NO next cmdbuf.\n");
	}

	if (dev_delink_job(dev, node))
		obj->session = NULL;

	return 0;
}

/**
 * @brief get remain (un-do) jobs count from dev work_list,
 */
static u32 dev_get_job_num(struct hantrovcmd_dev *dev)
{
	struct cmdbuf_obj *obj;
	bi_list_node *node = dev->work_list.head;
	u32 num = 0;

	while (node) {
		obj = (struct cmdbuf_obj *)node->data;
		if (obj->cmdbuf_run_done == 0)
			num++;
		node = node->next;
	}
	return num;
}

/**
 * @brief calculate workload after specified node in dev work_list.
 */
static u32 calc_workload_after_node(bi_list_node *node)
{
	u32 sum = 0;
	struct cmdbuf_obj *obj;

	while (node) {
		obj = (struct cmdbuf_obj *)node->data;
		sum += obj->workload;
		node = node->next;
	}
	return sum;
}

/**
 * @brief check if specified device is in core_mask
 * @return int: 1: device is in core_mask; 0: not in core_mask.
 */
static int is_supported_core(u32 core_mask, u16 dev_id)
{
	if (core_mask && ((core_mask >> dev_id) & 0x1))
		return 1; //found one supported core

	return 0; //not found the supported core
}

struct sub_ip_init_cfg {
	u32 reg_id;
	u32 reg_val;
};

#ifdef SUPPORT_AXIFE
struct sub_ip_init_cfg axife_init_cfg[] = {
	{AXI_REG10_SW_FRONTEND_EN, 0x02},
	{AXI_REG11_SW_WORK_MODE, 0x00},
	{0xffff, }	//end guard
};
#endif

#ifdef AXI2TO1_SUPPORT
struct sub_ip_init_cfg axi2to1_init_cfg[] = {
	{AXI2TO1_REG6_SW_IRQ_EN, 0xffffffff}, // axi2to1 irq enable
	{AXI2TO1_REG7_SW_TIMEOUT_CYCLES, 0x40000000} // axife flush timeout cycles
};
#endif

#ifdef SUPPORT_MMU
struct sub_ip_init_cfg mmu_init_cfg[] = {
	{MMU_REG_ADDRESS, 0x0000},		//not move, sw will update mmu addr LSB to index0
	{MMU_REG_ADDRESS_MSB, 0x0000},	//not move, sw will update mmu addr MSB to index1
#ifdef SUPPORT_48PA_MMU
	{MMU_REG_ARRAY_SIZE, 0x0001},
#endif
	{MMU_REG_PAGE_TABLE_ID, 0x10000},
	{MMU_REG_PAGE_TABLE_ID, 0x00000},
	{MMU_REG_CONTROL, 0x0001},
	{0xffff, }	//end guard
};
#endif

#ifdef HANTROVCMD_ENABLE_IP_SUPPORT
/**
 * @brief set reg id/val of specified sub-ip to init-cmd regs.
 */
static void _set_module_init_cmds(struct hantrovcmd_dev *dev, u32 module_id,
									struct sub_ip_init_cfg *module_cfg)
{
	u16 reg_off = dev->subsys_info->reg_off[module_id];
	u32 opcode = OPCODE_WREG | WREG_MODE(WREG_ADDR_FIX) | WREG_LEN(1);

	if (reg_off == 0xffff)
		return;

	while (module_cfg->reg_id != 0xffff) {
		dev->reg_mirror[dev->init_cmd_idx++] = opcode |
												(reg_off + module_cfg->reg_id);
		dev->reg_mirror[dev->init_cmd_idx++] = module_cfg->reg_val;
		module_cfg++;
	}
}
#endif

/**
 * @brief set init-cmd regs to init sub-ips if necessary.
 */
static void vcmd_set_init_cmds(struct hantrovcmd_dev *dev)
{
#ifdef HANTROVCMD_ENABLE_IP_SUPPORT
	u32 i;

	dev->init_cmd_idx = VCMD_REG_ID_SW_INIT_CMD0;

#ifdef SUPPORT_AXIFE
	//enable AXIFE by VCMD
	_set_module_init_cmds(dev, SUB_MOD_AXIFE, axife_init_cfg);//_set_module_init_cmds(dev, SUB_MOD_AXIFE0, axife_init_cfg);
	if (dev->subsys_info->sub_module_type == VCMD_TYPE_ENCODER) {
	    _set_module_init_cmds(dev, SUB_MOD_AXIFE1, axife_init_cfg);
	}
#endif

#ifdef AXI2TO1_SUPPORT
	_set_module_init_cmds(dev, SUB_MOD_AXI2TO1, axi2to1_init_cfg);
#endif

#ifdef SUPPORT_MMU
	//enable MMU by VCMD
	if (dev->mmu_enable) {
		u64 mmu_addr = GetMMUAddress();

		vcmd_klog(LOGLVL_FLOW, "%s: mmu address = 0x%llx", __func__, mmu_addr);
		mmu_init_cfg[0].reg_val = (u32)mmu_addr;
		mmu_init_cfg[1].reg_val = (u32)(mmu_addr >> 32);
#if defined(SUPPORT_48PA_MMU) && defined(MMU_PAGE_TABLE_SWITCH)
		mmu_init_cfg[2].reg_val = GetMMUPageTableArraySize();
#endif

		_set_module_init_cmds(dev, SUB_MOD_MMU0, mmu_init_cfg);//	_set_module_init_cmds(dev, SUB_MOD_MMU, mmu_init_cfg);
		_set_module_init_cmds(dev, SUB_MOD_MMU1, mmu_init_cfg);//	_set_module_init_cmds(dev, SUB_MOD_MMU_WR, mmu_init_cfg);
	}
#endif

	//finished with END command
	dev->reg_mirror[dev->init_cmd_idx++] = OPCODE_END;
	dev->reg_mirror[dev->init_cmd_idx++] = 0x00;

	for (i = VCMD_REG_ID_SW_INIT_CMD0; i < dev->init_cmd_idx; i++)
		vcmd_write_reg((const void *)dev->hwregs, i*4, dev->reg_mirror[i]);
#endif
}

/**
 * @brief start a vcmd hw device to run the 1st not-done job in its work list.
 */
void vcmd_start(struct hantrovcmd_dev *dev, int irq)
{
	struct cmdbuf_obj *obj = NULL;
	const void *hwregs = (const void *)dev->hwregs;
	bi_list_node *node;
	u32 *reg_mirror = dev->reg_mirror;
	ptr_t cmd_ba;
	u32 ba_msb = 0;

	if (dev->state == VCMD_STATE_WORKING) {
		vcmd_klog(LOGLVL_BRIEF, "%s: vcmd is already in working state!\n", __func__);
		return;
	}

	dev->sw_cmdbuf_rdy_num = dev_get_job_num(dev);
	node = dev->work_list.head;
	while (node && ((struct cmdbuf_obj *)node->data)->cmdbuf_run_done)
		node = node->next;

	if (dev->sw_cmdbuf_rdy_num == 0 || node == NULL) {
		vcmd_klog(LOGLVL_BRIEF, "%s: no cmdbuf to start yet!\n", __func__);
		return;
	}

	obj = (struct cmdbuf_obj *)node->data;

#ifdef VCMD_DEBUG_INTERNAL
	printk_vcmd_register_debug(hwregs, "vcmd start enters");
#endif

	vcmd_klog(LOGLVL_FLOW, "vcmd start for cmdbuf id %d, cmdbuf_run_done = %d\n",
							obj->cmdbuf_id, obj->cmdbuf_run_done);

	//init HWIF_VCMD_EXE_CMDBUF_COUNT
	vcmd_write_register_value(hwregs, reg_mirror, HWIF_VCMD_EXE_CMDBUF_COUNT, 0);

	vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_RDY_CMDBUF_COUNT,
							dev->sw_cmdbuf_rdy_num);

	if (dev->state == VCMD_STATE_POWER_ON) {
		//0x40
	#ifdef HANTROVCMD_ENABLE_IP_SUPPORT
		//when start vcmd, first vcmd is init mode
		vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_INIT_ENABLE, dev->init_mode);
	#endif
		vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_AXI_CLK_GATE_DISABLE, 0);
		vcmd_set_reg_mirror(reg_mirror,
						HWIF_VCMD_MASTER_OUT_CLK_GATE_DISABLE, APB_CLK_MODE);
		vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_CORE_CLK_GATE_DISABLE, 0);
		vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_ABORT_MODE, dev->abort_mode);
		vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_RESET_CORE, 0);
		vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_RESET_ALL, 0);
		vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_START_TRIGGER, 0);
		//0x48
		if (dev->hw_feature.vcarb_ver) {
			if (dev->hw_feature.vcarb_ver == VCARB_VERSION_2_0)
				vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_IRQ_ARBRST_EN, 1);
			vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_IRQ_ARBERR_EN, 0);
		}

		vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_IRQ_JMP_EN, 1);

		vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_IRQ_ABORT_EN, 1);
		vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_IRQ_CMDERR_EN, 1);
		vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_IRQ_TIMEOUT_EN, 1);
		if (dev->hw_version_id >= HW_ID_1_6_0) {
			vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_IRQ_BUSERR_EN, 1);
		} else {
			/* not report bus err interrup before v1.6.0*/
			vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_IRQ_BUSERR_EN, 0);
		}
		vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_IRQ_ENDCMD_EN, 1);
		//0x4c
		vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_TIMEOUT_ENABLE, 1);
		vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_TIMEOUT_CYCLES, 500000000);

		vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_MAX_BURST_LEN, 0x10);

		//0x64
		reg_mirror[VCMD_REGISTER_EXT_INT_GATE_OFFSET / 4] = dev->intr_gate_mask;

		if (dev->hw_feature.vcarb_ver == VCARB_VERSION_2_0) {
            vcodec_config_t *config = vcodec_get_config();
			//0x70
			reg_mirror[VCMD_REGISTER_ARBITER_CONFIG_OFFSET / 4] =
					VCMD_ARBITER_PARAMS(config->arbiter_weight, config->arbiter_urgent, config->arbiter_bw_overflow,
										config->arbiter_timewindow);

		} else if (dev->hw_feature.has_cmdbuf_timeout) {
			// 0x70
			/* set cmdbuf_timeout_enable */
			u32 regVal = 0xC0000000;
			if (dev->subsys_info->sub_module_type == VCMD_TYPE_ENCODER) {
				regVal = 0x80000000;
			}
			reg_mirror[VCMD_REGISTER_CMDBUF_TIMEOUT_OFFSET / 4] |= regVal;
			/* need set one frame time, the value is multiple 256 cycles:
			 *  default: 0x40000000
			 *  according one frame time to set: frequency * ms / 1000 / 256
			 */
			if (dev->subsys_info->sub_module_type == VCMD_TYPE_ENCODER) {
				reg_mirror[VCMD_REGISTER_CMDBUF_TIMEOUT_OFFSET / 4] |=
						PLATFORM_FREQUENCY * ONE_JOB_WAIT_TIME / 1000 / 256;
			}
			// 0x48
			reg_mirror[VCMD_REGISTER_INT_CTL_OFFSET / 4] |= (0x1 << 9);
		}
	}

	cmd_ba = obj->cmd_pa - dev->pa_trans_offset;
	if (dev->mmu_enable)
		cmd_ba = (ptr_t)obj->mmu_cmd_ba;
	if (sizeof(ptr_t) == 8)
		ba_msb = (u32)(cmd_ba >> 32);


	vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_CMDBUF_EXE_ADDR, (u32)cmd_ba);
	vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_CMDBUF_EXE_ADDR_MSB, ba_msb);
	vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_CMDBUF_EXE_LENGTH,
							(obj->cmdbuf_size + 7) / 8);

	if (dev->hw_version_id > HW_ID_1_0_C)
		vcmd_write_register_value(hwregs, reg_mirror,
									HWIF_VCMD_CMDBUF_EXE_ID,
									(u32)obj->cmdbuf_id);

	if (dev->state == VCMD_STATE_POWER_ON) {
		//0x44
		vcmd_write_reg(hwregs, VCMD_REGISTER_INT_STATUS_OFFSET,
			vcmd_read_reg(hwregs, VCMD_REGISTER_INT_STATUS_OFFSET));
		//0x40
		vcmd_write_reg(hwregs, VCMD_REGISTER_CONTROL_OFFSET,
			reg_mirror[VCMD_REGISTER_CONTROL_OFFSET / 4]);
		//0x48
		vcmd_write_reg(hwregs, VCMD_REGISTER_INT_CTL_OFFSET,
			reg_mirror[VCMD_REGISTER_INT_CTL_OFFSET / 4]);
		//0x4c
		vcmd_write_reg(hwregs, VCMD_REGISTER_TIMEOUT_OFFSET,
			reg_mirror[VCMD_REGISTER_TIMEOUT_OFFSET / 4]);
		//0x5c
		vcmd_write_reg(hwregs, VCMD_REGISTER_SWAP_OFFSET,
			reg_mirror[VCMD_REGISTER_SWAP_OFFSET / 4]);
		//0x64
		vcmd_write_reg(hwregs, VCMD_REGISTER_EXT_INT_GATE_OFFSET,
			reg_mirror[VCMD_REGISTER_EXT_INT_GATE_OFFSET / 4]);
		//0x70
		if (dev->hw_feature.vcarb_ver == VCARB_VERSION_2_0)
			vcmd_write_reg(hwregs, VCMD_REGISTER_ARBITER_CONFIG_OFFSET,
				reg_mirror[VCMD_REGISTER_ARBITER_CONFIG_OFFSET / 4]);
		else if (dev->hw_feature.has_cmdbuf_timeout)
			vcmd_write_reg(hwregs, VCMD_REGISTER_CMDBUF_TIMEOUT_OFFSET,
				reg_mirror[VCMD_REGISTER_CMDBUF_TIMEOUT_OFFSET / 4]);
		if (dev->hw_version_id >= HW_ID_1_2_1)
			vcmd_set_init_cmds(dev);
	}
	//0x50
	vcmd_write_reg(hwregs, VCMD_REGISTER_CMDBUF_EXE_ADDR_LSB_OFFSET,
		reg_mirror[VCMD_REGISTER_CMDBUF_EXE_ADDR_LSB_OFFSET / 4]);
	//0x54
	vcmd_write_reg(hwregs, VCMD_REGISTER_CMDBUF_EXE_ADDR_MSB_OFFSET,
		reg_mirror[VCMD_REGISTER_CMDBUF_EXE_ADDR_MSB_OFFSET / 4]);
	//0x58
	vcmd_write_reg(hwregs, VCMD_REGISTER_CMDBUF_EXE_LEN_OFFSET,
		reg_mirror[VCMD_REGISTER_CMDBUF_EXE_LEN_OFFSET / 4]);
	//0x60
	vcmd_write_reg(hwregs, VCMD_REGISTER_RDY_CMDBUF_COUNT_OFFSET,
		reg_mirror[VCMD_REGISTER_RDY_CMDBUF_COUNT_OFFSET / 4]);

	dev->state = VCMD_STATE_WORKING;

	//start
	vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_START_TRIGGER, 1);
	vcmd_write_reg(hwregs, VCMD_REGISTER_CONTROL_OFFSET,
		reg_mirror[VCMD_REGISTER_CONTROL_OFFSET / 4]);

#ifdef VCMD_DEBUG_INTERNAL
	printk_vcmd_register_debug(hwregs, "vcmd start exits");
#endif
}

/**
 * @brief abort a specified vcmd hw device.
 * @param u32 *aborted_id: the id of aborted cmdbuf.
 * @param u32 vcmd_isr_polling: the mode to wait for device being aborted.
 */
int vcmd_abort(vcmd_mgr_t *vcmd_mgr, struct hantrovcmd_dev *dev, u32 *aborted_id)
{
	uint32_t   flags;
	u32 cnt = 100000, irq;

	flags = spin_lock_irqsave(dev->spinlock);
	vcmd_write_register_value((const void *)dev->hwregs,
								dev->reg_mirror,
								HWIF_VCMD_START_TRIGGER, 0);
	spin_unlock_irqrestore(dev->spinlock, flags);
	if (vcodec_get_config()->vcmd_isr_polling == 0) {
//	    BaseType_t retCode = wait_event_interruptible(*dev->abort_waitq, (dev->state == VCMD_STATE_IDLE));
//		if (retCode == pdFALSE) {
//			vcmd_klog(LOGLVL_ERROR, "%s: abort_waitq is signaled, continue to wait vcmd aborted!!!\n", __func__);
//			goto isr_polling;
//		} else {
//			goto out;
//		}
	}

isr_polling:
	irq = (dev->subsys_info->irq == -1) ?
			dev->core_id : dev->subsys_info->irq;

	while (cnt--) {
		//(100, 120);
//        vTaskDelay(pdMS_TO_TICKS(5)); 
		hantrovcmd_isr(irq, vcmd_mgr);
		if (dev->state == VCMD_STATE_IDLE) {
			cnt += 1;
			break;
		}
	}
	if (cnt == 0) {
		vcmd_klog(LOGLVL_ERROR, "%s: can't wait aborted!!!\n", __func__);
		return -ERESTARTSYS;
	}

out:
	if (aborted_id)
		*aborted_id = dev->aborted_cmdbuf_id;

	vcmd_klog(LOGLVL_BRIEF, "%s: vcmd aborted cmdbuf[%u].\n", __func__, dev->aborted_cmdbuf_id);

	return 0;

}

/**
 * @brief select a suitable device, and add/insert a job node to its work_list.
 */
static int select_vcmd(vcmd_mgr_t *vcmd_mgr, bi_list_node *new_node)
{
	struct hantrovcmd_dev *dev, *smallest_dev;
	struct vcmd_module_mgr *module;
	struct cmdbuf_obj *obj, *tmp_obj;
	bi_list_node *curr_node;
	bi_list *list;
	u32 least_workload;
	u32 reg_id_exe;

	u32 cmdbuf_id, i, devID = 0;
	uint32_t   flags;
	ptr_t curr_exe_addr;
	int ret;

	obj = (struct cmdbuf_obj *)new_node->data;
	module = &vcmd_mgr->module_mgr[obj->module_type];

	/* To check if there is free dev, or dev which tail node is run-done. */
	for (i = 0; i < module->num; i++) {
		dev = module->dev[i];
		devID = dev->id_in_type;//		if (obj->module_type == VCMD_TYPE_ENCODER)
		if (obj->module_type == VCMD_TYPE_DECODER)
			devID = dev->core_id;
		ret = is_supported_core(obj->core_mask, devID);//ret = is_supported_core(obj->core_mask, dev->id_in_type);ret = is_supported_core(obj->core_mask, dev->core_id);
		if (ret == 0)
			continue;

		list = &dev->work_list;
		flags = spin_lock_irqsave(dev->spinlock);
		if (!list->tail ||
			((struct cmdbuf_obj *)list->tail->data)->cmdbuf_run_done) {
			dev_add_job(dev, new_node);
			spin_unlock_irqrestore(dev->spinlock, flags);
			return 0;
		}
		spin_unlock_irqrestore(dev->spinlock, flags);
	}

	// There is no vcmd in free, calculate each workload, select the least one.
	// If low priority, insert to tail.
	// If high priority, abort the dev, and insert to "head".
	reg_id_exe = REG_ID_CMDBUF_EXE_ID;
	if (obj->priority == CMDBUF_PRIORITY_NORMAL)
		reg_id_exe = REG_ID2_CMDBUF_EXE_ID;

	least_workload = 0xffffffff;
	smallest_dev = NULL;

	//calculate remain workload of all dev, find the least one
	for (i = 0; i < module->num; i++) {
		dev = module->dev[i];
		devID = dev->id_in_type;//		if (obj->module_type == VCMD_TYPE_ENCODER)
		if (obj->module_type == VCMD_TYPE_DECODER)
			devID = dev->core_id;
		ret = is_supported_core(obj->core_mask, devID);//ret = is_supported_core(obj->core_mask, dev->id_in_type);ret = is_supported_core(obj->core_mask, dev->core_id);
		if (ret == 0)
			continue;

		list = &dev->work_list;

		//get the executing cmdbuf node.
		if (dev->hw_version_id <= HW_ID_1_0_C) {
			curr_exe_addr = VCMDGetAddrRegisterValue((const void *)dev->hwregs,
												dev->reg_mirror,
												HWIF_VCMD_CMDBUF_EXE_ADDR);

			curr_node = get_cmdbuf_node_by_addr(vcmd_mgr, dev, curr_exe_addr);

		} else {
			//cmdbuf_id = vcmd_get_register_value((const void *)dev->hwregs,
			//dev->reg_mirror,HWIF_VCMD_CMDBUF_EXE_ID);
			cmdbuf_id = *(dev->reg_mem_va + reg_id_exe);
			if (cmdbuf_id >= SLOT_NUM_CMDBUF) {
				vcmd_klog(LOGLVL_ERROR, "cmdbuf_id greater than the ceiling !!\n");
				return -1;
			}

			curr_node = &vcmd_mgr->nodes[cmdbuf_id];
		}

		flags = spin_lock_irqsave(dev->spinlock);
		if (!curr_node)
			curr_node = list->head;
		//calculate total workload of this device
		dev->total_workload = calc_workload_after_node(curr_node);
		spin_unlock_irqrestore(dev->spinlock, flags);

		if (dev->total_workload <= least_workload) {
			least_workload = dev->total_workload;
			smallest_dev = dev;
		}
	}

	if (smallest_dev == NULL) {
		vcmd_klog(LOGLVL_ERROR, "%s: no dev is available to cmdbuf [%d] with core_mask 0x%x\n",
					__func__, obj->cmdbuf_id, obj->core_mask);
		return -EINVAL;
	}

	list = &smallest_dev->work_list;
	if (obj->priority == CMDBUF_PRIORITY_NORMAL) {
		//insert to tail
		flags = spin_lock_irqsave(smallest_dev->spinlock);
		dev_add_job(smallest_dev, new_node);
		spin_unlock_irqrestore(smallest_dev->spinlock, flags);
		return 0;
	}

	//CMDBUF_PRIORITY_HIGH
	//abort the vcmd and wait
	if (vcmd_abort(vcmd_mgr, smallest_dev, &cmdbuf_id)) {
		//abort failed
		return -ERESTARTSYS;
	}

	// need to select inserting position again
	// because hw maybe have run to the next node.
	// CMDBUF_PRIORITY_HIGH
	flags = spin_lock_irqsave(smallest_dev->spinlock);
	curr_node = &vcmd_mgr->nodes[cmdbuf_id];
	if (smallest_dev->abort_mode == 0)
		curr_node = curr_node->next;
	while (curr_node) {
		tmp_obj = (struct cmdbuf_obj *)curr_node->data;
		//find the 1st node with normal priority, and insert node prior to it
		if (tmp_obj->priority == CMDBUF_PRIORITY_NORMAL)
			break;
		curr_node = curr_node->next;
	}

	//insert to "head" of normal priority nodes
	dev_insert_job(smallest_dev, curr_node, new_node);
	spin_unlock_irqrestore(smallest_dev->spinlock, flags);
	return 0;
}



/**
 * @brief reserve a cmdbuf for specified process object.
 * @param struct exchange_parameter *param: the param of cmdbuf to reserve.
 * @return long: 0: succeed; oters: failed.
 */
long reserve_cmdbuf(vcmd_mgr_t *vcmd_mgr, cmdr52_session_t *session, struct exchange_parameter *param)
{
	struct cmdbuf_obj *obj;
	u32 cmdbuf_id = 0;
	u32 workload = param->interrupt_ctrl, maxworkload = 32 * VCMD_WORKLOAD_UNIT;

	if (param->cmdbuf_size > SLOT_SIZE_CMDBUF) {
		vcmd_klog(LOGLVL_ERROR, "%s size is larger than slot size !!\n", __func__);
		return -1;
	}

	if (!session) {
		vcmd_klog(LOGLVL_ERROR, "%s: not find process obj!\n", __func__);
		return -1;
	}

    if (param->module_type == VCMD_TYPE_ENCODER) {
	    workload = (param->interrupt_ctrl) & 0x7fffffff; //bit31 for interrupt
	    maxworkload = 32 * VCMD_WORKLOAD_UNIT;
	}

    if (param->module_type == VCMD_TYPE_DECODER) {
	    workload = param->interrupt_ctrl;
	    maxworkload = 64 * VCMD_WORKLOAD_UNIT;
	}

	reset_cmdbuf_obj(vcmd_mgr, cmdbuf_id);
	reset_cmdbuf_node(vcmd_mgr, cmdbuf_id);

	obj = &vcmd_mgr->objs[cmdbuf_id];
	obj->module_type = param->module_type;
	obj->priority = EXCH_G_BIT(param->input_mask, EXCH_PRIO_BIT);
	obj->workload = workload;
	obj->interrupt_ctrl = param->interrupt_ctrl;
	obj->session = session;
	obj->core_mask = param->core_mask;

	param->cmdbuf_size = SLOT_SIZE_CMDBUF;
	param->cmdbuf_id = cmdbuf_id;

	vcmd_klog(LOGLVL_FLOW, "%s, reserved cmdbuf[%d]: obj %p, node %p\n",
			__func__,  cmdbuf_id, (void *)obj,
			(void *)&vcmd_mgr->nodes[cmdbuf_id]);
#ifdef SUPPORT_DBGFS
	_dbgfs_record_reserved_time(vcmd_mgr->dev_ctx[0].dbgfs_info,
								param->cmdbuf_id);
#endif

	return 0;
}

int32_t vcmd_release_cmdbuf(vcmd_mgr_t *vcmd_mgr, u16 cmdbuf_id)
{
	struct cmdbuf_obj *obj = NULL;
	bi_list_node *curr_node = NULL;
	uint32_t   flags;
	struct hantrovcmd_dev *dev = NULL;

	if (cmdbuf_id >= SLOT_NUM_CMDBUF) {
		//should not happen
		vcmd_klog(LOGLVL_ERROR, "%s: ERROR cmdbuf_id %d!!\n", __func__, cmdbuf_id);
		return -1;
	}

	curr_node = &vcmd_mgr->nodes[cmdbuf_id];
	obj = (struct cmdbuf_obj *)curr_node->data;

	if (obj->cmdbuf_linked == 0) {
		//not link_and_run yet
		obj = (struct cmdbuf_obj *)curr_node->data;
		return_process_resource(obj->session, obj);
		obj->session = NULL;
	} else {
		obj->cmdbuf_need_remove = 1;
		dev = &vcmd_mgr->dev_ctx[obj->core_id];

		return_process_resource(obj->session, obj);
		flags = spin_lock_irqsave(dev->spinlock);
		dev_remove_job(vcmd_mgr, dev, curr_node);
		spin_unlock_irqrestore(dev->spinlock, flags);
	}
	return 0;
}

/**
 * @brief add/insert a cmdbuf (job) to suitable device,
 *  and start the device hw if it is not working.
 * @param struct exchange_parameter *param: the param of cmdbuf to link & run.
 * @return long: 0: succeed; oters: failed.
 */
long link_and_run_cmdbuf(vcmd_mgr_t *vcmd_mgr, cmdr52_session_t *session, struct exchange_parameter *param)
{
	struct cmdbuf_obj *obj;
	bi_list_node *curr_node;

	struct hantrovcmd_dev *dev = NULL;
	uint32_t   flags;
	int ret;
	u16 cmdbuf_id = param->cmdbuf_id;
	u16 batchcount = ((param->interrupt_ctrl >> 32) & 0xff);

	struct cmd_jmp_t *cmd_jmp;

	if (cmdbuf_id >= SLOT_NUM_CMDBUF) {
		//should not happen
		vcmd_klog(LOGLVL_ERROR, "%s: ERROR cmdbuf_id %d!!\n", __func__, cmdbuf_id);
		return -1;
	}

	curr_node = &vcmd_mgr->nodes[cmdbuf_id];
	obj = (struct cmdbuf_obj *)curr_node->data;
	if (obj->session != session) {
		//should not happen
		vcmd_klog(LOGLVL_ERROR, "%s: cmdbuf[%d] session not match: owned by %p, released by %p!!\n",
						__func__, cmdbuf_id, obj->session, session);
		return -1;
	}

	obj->cmdbuf_size = param->cmdbuf_size;
	obj->interrupt_ctrl = param->interrupt_ctrl;

#ifdef VCMD_DEBUG_INTERNAL
	_dbg_log_cmdbuf(obj);
#endif

	//0: has jmp opcode,1 has end code
	obj->has_jmp_cmd = (EXCH_G_BIT(param->input_mask, EXCH_END_CMD_BIT)) ? 0 : 1;

	if (obj->has_jmp_cmd) {
		//last command is JMP, get its IE value.
		cmd_jmp = _get_jmp_cmd(obj);
		vcmd_klog(LOGLVL_FLOW, "has jmp cmd and the last cmd is JMP!\n");

		if ((cmd_jmp->opcode & OPCODE_MASK) != OPCODE_JMP) {
			vcmd_klog(LOGLVL_ERROR, "%s: cmdbuf[%d] is not terminated by JMP, not match with its flag!",
					  __func__, obj->cmdbuf_id);
			return -1;
		}
		obj->jmp_ie = 0;
		if (obj->interrupt_ctrl >> 31)
			obj->jmp_ie = obj->interrupt_ctrl & 1; //force jmp_ie to be 1 or 0
		else if (JMP_G_IE(cmd_jmp->opcode))
			obj->jmp_ie = 1;
	}

	ret = select_vcmd(vcmd_mgr, curr_node);
	if (ret) {
		return ret;
	}

	dev = &vcmd_mgr->dev_ctx[obj->core_id];
	param->core_id = obj->core_id;
	vcmd_klog(LOGLVL_FLOW, "Assign cmdbuf[%d] to core[%d]\n",
			  cmdbuf_id, param->core_id);

	//start to run
	flags = spin_lock_irqsave(dev->spinlock);
	if (dev->state != VCMD_STATE_WORKING) {
		//start vcmd
		vcmd_start(dev, 0);
	} else {
		dev->sw_cmdbuf_rdy_num++;
		//if ((batchcount > 0 && obj->jmp_ie == 1 && vcmd_mgr->module_mgr[VCMD_TYPE_ENCODER].num == 1) ||
		//	batchcount == 0 || vcmd_mgr->module_mgr[VCMD_TYPE_ENCODER].num > 1) {
		if ((batchcount > 0 && obj->jmp_ie == 1 && vcmd_mgr->module_mgr[obj->module_type].num == 1) ||
			batchcount == 0 || vcmd_mgr->module_mgr[obj->module_type].num > 1) {
#ifdef SUPPORT_DBGFS
			_dbgfs_record_link_time(dev->dbgfs_info, cmdbuf_id,
									dev->sw_cmdbuf_rdy_num,
									obj->workload);
#endif
			//just update cmdbuf ready number
			vcmd_write_register_value((const void *)dev->hwregs,
										dev->reg_mirror,
										HWIF_VCMD_RDY_CMDBUF_COUNT,
										dev->sw_cmdbuf_rdy_num);
		}
	}

	//new cmdbuf linked, and free the removeable cmdbuf.
	if (curr_node->prev) {
		obj = (struct cmdbuf_obj *)curr_node->prev->data;
		if (obj->cmdbuf_need_remove) {
			//free the job
			dev_remove_job(vcmd_mgr, dev, curr_node->prev);
		}
	}

	spin_unlock_irqrestore(dev->spinlock, flags);
	return 0;
}

int32_t vcmd_link_and_rum_cmdbuf(vcmd_mgr_t *vcmd_mgr, cmdr52_session_t *session, cmdReqRunCmdBuf_Body_t *cmd_body) {
	struct cmdbuf_obj *obj;
	bi_list_node *curr_node;

	struct hantrovcmd_dev *dev = NULL;
	uint32_t   flags;
	int ret;
	uint16_t cmdbuf_id = cmd_body->cmdbuf_id;
	uint16_t batchcount = ((cmd_body->interrupt_ctrl >> 32) & 0xff);

    uint16_t       core_mask;
	struct cmd_jmp_t *cmd_jmp;

	if (cmdbuf_id >= SLOT_NUM_CMDBUF) {		//should not happen
		vcmd_klog(LOGLVL_ERROR, "%s: ERROR cmdbuf_id %d!!\n", __func__, cmdbuf_id);
		return -1;
	}

	curr_node = &vcmd_mgr->nodes[cmdbuf_id];
	obj = (struct cmdbuf_obj *)curr_node->data;

	obj->owner          = cmd_body->ownerID;
	obj->session        = session;
	obj->cmdbuf_size    = cmd_body->cmdbuf_size;
	obj->interrupt_ctrl = cmd_body->interrupt_ctrl;
	obj->module_type    = cmd_body->module_type;
	obj->core_mask      = cmd_body->core_mask;
	obj->priority       = EXCH_G_BIT(cmd_body->input_mask, EXCH_PRIO_BIT);
#ifdef VCMD_ALLOC_MEM

#ifdef VCMD_DEBUG_INTERNAL
	_dbg_log_cmdbuf(obj);
#endif

	//0: has jmp opcode,1 has end code
	obj->has_jmp_cmd = (EXCH_G_BIT(cmd_body->input_mask, EXCH_END_CMD_BIT)) ? 0 : 1;

	if (obj->has_jmp_cmd) {
		//last command is JMP, get its IE value.
		cmd_jmp = _get_jmp_cmd(obj);
		vcmd_klog(LOGLVL_FLOW, "has jmp cmd and the last cmd is JMP!\n");

		if ((cmd_jmp->opcode & OPCODE_MASK) != OPCODE_JMP) {
			vcmd_klog(LOGLVL_ERROR, "%s: cmdbuf[%d] is not terminated by JMP, not match with its flag!",
					  __func__, obj->cmdbuf_id);
			return -1;
		}
		obj->jmp_ie = 0;
		if (obj->interrupt_ctrl >> 31)
			obj->jmp_ie = obj->interrupt_ctrl & 1; //force jmp_ie to be 1 or 0
		else if (JMP_G_IE(cmd_jmp->opcode))
			obj->jmp_ie = 1;
	}

	ret = select_vcmd(vcmd_mgr, curr_node);
	if (ret) {
		return ret;
	}

	dev = &vcmd_mgr->dev_ctx[obj->core_id];
	cmd_body->core_id = obj->core_id;
	vcmd_klog(LOGLVL_FLOW, "Assign cmdbuf[%d] to core[%d]\n",
			  cmdbuf_id, cmd_body->core_id);

	//start to run
	flags = spin_lock_irqsave(dev->spinlock);
	if (dev->state != VCMD_STATE_WORKING) {
		//start vcmd
		vcmd_start(dev, 0);
	} else {
		dev->sw_cmdbuf_rdy_num++;
		//if ((batchcount > 0 && obj->jmp_ie == 1 && vcmd_mgr->module_mgr[VCMD_TYPE_ENCODER].num == 1) ||
		//	batchcount == 0 || vcmd_mgr->module_mgr[VCMD_TYPE_ENCODER].num > 1) {
		if ((batchcount > 0 && obj->jmp_ie == 1 && vcmd_mgr->module_mgr[obj->module_type].num == 1) ||
			batchcount == 0 || vcmd_mgr->module_mgr[obj->module_type].num > 1) {
#ifdef SUPPORT_DBGFS
			_dbgfs_record_link_time(dev->dbgfs_info, cmdbuf_id,
									dev->sw_cmdbuf_rdy_num,
									obj->workload);
#endif
			//just update cmdbuf ready number
			vcmd_write_register_value((const void *)dev->hwregs,
										dev->reg_mirror,
										HWIF_VCMD_RDY_CMDBUF_COUNT,
										dev->sw_cmdbuf_rdy_num);
		}
	}

	//new cmdbuf linked, and free the removeable cmdbuf.
	if (curr_node->prev) {
		obj = (struct cmdbuf_obj *)curr_node->prev->data;
		if (obj->cmdbuf_need_remove) {
			//free the job
			dev_remove_job(vcmd_mgr, dev, curr_node->prev);
		}
	}

	spin_unlock_irqrestore(dev->spinlock, flags);
#else
	obj->core_id        = 0;
	cmd_body->core_id = obj->core_id;

	flags = spin_lock_irqsave(&vcmd_mgr->job_lock);
    obj->cmdbuf_run_done = 1;
	spin_unlock_irqrestore(&vcmd_mgr->job_lock, flags);
//	proc_add_done_job(vcmd_mgr, obj);
    {
		cmdr52_mgr_t *mgr = (cmdr52_mgr_t*) cmdr52_mgr_get();
		cmdEvtIntIrqVpu_Body_t *cmdBody = NULL;
		cmdMsg_t *cmdMsg = NULL;
		cmdMsg = BQueueDequeue(mgr->cmd_queue);
		cmdBody = (cmdEvtIntIrqVpu_Body_t *)cmdMsg->data;
		cmd_init(cmdMsg);
		cmdMsg->cmdType = CMD_EVT_INTIRQ_VCODEC;
		cmdMsg->cmdSize = CMD_MSG_MIN_SIZE + sizeof(cmdEvtIntIrqVpu_Body_t);// command size  (include cmd header and body)
		cmdMsg->sessionID = 0xFFFFFFFF;// session id
		cmdMsg->timeStamp = 0xFFFFFFFF;// time stamp, default current time in ms
		cmdMsg->seqNum    = 0x00;
		cmdBody->vcmdmgr_id  = vcmd_mgr->vcmd_mgr_id;
		cmdBody->cmdbuf_id  = obj->cmdbuf_id;
		BQueueQueue(mgr->cmd_queue, cmdMsg);
	}

#endif
	return 0;
}

/**
 * @brief wait a cmdbuf runs done.
 * @param u16 cmdbuf_id: the id of cmdbuf to wait.
 * @param u16 *done_id: point to the id of done cmdbuf.
 * @return long: 0: succeed; oters: failed.
 */
int32_t vcmd_wait_cmdbuf_ready(vcmd_mgr_t *vcmd_mgr, u16 cmdbuf_id, u16 *done_id)
{
	struct cmdbuf_obj *obj = NULL;
    int32_t errCode = CMD_ERR_SUCCESS, ret;
	if (cmdbuf_id != ANY_CMDBUF_ID) {
        vcmd_klog(LOGLVL_FLOW, "%s\n", __func__);
        obj = &vcmd_mgr->objs[cmdbuf_id];
	}

//	errCode = wait_event_interruptible(vcmd_mgr->job_waitq, vcmd_get_done_job(vcmd_mgr, &obj));
//	if (errCode == pdFALSE) {
//		return -ERESTARTSYS;
//	}

	*done_id = obj->cmdbuf_id;
	if (obj->cmdbuf_run_done == 1) {
		return 0;
	} else {
		ret = _is_abnormal_run_done(obj);
		if (ret)
			_abnormal_run_done_clear(obj);
		else
			ret = -1;
		return ret;
	}
}

/**
 * @brief check if has abnormal run done
 * @return int 0: not run done; > 0: run done
 */
int _is_abnormal_run_done(struct cmdbuf_obj *obj)
{
	if ((obj->slice_run_done && obj->line_buffer_run_done) == 1)
		return 3;
	else if (obj->slice_run_done == 1)
		return 1;
	else if (obj->line_buffer_run_done == 1)
		return 2;

	return 0;
}

/**
 * @brief clear abnormal run done flag
 */
void _abnormal_run_done_clear(struct cmdbuf_obj *obj)
{
	if (obj->slice_run_done && obj->line_buffer_run_done) {
		obj->slice_run_done = 0;
		obj->line_buffer_run_done = 0;
	} else if (obj->slice_run_done == 1) {
		obj->slice_run_done = 0;
	} else if (obj->line_buffer_run_done == 1) {
		obj->line_buffer_run_done = 0;
	}
}

/**
 * @brief reset all vcmd hw devices.
 */
void vcmd_reset_asic(vcmd_mgr_t *vcmd_mgr)
{
	int n;
	u32 status;
	struct hantrovcmd_dev *dev = vcmd_mgr->dev_ctx;

	for (n = 0; n < vcmd_mgr->subsys_num; n++) {
		if (dev[n].hwregs) {
			//disable interrupt at first
			vcmd_write_reg((const void *)dev[n].hwregs,
					   VCMD_REGISTER_INT_CTL_OFFSET, 0x0000);
			//reset core
			vcmd_write_reg((const void *)dev[n].hwregs,
					   VCMD_REGISTER_CONTROL_OFFSET, 0x0004);
			//read status register
			status = vcmd_read_reg((const void *)dev[n].hwregs,
						   VCMD_REGISTER_INT_STATUS_OFFSET);
			//clean status register
			vcmd_write_reg((const void *)dev[n].hwregs,
					   VCMD_REGISTER_INT_STATUS_OFFSET, status);
			//when reset core need clear reg[3]
			vcmd_write_reg((const void *)dev[n].hwregs,
							VCMD_REGISTER_EXE_CMDBUF_COUNT_OFFSET, 0x0000);
		}
	}
}

/*
 * @brief abort vce when needed
 */
int abort_vce(volatile u8 *reg_base)
{
	u32 status;

	status = (u32)ioread32((void __iomem *)(reg_base + 0x14));
	if (status & 0x1) {
		//Stop VCE by setting reg5 bit0 to 0.
		status &= (~0x01);
		iowrite32(status, (void __iomem *)(reg_base + 0x14));
		return 1;
	}
	return 0;
}

/**
 * @brief stop vcd normall
 */
int abort_vcd(volatile u8 *reg_base)
{
	u32 status;//#define HANTRODEC_IRQ_STAT_DEC       1  #define HANTRODEC_IRQ_STAT_DEC_OFF   (HANTRODEC_IRQ_STAT_DEC * 4)

	status = (u32)ioread32((void __iomem *)(reg_base + 0x04));//status = (u32)ioread32((void __iomem *)(reg_base + HANTRODEC_IRQ_STAT_DEC_OFF));
	if (status & 0x1) {
		//abort vcd
		status |= 0x20;//status |= HANTRODEC_DEC_ABORT;#define HANTRODEC_DEC_ABORT          0x20
		iowrite32(status, (void __iomem *)(reg_base + 0x04));//iowrite32(status, (void __iomem *)(reg_base + HANTRODEC_IRQ_STAT_DEC_OFF));

		return 1;
	}

	return 0;
}

/**
 * @brief set vcmd abort by immediate mode for some special scenes
 */
u32 vcmd_abort_mode_set(vcmd_mgr_t *vcmd_mgr, struct hantrovcmd_dev *dev, struct cmdbuf_obj *obj)
{
	uint32_t   flags;

	/* abort for slice decoding */
	if (!obj->cmdbuf_run_done && !obj->slice_run_done) { 	
        uint64_t  timeout = arch_get_time_ms() + ONE_SLICE_WAIT_TIME;
		while(1) {
			if ((atomic_get(&dev->buff_empty_waitq) > 0) ||
				(time_after(timeout))) {
				break;
			}
		}

		if (atomic_get(&dev->buff_empty_waitq) > 0) {
			atomic_dec(&dev->buff_empty_waitq);
		}
	}

	flags = spin_lock_irqsave(dev->spinlock);
	if (!obj->cmdbuf_run_done && obj->slice_run_done) {
		/* abort vcmd by immediate mode */
		dev->abort_mode = 0x1;
		vcmd_set_reg_mirror(dev->reg_mirror, HWIF_VCMD_ABORT_MODE, dev->abort_mode);
	}
	spin_unlock_irqrestore(dev->spinlock, flags);

	return dev->abort_mode;
}

/**
 * @brief get current executing cmdbuf by vcmd register
 * @param bi_list_node *node: the current executing cmdbuf node
 */
void vcmd_get_executing_cmdbuf(vcmd_mgr_t *vcmd_mgr, struct hantrovcmd_dev *dev,
									bi_list_node **node)
{
	u32 curr_id;

	curr_id = vcmd_get_register_value((const void *)dev->hwregs,
									dev->reg_mirror,
									HWIF_VCMD_CMDBUF_EXE_ID);
	*node = &vcmd_mgr->nodes[curr_id];
}


/**
 * @brief flush slice regs in vcmd driver
 */
int32_t vcmd_flush_slice_regs(vcmd_mgr_t *vcmd_mgr, u16 cmdbuf_id)
{
	struct hantrovcmd_dev *dev = NULL;
	struct cmdbuf_obj *obj = NULL;
	uint32_t   flags;
	obj = &vcmd_mgr->objs[cmdbuf_id];
	dev = &vcmd_mgr->dev_ctx[obj->core_id];
	flags = spin_lock_irqsave(&dev->abn_irq_lock);
	obj->slice_run_done = 0;
	spin_unlock_irqrestore(&dev->abn_irq_lock, flags);

	vcmd_write_reg((const void *)dev->hwregs,  VCMD_REGISTER_EXT_INT_GATE_OFFSET, dev->intr_gate_mask);

	return 0;
}

/**
 * @brief polling cmdbuf in vcmd driver
 */
int32_t vcmd_polling_cmdbuf(vcmd_mgr_t *vcmd_mgr, u16 core_id)
{
#ifdef VCMD_ALLOC_MEM
	int32_t   irq = core_id;
    if (core_id != 0xFFFF) {
        hantrovcmd_isr(irq, vcmd_mgr);
	} else {
		for (irq = 0; irq < vcmd_mgr->subsys_num; irq++) {
	        hantrovcmd_isr(irq, vcmd_mgr);
		}
	}
#endif
	return 0;
}

/**
 * @brief abort the decoder of the cmdbuf_id which is waiting more slice.
 */
int32_t vcmd_abort_cmdbuf(vcmd_mgr_t *vcmd_mgr, u16 cmdbuf_id)
{
	struct hantrovcmd_dev *dev = NULL;
	struct cmdbuf_obj *obj = NULL;
	bi_list_node *node = NULL;
	volatile u8 *hwregs;
	u16  reg_id_exe;
	uint32_t   flags;

	node = &vcmd_mgr->nodes[cmdbuf_id];
	obj = (struct cmdbuf_obj *)node->data;

	dev = &vcmd_mgr->dev_ctx[obj->core_id];
	hwregs = dev->subsys_info->hwregs[SUB_MOD_MAIN];
	reg_id_exe = (u16)(*(dev->reg_mem_va + REG_ID2_CMDBUF_EXE_ID));
	if (cmdbuf_id == reg_id_exe) {
		flags = spin_lock_irqsave(dev->spinlock);
		abort_vcd(hwregs);
		spin_unlock_irqrestore(dev->spinlock, flags);
	} else {
		//should not happen
		vcmd_klog(LOGLVL_ERROR, "%s: ERROR cmdbuf id not match with current dev!\n", __func__);
		return CMD_ERR_INVALID_CMDBUFID;
	}

	return 0;
}

int32_t vcmd_drop_owner(vcmd_mgr_t *vcmd_mgr, cmdr52_session_t *session, uint64_t ownerID, cmdRspDropOwner_Body_t *cmd_body) {
	struct hantrovcmd_dev *dev;
	bi_list *list;
	bi_list_node *node;
	struct cmdbuf_obj *obj;

	u32 i, handled, to_drop;
	u32 has_work_node, vcmd_aborted, aborted_cmdbuf_id;
	uint32_t   flags;
//	int ;
	long dropped_cmdbuf_num = 0;

	handled = 0;
	//remove nodes in dev->work_list
	for (i = 0; i < vcmd_mgr->subsys_num; i++) {
		dev = &vcmd_mgr->dev_ctx[i];
		if (dev == NULL || dev->hwregs == NULL)
			continue;

		list = &dev->work_list;

		has_work_node = 0;
		vcmd_aborted = 0;

		flags = spin_lock_irqsave(dev->spinlock);
		node = list->head;
		while (node) {
			obj = (struct cmdbuf_obj *)node->data;
			if (obj->session == session) {
				if ((ownerID != 0x00) && (ownerID == obj->owner)) {
					has_work_node = 1;
					break;
				}
			}
			node = node->next;
		}

		if (has_work_node) {
			if (dev->state == VCMD_STATE_WORKING) {
				vcmd_klog(LOGLVL_FLOW, "Abort dev[%d].\n", dev->core_id);
				vcmd_get_executing_cmdbuf(vcmd_mgr, dev, &node);
				obj = (struct cmdbuf_obj *)node->data;
				spin_unlock_irqrestore(dev->spinlock, flags);

				vcmd_abort_mode_set(vcmd_mgr, dev, obj);
				vcmd_abort(vcmd_mgr, dev, &aborted_cmdbuf_id);

				flags = spin_lock_irqsave(dev->spinlock);
				if (dev->abort_mode == 1)
					dev->abort_mode = 0;
				if (dev->state != VCMD_STATE_IDLE) {
					vcmd_klog(LOGLVL_ERROR, "dev [%d] is not aborted as expected.", dev->core_id);
					spin_unlock_irqrestore(dev->spinlock, flags);
					continue;
				}
				vcmd_aborted = 1;

				if (!obj->cmdbuf_run_done && obj->slice_run_done)
					abort_vcd(dev->subsys_info->hwregs[SUB_MOD_MAIN]);
			}

			node = list->head;
			while (node) {
				obj = (struct cmdbuf_obj *)node->data;
				if (obj->session == session || obj->cmdbuf_need_remove) {
					to_drop = 0;
					if ((ownerID != 0x00) && (ownerID == obj->owner) && !obj->cmdbuf_run_done) {
						to_drop = 1;
						handled++;
					}
					if (ownerID == 0x00 || to_drop == 1) {
						vcmd_klog(LOGLVL_FLOW, "cmdbuf %d of process %p is released or dropped\n", obj->cmdbuf_id, obj->session);
						dev_remove_job(vcmd_mgr, dev, node);
					}
				}

				node = node->next;
			}

			vcmd_klog(LOGLVL_FLOW, "Restart dev[%d].\n", dev->core_id);
			if (vcmd_aborted == 1)
				vcmd_start(dev, 0);
		}
		spin_unlock_irqrestore(dev->spinlock, flags);
	}

	if ((ownerID != 0x00) && handled) {
		dropped_cmdbuf_num = handled;
//		wake_up_interruptible_all(&vcmd_mgr->job_waitq);
	}

	cmd_body->cmdbuf_num = dropped_cmdbuf_num;
	return 0;
}