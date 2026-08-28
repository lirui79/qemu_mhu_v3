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
**                           *.c mhu v3 source code                             **
*********************************************************************************/

#include "inc.h"
#include "vcx_priv.h"
#include "mhu_v3_priv.h"
#include "mhu_v3_client.h"
#include <linux/platform_device.h>
//#include <linux/mailbox_client.h>
#include <linux/workqueue.h>
#include <linux/completion.h>
#include <linux/interrupt.h>


typedef enum {
    INT_PRIO_LOWEST   = 0xFF,
    INT_PRIO_LOW      = 0xBF,
    INT_PRIO_NORMAL   = 0x7F,
    INT_PRIO_HIGH     = 0x3F,
    INT_PRIO_HIGHEST  = 0x00,
} INT_PRIORITY;

#define MHU_DATA_SIZE      128
#define TX_TIMEOUT_MS      1000


struct mhu_v3_manager {
	struct device       *dev;
	uintptr_t            pbx;
	uintptr_t            mbx;
	phys_addr_t          pbx_phys;
	phys_addr_t          mbx_phys;
	resource_size_t      pbx_size;
	resource_size_t      mbx_size;
	int                  rx_irq;
	spinlock_t           rlock[MHU_MAX_CLIENTS];
	spinlock_t           wlock[MHU_MAX_CLIENTS];
	struct completion    fifo_completion;
	wait_queue_head_t    waitq;

	atomic64_t           submitted;
	atomic64_t           completed;

    uint32_t             g_mbx_fc_data_0[32];  // fast channel data buffer
};

static struct mhu_v3_manager  g_mhu_v3_mgr;

/*
 * Recv 128-byte data via Mailbox
 */
int mhu_v3_recv_data(uint32_t r52id, u8 *data, uint32_t *size) {
    uint32_t ch = 2 * r52id + 1;
    struct mhu_v3_manager *mgr    = (struct mhu_v3_manager *)(vcx_get_private(DEVID_VCX)->priv);
    uint32_t *dwptr = (uint32_t*)data, buf_len = *size;
    uint32_t  dwlen = (buf_len / 4);
    uint32_t  fill, val, len = 0, i = 0,st;
    uint32_t  flags, stale;
    uint8_t   sot = 0, eot = 0;

    if ((buf_len % 4) != 0) {
        printk("MHUS: invalid len %u\n", buf_len);
        return -1;
    }

    while (1) {
        if (len >= dwlen) {
            printk("MBX: no enough buffer, len=%u\n", buf_len);
            break;
        }

        /* wait for fifo data */
        do {
            st   = mhu_read32(mgr->mbx + MHU_MBX_FFCW_ST(ch));
            fill = st & 0x7FF;
        } while (!fill);

        /* pop data and flag from fifo */
 	    spin_lock(&mgr->rlock[r52id]);
        val   = mhu_fifo_pop32(mgr->mbx, ch);
        flags = mhu_read32(mgr->mbx + MHU_MBX_FFCW_FLG(ch));
        stale = mhu_read32(mgr->mbx + MHU_MBX_FFCW_CTRL(ch));
	    spin_unlock(&mgr->rlock[r52id]);

//      printk("MBX: data=0x%x flg=0x%x stale=0x%x st=0x%x\n", val, flags, stale, st);
        if (flags & 0x4) {// 0
            if ((flags & 0x1) != 0)
                sot = 1;
            if ((flags & 0x2) != 0)
                eot = 1;
        }
        if (flags & (0x4 << 4)) {// 1
            if ((flags & (0x1 << 4)) != 0)
                sot = 1;
            if ((flags & (0x2 << 4)) != 0)
                eot = 1;
        }
        if (flags & (0x4 << 8)) {// 1
            if ((flags & (0x1 << 8)) != 0)
                sot = 1;
            if ((flags & (0x2 << 8)) != 0)
                eot = 1;
        }
        if (flags & (0x4 << 12)) {// 1
            if ((flags & (0x1 << 12)) != 0)
                sot = 1;
            if ((flags & (0x2 << 12)) != 0)
                eot = 1;
        }

        /* check for start of transfer boundary */
        if (!sot) {
            printk("MBX: invalid SOT, drop 0x%x\n", val);
            continue;
        }

        /* save data into user buffer */
        dwptr[len++] = val;

        /* check for end of transfer boundary */
        if (eot)
            break;
    }

	spin_lock(&mgr->rlock[r52id]);
    /* check if more data is pending */
    fill = mhu_fifo_rx_fill(mgr->mbx, ch);
    if (!fill) {
        stale = mhu_read32(mgr->mbx + MHU_MBX_FFCW_INT_ST(ch));
        if (stale) {
            mhu_fifo_clear_rx_irq(mgr->mbx, ch, stale);
        }
        mhu_write32(mgr->mbx + MHU_MBX_FFCW_INT_EN(ch), 0xFFFFFFFF);
    }
	spin_unlock(&mgr->rlock[r52id]);
    *size = len * 4;
    printk("MHU V3: Received and copied 128 bytes of data.\n");
    
    return 0;

}

