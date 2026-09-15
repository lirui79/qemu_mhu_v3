#ifndef __HOST_H__
#define __HOST_H__

#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Common definition                                                  */
/* ------------------------------------------------------------------ */

/* event sent to host */
#define TS_EVENT_STARTUP                (1u << 0)   /* MHU doorbell channel 0 */
#define TS_EVENT_KERNEL_COMPLETE        (1u << 31)  /* MHU doorbell channel 0 */

/* event received from host */
#define HOST_EVENT_STARTUP              (1u << 0)   /* MHU doorbell channel 0 */

/* maximum number of host process allowed */
#define HOST_PROCESS_MAX_NUM            16

/* maximum number of host queues allowed, limited by doorbell count */
#define HOST_QUEUE_MAX_NUM              128

/* maximum number of address translation table entry allowed */
#define HOST_TLB_ENT_MAX_NUM            8

#endif
