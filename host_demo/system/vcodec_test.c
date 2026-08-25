
#include "crc32.h"
#include "system.h"
#include "vcx_vcmd.h"
#include "cmda78_msg.h"
#include "cmda78_mgr.h"
#include "cmda78_proc.h"
#include "vcodec_test.h"
#include "cmda78_session.h"



static int  encode_test_func(void) {
    struct proc_obj *po = NULL;
    int32_t code = 0;
	po = create_process_object();
	if (!po) {
		return -1;
    }

    ts_printf("%s:%s:%d\n", __FILE__, __func__, __LINE__);

    code = cmda78_gen_open_session(po, R52_CORE_MASK_VENC);
	if (code < 0) {
		ts_printf("Open session failed!\n");
		free_process_object(po);
		return -2;
	}

    for(int i = 0; i < 10; i++) {
        struct exchange_parameter  param = {0};
        uint16_t cmdbuf_id = 0;
        param.owner  = po;//input ;executing_time=encoded_image_size*(rdoLevel+1)*(rdoq+1);
        param.executing_time = 1920 * 1080 * 2 * 2;
        param.interrupt_ctrl = 1920 * 1080 * 2 * 2; //input ;executing_time=encoded_image_size*(rdoLevel+1)*(rdoq+1);
        param.module_type = VCMD_TYPE_ENCODER;	/*input input vce=0,IM=1,vcd=2, jpege=3, jpegd=4 */
        param.cmdbuf_size = 256;	/*input, reserve is not used; link and run is input.*/
        param.cmdbuf_id   = 0;	/*input, reserve is not used; link and run is input.*/
        param.core_id     = 0x1;/* just used for polling. */
        param.core_mask   = 0xffff;
        param.input_mask  = 0xffff;
        code = reserve_cmdbuf(cmda78_get_vcmd_mgr(VCMD_MGR_ID_ENC), po, &param);
        cmdbuf_id = param.cmdbuf_id;
        ts_printf("%s:%s:%d %d %d\n", __FILE__, __func__, __LINE__,cmdbuf_id, code);
        code = link_and_run_cmdbuf(cmda78_get_vcmd_mgr(VCMD_MGR_ID_ENC), po, &param);
        ts_printf("%s:%s:%d %d %d\n", __FILE__, __func__, __LINE__,cmdbuf_id, code);
        code = wait_cmdbuf_ready(cmda78_get_vcmd_mgr(VCMD_MGR_ID_ENC), po, cmdbuf_id, &cmdbuf_id);
        ts_printf("%s:%s:%d %d %d\n", __FILE__, __func__, __LINE__,cmdbuf_id, code);
        code = release_cmdbuf(cmda78_get_vcmd_mgr(VCMD_MGR_ID_ENC), po, cmdbuf_id);
        ts_printf("%s:%s:%d %d %d\n", __FILE__, __func__, __LINE__,cmdbuf_id, code);
    }

    code = cmda78_gen_close_session(po, R52_CORE_MASK_VENC);

    ts_printf("%s:%s:%d\n", __FILE__, __func__, __LINE__);
    free_process_object(po);
    return 0;
}

static void encode_thread_func(void *arg) {
   // vcmd_mgr_t *vcmd_mgr = ((vcmd_mgr_t *)arg);
    ts_printf("%s:%s:%d started\n", __FILE__, __func__, __LINE__);
    while (1) {
        encode_test_func();
        vTaskDelay(1000);
    }
    ts_printf("%s:%s:%d exiting\n", __FILE__, __func__, __LINE__);
}

int   vcodec_test_encode() {
    TaskHandle_t xHandle = NULL;
    BaseType_t retCode;

    ts_printf("%s:%s:%d\n", __FILE__, __func__, __LINE__);
    retCode = xTaskCreate(encode_thread_func, "encode_thread", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, &xHandle);
    configASSERT(xHandle);
    if (xHandle == NULL) {
        ts_printf("Failed to create work thread\n");
        return -1;
    }

    ts_printf("%s:%s:%d\n", __FILE__, __func__, __LINE__);
    return 0;
}