/*
 * Send 128-byte data via Mailbox
 */
int mhu_v3_send_data(uint32_t r52id, const u8 *data_ptr, uint32_t data_len) {
    uint32_t ch = 2 * r52id;
    struct mhu_v3_manager *mgr    = (struct mhu_v3_manager *)(vcx_get_private(DEVID_VCX)->priv);
    uint32_t *dwptr = (uint32_t*)data_ptr;
    uint32_t  dwlen = data_len / 4;
    uint32_t  i, flg;
    if ((data_len % 4) != 0) {
        printk("MHUS: invalid len %u\n", data_len);
        return -1;
    }

	spin_lock(&mgr->wlock[r52id]);
    /* 平台 MHU 模型:数据逐 word 经 PBX FIFO(push32)送达对端 FIFO,
     * 每 word 需写 FFCW_FLG 标记 SOT(首)/EOT+ACK(末)。不能用
     * KICK_DATA+snd_data(那是被禁用的 #else 路径,数据不会进入 FIFO)。 */
    for (i = 0; i < dwlen; i++) {
        flg = 0;
        if (i == 0)
            flg |= 0x02;    /* SOT */
        if (i == (dwlen - 1))
            flg |= 0x04;    /* EOT+ACK */
        mhu_write32(mgr->pbx + MHU_PBX_FFCW_FLG(ch), flg);
        mhu_fifo_push32(mgr->pbx, ch, dwptr[i]);
    }
	spin_unlock(&mgr->wlock[r52id]);
    return data_len;
}

static struct platform_device *mhu_v3_find_platform_device(struct device *dev) {
    struct device_node *mhu_np = NULL;
	struct platform_device *mhu_pdev = NULL;

	mhu_np = of_find_compatible_node(NULL, NULL, "vcx,r52-vpu-mhu");
    if (!mhu_np) {
        pr_err("of_find_compatible_node can not find r52‑vpu node\n");
        return NULL;
    }

    mhu_pdev = of_find_device_by_node(mhu_np);
    if (!mhu_pdev) {
        pr_err("MHU V3: No platform device found for node %s\n", mhu_np->full_name);
        of_node_put(mhu_np);
        return NULL;
    }

    printk("MHU V3 Client find successfully\n");
    return mhu_pdev;
}

