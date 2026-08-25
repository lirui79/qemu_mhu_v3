#include "vcx_vcmd.h"
#include "vcx_vcmd_priv.h"
#include "vcx_cmdbuf_obj.h"



void vce_proc_add_done_job(vcmd_mgr_t *vcmd_mgr, struct cmdbuf_obj *obj) {
	u16 id = obj->cmdbuf_id;
	unsigned long flags;
	if (!obj->po) {
		ts_printf("%s: the po and cmdbufs of this po has been released!\n", __func__);
		return;
	}

	spin_lock_irqsave(&vcmd_mgr->job_lock, flags);
    obj->cmdbuf_run_done = 1;
	spin_unlock_irqrestore(&vcmd_mgr->job_lock, flags);
    wake_up_interruptible(&vcmd_mgr->job_waitq);//wake_up_interruptible_from_isr
}

void vcd_proc_add_done_job(vcmd_mgr_t *vcmd_mgr, struct cmdbuf_obj *obj) {
	u16 id = obj->cmdbuf_id;
	unsigned long flags;
	if (!obj->po) {
		ts_printf("%s: the po and cmdbufs of this po has been released!\n", __func__);
		return;
	}

	spin_lock_irqsave(&vcmd_mgr->job_lock, flags);
    obj->cmdbuf_run_done = 1;
	spin_unlock_irqrestore(&vcmd_mgr->job_lock, flags);
    wake_up_interruptible(&vcmd_mgr->job_waitq);//wake_up_interruptible_from_isr
}


/**
 * @brief add done obj to job_done_list of its po, wake-up job_waitq if needed.
 */
void vcmd_add_done_job(vcmd_mgr_t *vcmd_mgr, struct cmdbuf_obj *obj)
{
	u16 id = obj->cmdbuf_id;
	unsigned long flags;
	if (!obj->session) {
		ts_printf("%s: the session and cmdbufs of this session has been released!\n", __func__);
		return;
	}

	spin_lock_irqsave(&vcmd_mgr->job_lock, flags);
    obj->cmdbuf_run_done = 1;
	spin_unlock_irqrestore(&vcmd_mgr->job_lock, flags);
    wake_up_interruptible(&vcmd_mgr->job_waitq);//wake_up_interruptible_from_isr
}

int32_t vcmd_release_cmdbuf(vcmd_mgr_t *vcmd_mgr, u16 cmdbuf_id) {
	struct cmdbuf_obj *obj = NULL;
	if (cmdbuf_id >= SLOT_NUM_CMDBUF) {
		//should not happen
		ts_printf("%s %s %d: ERROR cmdbuf_id %d!!\n", __FILE__, __func__, __LINE__, cmdbuf_id);
		return -1;
	}

	obj = &vcmd_mgr->objs[cmdbuf_id];

	obj->owner = 0;
	obj->session = NULL;
	obj->po   =  NULL;
	obj->cmdbuf_run_done = 0;
	obj->slice_run_done = 0;
	obj->line_buffer_run_done = 0;
    return 0;
}


/**
 * @brief check if has abnormal run done
 * @return int 0: not run done; > 0: run done
 */
static int _is_abnormal_run_done(struct cmdbuf_obj *obj)
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
static void _abnormal_run_done_clear(struct cmdbuf_obj *obj)
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


int vcmd_get_done_job(vcmd_mgr_t *vcmd_mgr, struct cmdbuf_obj **pobj)
{
	unsigned long flags;
	struct cmdbuf_obj *obj = NULL;
	int is_done = 0, i = 0;

	spin_lock_irqsave(&vcmd_mgr->job_lock, flags);

	if (*pobj == NULL) {
		//any po's cmdbuf ready, return head of job_done_list
        for(i = 0 ; i < SLOT_NUM_CMDBUF; ++i) {
             obj = &vcmd_mgr->objs[i];
             if (obj->cmdbuf_run_done || _is_abnormal_run_done(obj)) {
                 *pobj = obj;
		         is_done = 1;
                 break;
             }
        }
	} else {
		//specified cmdbuf ready?
		obj = *pobj;
		if (obj->module_type == VCMD_TYPE_ENCODER) {
			is_done = obj->cmdbuf_run_done || _is_abnormal_run_done(obj);
		}
		if (obj->module_type == VCMD_TYPE_DECODER) {
			is_done = obj->cmdbuf_run_done || obj->slice_run_done;
		}
	}

	spin_unlock_irqrestore(&vcmd_mgr->job_lock, flags);

	return is_done;
}



int32_t vcmd_wait_cmdbuf_ready(vcmd_mgr_t *vcmd_mgr, u16 cmdbuf_id, u16 *done_id) {
	struct cmdbuf_obj *obj = NULL;
	long ret, retCode;
	if (cmdbuf_id != ANY_CMDBUF_ID) {
        ts_printf("%s\n", __func__);
        obj = &vcmd_mgr->objs[cmdbuf_id];
	}

    retCode = wait_event_interruptible(vcmd_mgr->job_waitq, vcmd_get_done_job(vcmd_mgr, &obj) > 0);
    ts_printf("%s:%s:%d %d\n", __FILE__, __func__, __LINE__, retCode);
    if (retCode == pdFALSE) {
        return -1;
    }

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
    return -1;
}

