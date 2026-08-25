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
**                       include vcx vcmd headers                               **
*********************************************************************************/

#ifndef _VCX_VCMD_H_
#define _VCX_VCMD_H_

#ifdef __FREERTOS__
#include "osal_freertos.h" /* needed for the _IOW etc stuff used later */
#endif

#include "vcx_vcmd_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

struct cmdbuf_mem_parameter {
	u32 *cmd_virt_addr; //cmdbuf pool base virtual address
	ptr_t cmd_phy_addr; //cmdbuf pool base physical address, it's for cpu
	ptr_t cmd_hw_addr; //cmdbuf pool base hardware address, it's for hardware ip
	u32 cmd_total_size; //cmdbuf pool total size in bytes.
	u16 cmd_unit_size; //one cmdbuf size in bytes. all cmdbuf have same size.
	u32 *status_virt_addr;
	ptr_t status_phy_addr; //status cmdbuf pool base physical address, it's for cpu
	ptr_t status_hw_addr; //status cmdbuf pool base hardware address, it's for hardware ip
	u32 status_total_size; //status cmdbuf pool total size in bytes.
	u16 status_unit_size; //one status cmdbuf size in bytes. all status cmdbuf have same size.
	ptr_t base_ddr_addr; //for pcie interface, hw can only access phy_cmdbuf_addr-pcie_base_ddr_addr.
		//for other interface, this value should be 0?
	u32 *reg_virt_addr; //register cmdbuf pool base virtual address
	ptr_t reg_phy_addr; //register cmdbuf pool base physical address, it's for cpu
	ptr_t reg_hw_addr; //register cmdbuf pool base hardware address, it's for hardware ip
	u32 reg_total_size; //register cmdbuf pool total size in bytes.
	u32 reg_unit_size; //one reg cmdbuf size in bytes. all status cmdbuf have same size.
};

struct config_parameter {
	u16 module_type; //input vce=0,cutree=1,vcd=2，jpege=3, jpegd=4
	u16 vcmd_core_num; //output, how many vcmd cores are there with corresponding module_type.
	u16 submodule_main_addr; //output,if submodule addr == 0xffff, this submodule does not exist.
	u16 status_main_addr; //output, main ip offset instatus buffer.
	u16 submodule_dec400_addr; //output ,if submodule addr == 0xffff, this submodule does not exist.
	u16 status_dec400_addr; //output, dec400 ip offset instatus buffer.
	u16 submodule_L2Cache_addr; //output,if submodule addr == 0xffff, this submodule does not exist.
	u16 status_L2Cache_addr; //output, L2Cache ip offset instatus buffer.

	u16 submodule_MMU_addr[2]; //output,if submodule addr == 0xffff, this submodule does not exist.
	u16 status_MMU_addr[2]; //output, MMU ip offset instatus buffer.
	u16 submodule_axife_addr[2]; //output,if submodule addr == 0xffff, this submodule does not exist.
	u16 status_axife_addr[2]; //output, axife ip offset instatus buffer.
	u16 submodule_ufbc_addr; //output ,if submodule addr == 0xffff, this submodule does not exist.
	u16 status_ufbc_addr; //output, ufbc ip offset instatus buffer.
	u32 vcmd_hw_version_id;
	u32 vcmd_priority[MAX_VCMD_CORE_NUM]; //specify the priority of vcmd
};

/*need to consider how many memory should be allocated for status.*/
struct exchange_parameter {
	/** control interrupt mode when generate JMP command
	 * bit31 is mode_flag.
	 *	- when mode_flag is 0, adapative interrupt mode is selected. in such
	 *	  mode, bit[30:0] is executing time estimated for current job;
	 *	- when mode_flag is 1, manual interrupt mode is selected. in such mode,
	 *	  bit[0] is used to set IE flag in JMP command.
	 *	  bit[39:32] is batch count.
	 */
	u64 interrupt_ctrl; //input ;executing_time=encoded_image_size*(rdoLevel+1)*(rdoq+1);
	u16 module_type; //input input vce=0,IM=1,vcd=2，jpege=3, jpegd=4
	u16 cmdbuf_size; //input, reserve is not used; link and run is input.
	u16 cmdbuf_id; //output ,it is unique in driver.
	u16 core_id; //just used for polling.
	u16 core_mask; //core_mask for user to select cores
	/* input, bit[0]: priority    - normal=0, high/live=1
	 *        bit[1]: has_end_cmd - last cmd is JMP (0) or END (1) command
	 */
	u16 input_mask;
};

#define CORE_REGS_WR_NUM    16
struct core_regs_wr {
	u32 id;         /* id of core to be written */
	u32 type;      /* type of core to be written */
	u32 reg_num;      /* num of register to be written */
	u32 reg_id;    /* start id of reigster to be written */
	u32 reg_val[CORE_REGS_WR_NUM]; /* value of reigster to be written */
};

struct reg_desc {
	u32 reg_id; //id of the register
	u32 reg_data; // data of register
};

struct subsys_regs_desc {
	u32 id; /* id of the subsystem */
	u32 reg_num; /* num of reg to access */
	/* store some user registers id and data */
	struct reg_desc regs[CORE_REGS_WR_NUM];
};

#ifdef __cplusplus
}
#endif

#endif //_VCX_VCMD_H_