static irqreturn_t mhu_irq_handler(int irq, void *data)
{
    struct mhu_v3_manager *mgr = (struct mhu_v3_manager *)data;
    uint32_t db_int = mhu_read32(mgr->mbx + MHU_MBX_DBCH_INT_ST(0));
    uint32_t fc_int = mhu_read32(mgr->mbx + MHU_MBX_FCH_GRP_INT_ST(0));
    uint32_t ff_int = mhu_read32(mgr->mbx + MHU_MBX_FFCH_INT_ST(0));
    uint32_t i, status;
    irqreturn_t ret = IRQ_NONE;

    if (db_int != 0) {
        ret = IRQ_HANDLED;
        for (i = 0; i < 4; i++) {
            if (db_int & (1 << i)) {
                status = mhu_receiver_status(mgr->mbx, i);
                if (status) {
                    mhu_receiver_clear_irq(mgr->mbx, i, status);
                    atomic64_inc(&mgr->completed);
                    wake_up_interruptible(&mgr->waitq);
                }
            }
        }

    }
    if (fc_int != 0) {
        ret = IRQ_HANDLED;
        for (i = 0; i < 32; i++) {
            if (fc_int & (1 << i)) {
                mgr->g_mbx_fc_data_0[i] = mhu_read32(mgr->mbx + MHU_MBX_FCH_PAY32(i));
                /* write 0 to clear FC interrupt latch in hardware,
                 * otherwise subsequent FC notifications won't
                 * generate a new combo-interrupt edge */
                mhu_write32(mgr->mbx + MHU_MBX_FCH_PAY32(i), 0);
            }
        }

        complete_all(&mgr->fifo_completion);
    }

    if (ff_int != 0) {
        ret = IRQ_HANDLED;
        for (i = 0; i < 4; i++) {
            if (ff_int & (1 << i)) {
                uint32_t ff_int_st = mhu_read32(mgr->mbx + MHU_MBX_FFCW_INT_ST(i));
                /* FIFO 中断是电平触发(fill>0 条件持续),仅写 INT_CLR
                 * 不能阻止中断重入——数据仍在 FIFO 里,INT_CLR 写完
                 * 立刻被硬件重新置位。必须禁用通道中断,等 recv 线程
                 * 排空 FIFO 后再重新使能。 */
                mhu_write32(mgr->mbx  + MHU_MBX_FFCW_INT_EN(i), 0);
                mhu_fifo_clear_rx_irq(mgr->mbx , i, ff_int_st);
            }
        }

        complete_all(&mgr->fifo_completion);
    }

    printk("MBX_INT: db=0x%x fc=0x%x ff=0x%x\n", db_int, fc_int, ff_int);
	return ret;
}

static int mhu_init(struct platform_device *pdev, struct mhu_v3_manager *mgr)
{
   uint32_t i, dbch, ffch, fch, iidr;
   int ret;

    dbch = (mhu_read32(mgr->mbx + MHU_MBX_DBCH_CFG0) & 0xFF) + 1; //num of doorbell channel
    ffch = (mhu_read32(mgr->mbx + MHU_MBX_FFCH_CFG0) & 0xFF) + 1; //num of fifo channel
    fch  = (mhu_read32(mgr->mbx + MHU_MBX_FCH_CFG0) & 0x3FF) + 1; //num of fast channel num
    iidr =  mhu_read32(mgr->mbx + MHU_MBX_IIDR);
    printk("MHU: dbch=%u ffch=%u fch=%u iidr=0x%x\n", dbch, ffch, fch, iidr);

       /* Enable MBX doorbell channel interrupt */
    mhu_write32(mgr->mbx + MHU_MBX_DBCH_CTRL,   0x4);      //INT_EN
    mhu_write32(mgr->mbx + MHU_MBX_DBG_INT_EN,  0x1);      //DB group 0

    /* Clear stale FC interrupts, then enable */
    for (i = 0; i < fch && i < 32; i++) {
        mhu_write32(mgr->mbx + MHU_MBX_FCH_PAY32(i), 0);
    }
    mhu_write32(mgr->mbx + MHU_MBX_FCH_CTRL,   0x4);      //INT_EN
    mhu_write32(mgr->mbx + MHU_MBX_FCG_INT_EN, 0x1);

    /* Clear stale FF interrupts before enabling, then enable.
     * 平台 MHU 模型:接收侧必须对每个 FF 通道写 FFCW_CTRL.RA_EN,
     * 数据才会经"读 MFFCW_PAY"弹出并递减 fill;否则对端 push 的
     * 数据虽入 FIFO,但本侧读 ST 的 fill 恒为 0、无法接收。 */
    for (i = 0; i < ffch; i++) {
        uint32_t stale = mhu_read32(mgr->mbx + MHU_MBX_FFCW_INT_ST(i));
        if (stale)
            mhu_write32(mgr->mbx + MHU_MBX_FFCW_INT_CLR(i), stale);
        mhu_write32(mgr->mbx + MHU_MBX_FFCW_INT_EN(i), 0xFFFFFFFF);
        //mhu_write32(mgr->mbx + MHU_MBX_FFCW_CTRL(i), MHU_FF_CTRL_RA_EN);
        stale = mhu_read32(mgr->mbx + MHU_MBX_FFCW_CTRL(i));
        mhu_write32(mgr->mbx + MHU_MBX_FFCW_CTRL(i), stale | 0xF);
    }

	ret = devm_request_irq(&pdev->dev, mgr->rx_irq, mhu_irq_handler, 0, dev_name(&pdev->dev), mgr);
	if (ret) {
		pr_err("MHU V3: Failed to request RX IRQ %d: %d\n", mgr->rx_irq, ret);
		return ret;
    }

	/* The R52 startup doorbell is level/state based; acknowledging is safe. */
	mhu_send_doorbell(mgr->pbx , 0, MHU_DB_STARTUP);
    return 0;
}