int32_t wait_cmdbuf_ready(vcmd_mgr_t *vcmd_mgr, struct proc_obj *po, u16 cmdbuf_id, u16 *done_id)
{
	struct cmdbuf_obj *obj = NULL;
	long ret, retCode;
	if (cmdbuf_id != ANY_CMDBUF_ID) {
		ts_printf("%s %s %d cmdbuf_id %d\n", __FILE__, __func__, __LINE__, cmdbuf_id);
		obj = &vcmd_mgr->objs[cmdbuf_id];
		if (obj->po != po) {
			ts_printf("%s: ERROR cmdbuf filp not match!\n", __func__);
			return -1;
		}
	}

    retCode = wait_event_interruptible(vcmd_mgr->job_waitq, vcmd_get_done_job(vcmd_mgr, &obj) > 0);
    ts_printf("%s:%s:%d return code:%d\n", __FILE__, __func__, __LINE__, retCode);
    if (retCode == pdFALSE) {
        return -1;
    }

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
    return -1;
}


int32_t vcmd_flush_slice_regs(vcmd_mgr_t *vcmd_mgr, u16 cmdbuf_id){
	struct cmdbuf_obj *obj = NULL;
	unsigned long flags;
	obj = &vcmd_mgr->objs[cmdbuf_id];
	obj->slice_run_done = 0;
    return 0;
}


int32_t vcmd_polling_cmdbuf(vcmd_mgr_t *vcmd_mgr, u16 core_id){
    return 0;
}

int32_t vcmd_abort_cmdbuf(vcmd_mgr_t *vcmd_mgr, u16 cmdbuf_id){
    struct cmdbuf_obj *obj = NULL;
	obj = &vcmd_mgr->objs[cmdbuf_id];
    obj->owner   = 0;
    obj->session = NULL;
    return 0;
}


void vcmd_mgr_set_vcmd_mgr_id(vcmd_mgr_t *vcmd_mgr, uint32_t vcmd_mgr_id) {
    vcmd_mgr->vcmd_mgr_id = vcmd_mgr_id;
}

struct proc_obj *create_process_object(void) {
	struct proc_obj *po = NULL;
	po = vmalloc(sizeof(struct proc_obj));
	if (!po) {
		ts_printf("%s: vmalloc failed!\n", __func__);
		return NULL;
	}

	memset(po, 0, sizeof(struct proc_obj));
	return po;
}


/**
 * @brief free a process object
 */
void free_process_object(struct proc_obj *po)
{
	if (!po) {
		ts_printf("%s: po is NULL!\n", __func__);
		return;
	}
	vfree(po);
}

static void reset_cmdbuf_obj(vcmd_mgr_t *vcmd_mgr, u32 id)
{
	struct cmdbuf_obj *obj = &vcmd_mgr->objs[id];

	obj->cmdbuf_run_done = 0;
	obj->slice_run_done = 0;
	obj->line_buffer_run_done = 0;
	obj->core_id = 0xFFFF;
	obj->cmdbuf_size = 256;
	obj->owner  = 0;
	obj->interrupt_ctrl = 0;
	obj->module_type;
	obj->core_mask;
	obj->owner = 0;
	obj->session = NULL;
	obj->po   =  NULL;
}

long      reserve_cmdbuf(vcmd_mgr_t *vcmd_mgr, struct proc_obj *po, struct exchange_parameter *param) {
	struct cmdbuf_obj *obj;
	 static u32 cmdbuf_id = 0;

	if (!po) {
		ts_printf("%s: not find process obj!\n", __func__);
		return -1;
	}

	reset_cmdbuf_obj(vcmd_mgr, cmdbuf_id);

	obj = &vcmd_mgr->objs[cmdbuf_id];
	obj->module_type = param->module_type;
	obj->interrupt_ctrl = param->executing_time;
	obj->po = po;
	obj->owner = (uint64_t)param->owner;
	obj->core_mask = param->core_mask;

	param->cmdbuf_size = 256;
	param->cmdbuf_id = cmdbuf_id++;
	if (cmdbuf_id >= SLOT_NUM_CMDBUF)
		cmdbuf_id = 0;

	return 0;
}

long      release_cmdbuf(vcmd_mgr_t *vcmd_mgr, struct proc_obj *po, u16 cmdbuf_id) {
	struct cmdbuf_obj *obj = NULL;
	if (cmdbuf_id >= SLOT_NUM_CMDBUF) {
		ts_printf("%s %s %d: ERROR cmdbuf_id %d!!\n", __FILE__, __func__, __LINE__, cmdbuf_id);
		return -1;
	}

	obj = &vcmd_mgr->objs[cmdbuf_id];
	if (obj->po != po) {
		ts_printf("%s: cmdbuf[%d] po not match: owned by %p, released by %p!!\n",	__func__, cmdbuf_id, obj->po, po);
		return -1;
	}

	obj->owner = 0;
	obj->session = NULL;
	obj->po   =  NULL;
	obj->cmdbuf_run_done = 0;
	obj->slice_run_done = 0;
	obj->line_buffer_run_done = 0;
	return 0;
}