static int  decode_test_func(void) {
    struct proc_obj *po = NULL;
    int32_t code = 0;
	po = create_process_object();
	if (!po) {
		return -1;
    }

    ts_printf("%s:%s:%d\n", __FILE__, __func__, __LINE__);

    code = cmda78_gen_open_session(po, R52_CORE_MASK_VDEC);
	if (code < 0) {
		ts_printf("Open session failed!\n");
		free_process_object(po);
		return -2;
	}

    for(int i = 0; i < 10; i++) {
        struct exchange_parameter  param = {0};
        uint16_t cmdbuf_id = 0;
        param.owner  = po;//input ;executing_time=encoded_image_size*(rdoLevel+1)*(rdoq+1);
        param.executing_time = 1920 * 1080 * 2 * 2;
        param.interrupt_ctrl = 1920 * 1080 * 2 * 2; //input ;executing_time=encoded_image_size*(rdoLevel+1)*(rdoq+1);
        param.module_type = VCMD_TYPE_DECODER;	/*input input vce=0,IM=1,vcd=2, jpege=3, jpegd=4 */
        param.cmdbuf_size = 256;	/*input, reserve is not used; link and run is input.*/
        param.cmdbuf_id   = 0;	/*input, reserve is not used; link and run is input.*/
        param.core_id     = 0x1;/* just used for polling. */
        param.core_mask   = 0xffff;
        param.input_mask  = 0xffff;
        code = reserve_cmdbuf(cmda78_get_vcmd_mgr(VCMD_MGR_ID_DEC), po, &param);
        cmdbuf_id = param.cmdbuf_id;
        ts_printf("%s:%s:%d %d %d\n", __FILE__, __func__, __LINE__,cmdbuf_id, code);
        code = link_and_run_cmdbuf(cmda78_get_vcmd_mgr(VCMD_MGR_ID_DEC), po, &param);
        ts_printf("%s:%s:%d %d %d\n", __FILE__, __func__, __LINE__,cmdbuf_id, code);
        code = wait_cmdbuf_ready(cmda78_get_vcmd_mgr(VCMD_MGR_ID_DEC), po, cmdbuf_id, &cmdbuf_id);
        ts_printf("%s:%s:%d %d %d\n", __FILE__, __func__, __LINE__,cmdbuf_id, code);
        code = release_cmdbuf(cmda78_get_vcmd_mgr(VCMD_MGR_ID_DEC), po, cmdbuf_id);
        ts_printf("%s:%s:%d %d %d\n", __FILE__, __func__, __LINE__,cmdbuf_id, code);
    }

    code = cmda78_gen_close_session(po, R52_CORE_MASK_VDEC);

    ts_printf("%s:%s:%d\n", __FILE__, __func__, __LINE__);
    free_process_object(po);
    return 0;
}

static void decode_thread_func(void *arg) {
   // vcmd_mgr_t *vcmd_mgr = ((vcmd_mgr_t *)arg);
    ts_printf("%s:%s:%d started\n", __FILE__, __func__, __LINE__);
    while (1) {
        decode_test_func();
        vTaskDelay(1000);
    }
    ts_printf("%s:%s:%d exiting\n", __FILE__, __func__, __LINE__);
}


int   vcodec_test_decode() {
    TaskHandle_t xHandle = NULL;
    BaseType_t retCode;

    ts_printf("%s:%s:%d\n", __FILE__, __func__, __LINE__);
    retCode = xTaskCreate(decode_thread_func, "decode_thread", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, &xHandle);
    configASSERT(xHandle);
    if (xHandle == NULL) {
        ts_printf("Failed to create work thread\n");
        return -1;
    }

    ts_printf("%s:%s:%d\n", __FILE__, __func__, __LINE__);
    return 0;
}