/*
 * Probe function: Initialize mailbox client and request channels
 */
int mhu_v3_client_probe(struct platform_device *pdev) {
    struct mhu_v3_manager *mgr = &g_mhu_v3_mgr;
    vcx_priv_t *vcx_priv = platform_get_drvdata(pdev);
    struct device *mhu_dev, *dev = &pdev->dev;
	struct platform_device *mhu_pdev = NULL;
	struct resource *res;
    int ret, i = 0;
    vcx_priv->priv = (void*)mgr;
    mgr->dev = &pdev->dev;
    mhu_pdev = mhu_v3_find_platform_device(dev);
    if (!mhu_pdev) {
        pr_err("MHU V3: Failed to find platform device for MHU\n");
        return -ENODEV;
    }

    res = platform_get_resource_byname(mhu_pdev, IORESOURCE_MEM, "pbx");
	if (!res) {
		pr_err("MHU V3: Failed to get platform resource for PBX\n");
		return -ENODEV;
    }

	mgr->pbx_phys = res->start;
	mgr->pbx_size = resource_size(res);
	mgr->pbx = (uintptr_t)devm_ioremap_resource(&mhu_pdev->dev, res);
	if (IS_ERR(mgr->pbx)) {
		pr_err("MHU V3: Failed to get ioremap resource for PBX\n");
		return PTR_ERR(mgr->pbx);
    }

	res = platform_get_resource_byname(mhu_pdev, IORESOURCE_MEM, "mbx");
	if (!res) {
		pr_err("MHU V3: Failed to get platform resource for MBX\n");
		return -ENODEV;
    }

	mgr->mbx_phys = res->start;
	mgr->mbx_size = resource_size(res);
	mgr->mbx = (uintptr_t)devm_ioremap_resource(&mhu_pdev->dev, res);
	if (IS_ERR(mgr->mbx)) {
		pr_err("MHU V3: Failed to get ioremap resource for MBX\n");
		return PTR_ERR(mgr->mbx);
    }

	mgr->rx_irq = platform_get_irq_byname(mhu_pdev, "rx");
	if (mgr->rx_irq < 0) {
		pr_err("MHU V3: Failed to get platform resource for RX IRQ\n");
		return mgr->rx_irq;
    }

    for (i = 0; i < MHU_MAX_CLIENTS; i++) {
        spin_lock_init(&mgr->rlock[i]);
        spin_lock_init(&mgr->wlock[i]);
    }

	init_completion(&mgr->fifo_completion);
	init_waitqueue_head(&mgr->waitq);
	atomic64_set(&mgr->submitted, 0);
	atomic64_set(&mgr->completed, 0);
//	printk("%s %s %d:\n", __FILE__, __func__, __LINE__);
    ret = mhu_init(pdev, mgr);
    printk("MHU V3 Client probed %d successfully\n", ret);
    return ret;
}

/*
 * Remove function: Cleanup resources
 */
int mhu_v3_client_remove(struct platform_device *pdev) {
    vcx_priv_t *vcx_priv = platform_get_drvdata(pdev);
    struct mhu_v3_manager *mgr =  (struct mhu_v3_manager *)vcx_priv->priv;
    vcx_priv->priv = NULL;
    dev_info(&pdev->dev, "MHU V3 Client removed\n");
    return 0;
}