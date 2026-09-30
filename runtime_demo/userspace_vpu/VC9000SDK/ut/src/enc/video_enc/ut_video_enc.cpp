#include "video_enc_catch2.c"

#include "catch2/catch.hpp"

// --------------------------------------------------------------------------------------
int loop_process_enc = 0;
#if defined  __aarch64__
    std::string funcs_align_out = "funcs_align_out_arm64/";
#elif defined __x86_64__
    std::string funcs_align_out = "funcs_align_out/";
#endif    
TEST_CASE( "unitest_video_enc_sample_normal_enc", "[sample][enc][normal][unitest]" ) {
    params_video_enc_t params_video_enc = {0};
    ++loop_process_enc;
    std::string input_file;
    std::tie(input_file, params_video_enc.width, params_video_enc.height, params_video_enc.pixel_fmt, params_video_enc.firstFrame_time_test, params_video_enc.naltype_check_num) = GENERATE(
        table<std::string, int, int, char*, int, int>({
            std::make_tuple(std::string(UT_RES_PATH) + "cdzj_hevc_1080p_nv12.yuv", 1920, 1080, (char *)"nv12", 0, 8), 
            std::make_tuple(std::string(UT_RES_PATH) + "cdzj_1080p_nv12.yuv", 1920, 1080, (char *)"nv12", 0, 0),
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_rgb/cdzj_h264_rgb.rgb", 1920, 1080, (char *)"rgba", 0, 8), 
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_rgb/cdzj_hevc_rgb.rgb", 1920, 1080, (char *)"rgba", 0, 8), 
            std::make_tuple(std::string(UT_RES_PATH) + "yuv420p10le.yuv", 1920, 1080, (char *)"yuv420p_10bit", 0, 8),
            std::make_tuple(std::string(UT_RES_PATH) + "p010le.yuv", 1920, 1080, (char *)"p010le", 0, 8),
        }));
    
    std::string output_file;
    std::tie(params_video_enc.codec, output_file) = GENERATE(
        table<int, std::string>({
            std::make_tuple(2, std::string(UT_RES_PATH) + "enc/"),
            std::make_tuple(1, std::string(UT_RES_PATH) + "enc/"),
            std::make_tuple(0, std::string(UT_RES_PATH) + "enc/")
        }));
    // auto naltype_check_num = GENERATE(8); // check naltypes of previous frames, av1 not support
    params_video_enc.core_mode = GENERATE(0);
    params_video_enc.vframes = GENERATE(0, 10);
    params_video_enc.stride = GENERATE(0);
    params_video_enc.device = GENERATE((char *)"/dev/vastai_video0");
    params_video_enc.loop = GENERATE(1);
    params_video_enc.store = GENERATE(1);
    params_video_enc.buffer_count = GENERATE(0);
    params_video_enc.idr_indices = GENERATE((char *)"7:9");
    auto json_params = GENERATE(std::string(UT_RES_PATH) + "json/enc/video_enc.json");
    
    params_video_enc.input_file = (char *)input_file.c_str();
    params_video_enc.output_file = (char *)output_file.c_str();
    params_video_enc.json_params = (char *)json_params.c_str();
    SECTION( "video_enc()" ) {
        REQUIRE( video_enc(&params_video_enc) == 0 );
    }
}
// note! AV1 not support functions: roi_qpmap_roiType_2_roiInt_1_roiMapDQpBU_1, roi_qpmap_roiType_2_roiInt_1_roiMapDQpBU_2, extSEIInt_x,  
// match_test_num - 9 for cmp
TEST_CASE( "unitest_video_enc_sample_functions_alignment_enc", "[sample][enc][functions_alignment][unitest]" ) {
    params_video_enc_t params_video_enc = {0};
    std::string input_file;
    std::string json_params, output_file;
    params_video_enc.out_name_check = GENERATE(1);
    params_video_enc.codec = GENERATE(0, 1, 2);
    params_video_enc.naltype_check_num = GENERATE(0); // check naltypes of previous frames, av1 not support
    params_video_enc.core_mode = GENERATE(0);
    params_video_enc.vframes = GENERATE(0);
    params_video_enc.stride = GENERATE(0);
    params_video_enc.device = GENERATE((char *)"/dev/vastai_video0");
    params_video_enc.loop = GENERATE(0);
    params_video_enc.store = GENERATE(1);
    params_video_enc.buffer_count = GENERATE(0);
    params_video_enc.idr_indices = GENERATE((char *)":");
    params_video_enc.firstFrame_time_test = GENERATE(0);
    SECTION( "video_enc_base()" ) {
        std::tie(input_file, params_video_enc.width, params_video_enc.height, params_video_enc.pixel_fmt) = GENERATE(
        table<std::string, int, int, char*>({
            std::make_tuple("/opt/funcs_align_src/1080p_nv12_100.yuv", 1920, 1080, (char *)"nv12")

        }));
        std::tie(json_params, output_file) = GENERATE(
        table<std::string, std::string>({
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/video_enc.json", std::string(UT_RES_PATH) + funcs_align_out + "1080p_origin.hxxx"), // default match_test_num - 3
            
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/vbv_MaxRate_800_BufSize_200.json", std::string(UT_RES_PATH) + funcs_align_out + "vbv_MaxRate_800_BufSize_200.hxxx"), // vbv
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/vbv_MaxRate_800_BufSize_800.json", std::string(UT_RES_PATH) + funcs_align_out + "vbv_MaxRate_800_BufSize_800.hxxx"),
            // std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/vbv_bitRate_2000k_MaxRate_2000_BufSize_3000.json", std::string(UT_RES_PATH) + funcs_align_out + "vbv_bitRate_3000k_MaxRate_2000_BufSize_3000.hxxx"), // check next SECTION
            // std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/vbv_bitRate_2000k_MaxRate_4000_BufSize_3000.json", std::string(UT_RES_PATH) + funcs_align_out + "vbv_bitRate_3000k_MaxRate_4000_BufSize_3000.hxxx"), // check next SECTION
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/roi-qpmap_roiType_2_roiInt_1_roiMapDQpBU_0.json", std::string(UT_RES_PATH) + funcs_align_out + "roi_qpmap_roiType_2_roiInt_1_roiMapDQpBU_0.hxxx"), // roi:qpmap
            // std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/roi-qpmap_roiType_2_roiInt_1_roiMapDQpBU_1.json", std::string(UT_RES_PATH) + funcs_align_out + "roi_qpmap_roiType_2_roiInt_1_roiMapDQpBU_1.hxxx"), // av1 not support!!!
            // std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/roi-qpmap_roiType_2_roiInt_1_roiMapDQpBU_2.json", std::string(UT_RES_PATH) + funcs_align_out + "roi_qpmap_roiType_2_roiInt_1_roiMapDQpBU_2.hxxx"), // av1 not support!!!
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/lookahead_0.json", std::string(UT_RES_PATH) + funcs_align_out + "lookahead_0.hxxx"), // 1_2pass
            // std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/lookahead_20.json", std::string(UT_RES_PATH) + funcs_align_out + "lookahead_20.hxxx"), 
            // std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/lookahead_40.json", std::string(UT_RES_PATH) + funcs_align_out + "lookahead_40.hxxx"),
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/gopSize_0.json", std::string(UT_RES_PATH) + funcs_align_out + "gopSize_0.hxxx"), // gopSize
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/gopSize_1.json", std::string(UT_RES_PATH) + funcs_align_out + "gopSize_1.hxxx"),
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/gopSize_8.json", std::string(UT_RES_PATH) + funcs_align_out + "gopSize_8.hxxx"), 
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/crf_0.json", std::string(UT_RES_PATH) + funcs_align_out + "crf_0.hxxx"), // crf
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/crf_1.json", std::string(UT_RES_PATH) + funcs_align_out + "crf_1.hxxx"),
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/crf_51.json", std::string(UT_RES_PATH) + funcs_align_out + "crf_51.hxxx"),
            
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/bBPyramid_0.json", std::string(UT_RES_PATH) + funcs_align_out + "bBPyramid_0.hxxx"), //bBPyramid
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/bBPyramid_1.json", std::string(UT_RES_PATH) + funcs_align_out + "bBPyramid_1.hxxx"),
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/forceIDR_30.json", std::string(UT_RES_PATH) + funcs_align_out + "forceIDR_30.hxxx"), // forceIDR
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/forceIDRInt_keyInt_12_forceIDR_30.json", std::string(UT_RES_PATH) + funcs_align_out + "forceIDRInt_keyInt_12_forceIDR_30.hxxx"),
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/forceIDRInt_keyInt_30_forceIDR_12.json", std::string(UT_RES_PATH) + funcs_align_out + "forceIDRInt_keyInt_30_forceIDR_12.hxxx"), 
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/picSkip_0.json", std::string(UT_RES_PATH) + funcs_align_out + "picSkip_0.hxxx"), // pskip
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/picSkip_1.json", std::string(UT_RES_PATH) + funcs_align_out + "picSkip_1.hxxx"),
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/llRc_0.json", std::string(UT_RES_PATH) + funcs_align_out + "llRc_0.hxxx"), // llRc
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/llRc_1.json", std::string(UT_RES_PATH) + funcs_align_out + "llRc_1.hxxx"),
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/llRc_5.json", std::string(UT_RES_PATH) + funcs_align_out + "llRc_5.hxxx"),
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/gdr_gop_1_keyInt_10_gdr_0.json", std::string(UT_RES_PATH) + funcs_align_out + "gdr_gop_1_keyInt_10_gdr_0.hxxx"), // GDR
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/gdr_gop_1_keyInt_10_gdr_2.json", std::string(UT_RES_PATH) + funcs_align_out + "gdr_gop_1_keyInt_10_gdr_2.hxxx"),
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/gdr_gop_1_keyInt_10_gdr_8.json", std::string(UT_RES_PATH) + funcs_align_out + "gdr_gop_1_keyInt_10_gdr_8.hxxx"),
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/P2B_gopSize_1_P2B_0.json", std::string(UT_RES_PATH) + funcs_align_out + "P2B_gopSize_1_P2B_0.hxxx"), // P2B
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/P2B_gopSize_1_P2B_1.json", std::string(UT_RES_PATH) + funcs_align_out + "P2B_gopSize_1_P2B_1.hxxx"),
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/extSEIInt_0.json", std::string(UT_RES_PATH) + funcs_align_out + "extSEIInt_0.hxxx"), // extSEIInt
            // std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/extSEIInt_1.json", std::string(UT_RES_PATH) + funcs_align_out + "extSEIInt_1.hxxx"), //                          av1 not support!!!
            // std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/extSEIInt_8.json", std::string(UT_RES_PATH) + funcs_align_out + "extSEIInt_8.hxxx"), //                          av1 not support!!!
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/maxFrameSize_llrc_2_maxFrameSizeM_0.json", std::string(UT_RES_PATH) + funcs_align_out + "maxFrameSize_llrc_2_maxFrameSizeM_0.hxxx"), // maxFrameSizeMultiple
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/maxFrameSize_llrc_2_maxFrameSizeM_1.json", std::string(UT_RES_PATH) + funcs_align_out + "maxFrameSize_llrc_2_maxFrameSizeM_1.hxxx"),
            
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/svcTLayers_3_svcExtractMaxTLayer_2.json", std::string(UT_RES_PATH) + funcs_align_out + "svcTLayers_3_svcExtractMaxTLayer_2.hxxx"), // svc-t
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/svcTLayers_3_svcExtractMaxTLayer_1.json", std::string(UT_RES_PATH) + funcs_align_out + "svcTLayers_3_svcExtractMaxTLayer_1.hxxx"),
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/svcTLayers_3_svcExtractMaxTLayer_0.json", std::string(UT_RES_PATH) + funcs_align_out + "svcTLayers_3_svcExtractMaxTLayer_0.hxxx"),
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/sliceSize_0.json", std::string(UT_RES_PATH) + funcs_align_out + "sliceSize_0.hxxx"), // sliceSize
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/sliceSize_8.json", std::string(UT_RES_PATH) + funcs_align_out + "sliceSize_8.hxxx"), // sliceSize 8   av1 not support!!!
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/ltr_gop_1_ltrInterval_0_ltrQpDelta_0_ltrRefGap_0.json", std::string(UT_RES_PATH) + funcs_align_out + "ltr_gop_1_ltrInterval_0_ltrQpDelta_0_ltrRefGap_0.hxxx"), // LongTerm
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/ltr_gop_1_ltrInterval_10_ltrQpDelta_0_ltrRefGap_3.json", std::string(UT_RES_PATH) + funcs_align_out + "ltr_gop_1_ltrInterval_10_ltrQpDelta_0_ltrRefGap_3.hxxx"), 
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/rotation_0.json", std::string(UT_RES_PATH) + funcs_align_out + "rotation_0.hxxx"), // rotation
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/rotation_1.json", std::string(UT_RES_PATH) + funcs_align_out + "rotation_1.hxxx"),
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/coreID_0.json", std::string(UT_RES_PATH) + funcs_align_out + "coreID_0.hxxx"), // coreID
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/coreID_1.json", std::string(UT_RES_PATH) + funcs_align_out + "coreID_1.hxxx"),
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/smartEnc_0.json", std::string(UT_RES_PATH) + funcs_align_out + "smartEnc_0.hxxx"), // smartEnc
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/smartEnc_1.json", std::string(UT_RES_PATH) + funcs_align_out + "smartEnc_1.hxxx"),
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/enableSpsCropInfo_0.json", std::string(UT_RES_PATH) + funcs_align_out + "enableSpsCropInfo_0.hxxx"), // enableSpsCropInfo
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/enableSpsCropInfo_1_cropRect_720p.json", std::string(UT_RES_PATH) + funcs_align_out + "enableSpsCropInfo_1_cropRect_720p.hxxx") // SpsCrop 1, av1 not support!!!
        }));

        params_video_enc.input_file = (char *)input_file.c_str();
        params_video_enc.output_file = (char *)output_file.c_str();
        params_video_enc.json_params = (char *)json_params.c_str();
        if (params_video_enc.codec != 2 || (output_file != std::string(UT_RES_PATH) + funcs_align_out + "sliceSize_8.hxxx" && output_file != std::string(UT_RES_PATH) + funcs_align_out + "enableSpsCropInfo_1_cropRect_720p.hxxx"))
            REQUIRE( video_enc(&params_video_enc) == 0 );
    }

    SECTION( "video_enc_much_frames()" ) {
        std::tie(input_file, params_video_enc.width, params_video_enc.height, params_video_enc.pixel_fmt) = GENERATE(
        table<std::string, int, int, char*>({
            std::make_tuple("/opt/funcs_align_src/1080p_nv12_320.yuv", 1920, 1080, (char *)"nv12")

        }));
        std::tie(json_params, output_file) = GENERATE(
        table<std::string, std::string>({
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/video_enc.json", std::string(UT_RES_PATH) + funcs_align_out + "1080p_origin_300.hxxx"), // default   match_test_num - 3

            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/keyInt_0.json", std::string(UT_RES_PATH) + funcs_align_out + "keyInt_0.hxxx"), // keyInt
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/keyInt_10.json", std::string(UT_RES_PATH) + funcs_align_out + "keyInt_10.hxxx"),
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/dynamic_bit_0.json", std::string(UT_RES_PATH) + funcs_align_out + "dynamic_bit_0.hxxx"), // dynamic bitrate
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/dynamic_bit_1.json", std::string(UT_RES_PATH) + funcs_align_out + "dynamic_bit_1.hxxx"),
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/dynamicFPS_0.json", std::string(UT_RES_PATH) + funcs_align_out + "dynamicFPS_0.hxxx"), // dynamic FPS
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/dynamicFPS_1.json", std::string(UT_RES_PATH) + funcs_align_out + "dynamicFPS_1.hxxx"),
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/dynamicCrf_0.json", std::string(UT_RES_PATH) + funcs_align_out + "dynamicCrf_0.hxxx"), // enableDynamicCrf
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/dynamicCrf_1.json", std::string(UT_RES_PATH) + funcs_align_out + "dynamicCrf_1.hxxx"),
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/enableDynamicKeyInt_0.json", std::string(UT_RES_PATH) + funcs_align_out + "enableDynamicKeyInt_0.hxxx"), // enableDynamicKeyInt_0
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/enableDynamicKeyInt_1.json", std::string(UT_RES_PATH) + funcs_align_out + "enableDynamicKeyInt_1.hxxx"),
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/disableMMCO_0.json", std::string(UT_RES_PATH) + funcs_align_out + "disableMMCO_0.hxxx"), // enableDynamicKeyInt_0
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/disableMMCO_1.json", std::string(UT_RES_PATH) + funcs_align_out + "disableMMCO_1.hxxx")

        }));

        params_video_enc.input_file = (char *)input_file.c_str();
        params_video_enc.output_file = (char *)output_file.c_str();
        params_video_enc.json_params = (char *)json_params.c_str();
        if (params_video_enc.codec == 0 || (output_file != std::string(UT_RES_PATH) + funcs_align_out + "disableMMCO_0.hxxx" && output_file != std::string(UT_RES_PATH) + funcs_align_out + "disableMMCO_1.hxxx"))
            REQUIRE( video_enc(&params_video_enc) == 0 );
    }

    SECTION( "video_enc_vbv_MaxRate()" ) {
        std::tie(input_file, params_video_enc.width, params_video_enc.height, params_video_enc.pixel_fmt) = GENERATE(
        table<std::string, int, int, char*>({
            std::make_tuple("/opt/funcs_align_src/Park_1920x1080_30fps_loop_8M.yuv", 1920, 1080, (char *)"nv12")    // match_test_num - 3

        }));
        std::tie(json_params, output_file) = GENERATE(
        table<std::string, std::string>({
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/vbv_bitRate_2000k_MaxRate_2000_BufSize_3000.json", std::string(UT_RES_PATH) + funcs_align_out + "vbv_bitRate_2000k_MaxRate_2000_BufSize_3000.hxxx"), 
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/vbv_bitRate_2000k_MaxRate_4000_BufSize_3000.json", std::string(UT_RES_PATH) + funcs_align_out + "vbv_bitRate_2000k_MaxRate_4000_BufSize_3000.hxxx"),

        }));
        params_video_enc.input_file = (char *)input_file.c_str();
        params_video_enc.output_file = (char *)output_file.c_str();
        params_video_enc.json_params = (char *)json_params.c_str();
        REQUIRE( video_enc(&params_video_enc) == 0 );
    }

    SECTION( "video_enc_rgba()" ) {
        std::tie(input_file, params_video_enc.width, params_video_enc.height, params_video_enc.pixel_fmt) = GENERATE(
        table<std::string, int, int, char*>({
            std::make_tuple("/opt/funcs_align_src/1080p.rgb", 1920, 1080, (char *)"rgba")

        }));
        std::tie(json_params, output_file) = GENERATE(
        table<std::string, std::string>({
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/video_enc.json", std::string(UT_RES_PATH) + funcs_align_out + "yuv2rgb_1080p.hxxx"),
        }));

        params_video_enc.input_file = (char *)input_file.c_str();
        params_video_enc.output_file = (char *)output_file.c_str();
        params_video_enc.json_params = (char *)json_params.c_str();
        REQUIRE( video_enc(&params_video_enc) == 0 );
    }
}
// match_test_num - 3 for cmp
TEST_CASE( "unitest_video_enc_sample_functions_alignment_enc_supply", "[sample][enc][functions_alignment][unitest]" ) {
    params_video_enc_t params_video_enc = {0};
    std::string input_file;
    std::string json_params, output_file;
    params_video_enc.out_name_check = GENERATE(1);
    params_video_enc.codec = GENERATE(0, 1, 2);
    params_video_enc.naltype_check_num = GENERATE(0); // check naltypes of previous frames, av1 not support
    params_video_enc.core_mode = GENERATE(0);
    params_video_enc.vframes = GENERATE(0);
    params_video_enc.stride = GENERATE(0);
    params_video_enc.device = GENERATE((char *)"/dev/vastai_video0");
    params_video_enc.loop = GENERATE(0);
    params_video_enc.store = GENERATE(1);
    params_video_enc.buffer_count = GENERATE(0);
    params_video_enc.idr_indices = GENERATE((char *)":");
    params_video_enc.firstFrame_time_test = GENERATE(0);

    SECTION( "video_enc_base()" ) {
        std::tie(input_file, params_video_enc.width, params_video_enc.height, params_video_enc.pixel_fmt) = GENERATE(
        table<std::string, int, int, char*>({
            std::make_tuple("/opt/funcs_align_src/1080p_nv12_100.yuv", 1920, 1080, (char *)"nv12")

        }));
        std::tie(json_params, output_file) = GENERATE(
        table<std::string, std::string>({
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/lookaheadDepth_4_100.json", std::string(UT_RES_PATH) + funcs_align_out + "lookaheadDepth_4_100.hxxx"),           // match_test_num - 3
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/aqMode_0_lookaheadDepth_4.json", std::string(UT_RES_PATH) + funcs_align_out + "aqMode_0_lookaheadDepth_4.hxxx"),
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/aqMode_1_lookaheadDepth_4.json", std::string(UT_RES_PATH) + funcs_align_out + "aqMode_1_lookaheadDepth_4.hxxx"),
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/enableRdoQuant_0.json", std::string(UT_RES_PATH) + funcs_align_out + "enableRdoQuant_0.hxxx"),
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/enableRdoQuant_1.json", std::string(UT_RES_PATH) + funcs_align_out + "enableRdoQuant_1.hxxx"),
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/inLoopDSRatio_0_lookaheadDepth_4.json", std::string(UT_RES_PATH) + funcs_align_out + "inLoopDSRatio_0_lookaheadDepth_4.hxxx"),
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/inLoopDSRatio_1_lookaheadDepth_4.json", std::string(UT_RES_PATH) + funcs_align_out + "inLoopDSRatio_1_lookaheadDepth_4.hxxx"),
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/psyFactor_0.json", std::string(UT_RES_PATH) + funcs_align_out + "psyFactor_0.hxxx"),
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/psyFactor_1.json", std::string(UT_RES_PATH) + funcs_align_out + "psyFactor_1.hxxx"),
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/qCompress_0.6_lookaheadDepth_4.json", std::string(UT_RES_PATH) + funcs_align_out + "qCompress_0.6_lookaheadDepth_4.hxxx"),
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/qCompress_1_lookaheadDepth_4.json", std::string(UT_RES_PATH) + funcs_align_out + "qCompress_1_lookaheadDepth_4.hxxx"),
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/rdoLevel_1.json", std::string(UT_RES_PATH) + funcs_align_out + "rdoLevel_1.hxxx"),
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/rdoLevel_3.json", std::string(UT_RES_PATH) + funcs_align_out + "rdoLevel_3.hxxx"),
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/rcMode_1.json", std::string(UT_RES_PATH) + funcs_align_out + "rcMode_0.hxxx"),
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/rcMode_3.json", std::string(UT_RES_PATH) + funcs_align_out + "rcMode_3.hxxx"),
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/enableOutputCuInfo_0.json", std::string(UT_RES_PATH) + funcs_align_out + "enableOutputCuInfo_0.hxxx"),
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/enableOutputCuInfo_1.json", std::string(UT_RES_PATH) + funcs_align_out + "enableOutputCuInfo_1.hxxx"),
        }));
        
        params_video_enc.input_file = (char *)input_file.c_str();
        params_video_enc.output_file = (char *)output_file.c_str();
        params_video_enc.json_params = (char *)json_params.c_str();
        if (params_video_enc.codec != 2 || (output_file != std::string(UT_RES_PATH) + funcs_align_out + "enableRdoQuant_0.hxxx" && output_file != std::string(UT_RES_PATH) + funcs_align_out + "enableRdoQuant_1.hxxx")) // RDO Quant is not support AV1
            if (params_video_enc.codec != 0 || (output_file != std::string(UT_RES_PATH) + funcs_align_out + "rdoLevel_1.hxxx" && output_file != std::string(UT_RES_PATH) + funcs_align_out + "rdoLevel_3.hxxx")) // rdoLevel is not support H264
            REQUIRE( video_enc(&params_video_enc) == 0 );
    }

    SECTION( "video_enc_much_frames()" ) {
        std::tie(input_file, params_video_enc.width, params_video_enc.height, params_video_enc.pixel_fmt) = GENERATE(
        table<std::string, int, int, char*>({
            std::make_tuple("/opt/funcs_align_src/1080p_nv12_320.yuv", 1920, 1080, (char *)"nv12")

        }));
        std::tie(json_params, output_file) = GENERATE(
        table<std::string, std::string>({
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/bitRateBalanceLevel_0.json", std::string(UT_RES_PATH) + funcs_align_out + "bitRateBalanceLevel_0.hxxx"),
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/bitRateBalanceLevel_1.json", std::string(UT_RES_PATH) + funcs_align_out + "bitRateBalanceLevel_1.hxxx")

        }));

        params_video_enc.input_file = (char *)input_file.c_str();
        params_video_enc.output_file = (char *)output_file.c_str();
        params_video_enc.json_params = (char *)json_params.c_str();
        if (params_video_enc.codec != 0 || (output_file != std::string(UT_RES_PATH) + funcs_align_out + "bitRateBalanceLevel_0.hxxx" && output_file != std::string(UT_RES_PATH) + funcs_align_out + "bitRateBalanceLevel_1.hxxx"))
            REQUIRE( video_enc(&params_video_enc) == 0 );
    }
    
    SECTION( "video_enc_vbv_Park()" ) {
        std::tie(input_file, params_video_enc.width, params_video_enc.height, params_video_enc.pixel_fmt) = GENERATE(
        table<std::string, int, int, char*>({
            std::make_tuple("/opt/funcs_align_src/Park_1920x1080_30fps_loop_8M.yuv", 1920, 1080, (char *)"nv12")

        }));
        std::tie(json_params, output_file) = GENERATE(
        table<std::string, std::string>({
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/bitRateBalanceLevel_0.json", std::string(UT_RES_PATH) + funcs_align_out + "bitRateBalanceLevel_0.hxxx"), 
            std::make_tuple(std::string(UT_RES_PATH) + "json/enc/funcs_alignment/bitRateBalanceLevel_1.json", std::string(UT_RES_PATH) + funcs_align_out + "bitRateBalanceLevel_1.hxxx"),

        }));

        params_video_enc.input_file = (char *)input_file.c_str();
        params_video_enc.output_file = (char *)output_file.c_str();
        params_video_enc.json_params = (char *)json_params.c_str();
        if (params_video_enc.codec == 0 || (output_file != std::string(UT_RES_PATH) + funcs_align_out + "bitRateBalanceLevel_0.hxxx" && output_file != std::string(UT_RES_PATH) + funcs_align_out + "bitRateBalanceLevel_1.hxxx"))
            REQUIRE( video_enc(&params_video_enc) == 0 );
    }
}

TEST_CASE( "unitest_video_enc_sample_functions_alignment_enc_bitmatch", "[sample][enc][functions_alignment][unitest]" ) {

    SECTION( "video_same()" ) { 
    auto file_cmp = GENERATE(
        // std::string(UT_RES_PATH) + funcs_align_out + "1080p_origin.", // default
        std::string(UT_RES_PATH) + funcs_align_out + "lookahead_0.", // 1_2pass
        // std::string(UT_RES_PATH) + funcs_align_out + "lookahead_20.", 
        // std::string(UT_RES_PATH) + funcs_align_out + "lookahead_40.",
        std::string(UT_RES_PATH) + funcs_align_out + "gopSize_1.",
        
        std::string(UT_RES_PATH) + funcs_align_out + "bBPyramid_1.",
        std::string(UT_RES_PATH) + funcs_align_out + "picSkip_0.", // pskip
        std::string(UT_RES_PATH) + funcs_align_out + "llRc_0.", // llRc
        // std::string(UT_RES_PATH) + funcs_align_out + "P2B_gopSize_1_P2B_0.", // P2B
        // std::string(UT_RES_PATH) + funcs_align_out + "P2B_gopSize_1_P2B_1.",
        std::string(UT_RES_PATH) + funcs_align_out + "extSEIInt_0.", // extSEIInt
        // std::string(UT_RES_PATH) + funcs_align_out + "svcTLayers_3_svcExtractMaxTLayer_2." // svc-t
        std::string(UT_RES_PATH) + funcs_align_out + "sliceSize_0.", // sliceSize
        std::string(UT_RES_PATH) + funcs_align_out + "coreID_0.", // coreID
        std::string(UT_RES_PATH) + funcs_align_out + "coreID_1.",
        std::string(UT_RES_PATH) + funcs_align_out + "rotation_0.", // rotation
        std::string(UT_RES_PATH) + funcs_align_out + "smartEnc_0.", // smartEnc
        std::string(UT_RES_PATH) + funcs_align_out + "enableSpsCropInfo_0.", // enableSpsCropInfo
        std::string(UT_RES_PATH) + funcs_align_out + "enableRdoQuant_0.",
        std::string(UT_RES_PATH) + funcs_align_out + "psyFactor_0.",
        std::string(UT_RES_PATH) + funcs_align_out + "rdoLevel_1.",
        std::string(UT_RES_PATH) + funcs_align_out + "rcMode_0.",
        std::string(UT_RES_PATH) + funcs_align_out + "enableOutputCuInfo_0.",
        std::string(UT_RES_PATH) + funcs_align_out + "enableOutputCuInfo_1."
        );
        
        if (file_cmp != std::string(UT_RES_PATH) + funcs_align_out + "enableRdoQuant_0.")
            REQUIRE( compareFiles(file_cmp + "ivf", std::string(UT_RES_PATH) + funcs_align_out + "1080p_origin.ivf") == 0);
        REQUIRE( compareFiles(file_cmp + "hevc", std::string(UT_RES_PATH) + funcs_align_out + "1080p_origin.hevc") == 0);
        if (file_cmp != std::string(UT_RES_PATH) + funcs_align_out + "rdoLevel_1.")
            REQUIRE( compareFiles(file_cmp + "h264", std::string(UT_RES_PATH) + funcs_align_out + "1080p_origin.h264") == 0);
    }

    SECTION( "video_same_lookaheadDepth_4()" ) { 
    auto file_cmp = GENERATE(
        std::string(UT_RES_PATH) + funcs_align_out + "aqMode_0_lookaheadDepth_4.",
        std::string(UT_RES_PATH) + funcs_align_out + "inLoopDSRatio_1_lookaheadDepth_4.",
        std::string(UT_RES_PATH) + funcs_align_out + "qCompress_0.6_lookaheadDepth_4."
        
        );
        
        REQUIRE( compareFiles(file_cmp + "ivf", std::string(UT_RES_PATH) + funcs_align_out + "lookaheadDepth_4_100.ivf") == 0);
        REQUIRE( compareFiles(file_cmp + "hevc", std::string(UT_RES_PATH) + funcs_align_out + "lookaheadDepth_4_100.hevc") == 0);
        REQUIRE( compareFiles(file_cmp + "h264", std::string(UT_RES_PATH) + funcs_align_out + "lookaheadDepth_4_100.h264") == 0);
    }

    SECTION( "video_same_much_frames()" ) { 
    auto file_cmp = GENERATE(
        std::string(UT_RES_PATH) + funcs_align_out + "dynamic_bit_0.", // dynamic bitrate
        std::string(UT_RES_PATH) + funcs_align_out + "dynamicFPS_0.", // dynamic FPS
        std::string(UT_RES_PATH) + funcs_align_out + "dynamicCrf_0.", // dynamic Crf
        std::string(UT_RES_PATH) + funcs_align_out + "enableDynamicKeyInt_0.", // enableDynamicKeyInt
        std::string(UT_RES_PATH) + funcs_align_out + "disableMMCO_0.", // disableMMCO
        std::string(UT_RES_PATH) + funcs_align_out + "bitRateBalanceLevel_0."
        
        );
        
        if (file_cmp != std::string(UT_RES_PATH) + funcs_align_out + "disableMMCO_0." && file_cmp != std::string(UT_RES_PATH) + funcs_align_out + "disableMMCO_1.") {
            REQUIRE( compareFiles(file_cmp + "ivf", std::string(UT_RES_PATH) + funcs_align_out + "1080p_origin_300.ivf") == 0);
            REQUIRE( compareFiles(file_cmp + "hevc", std::string(UT_RES_PATH) + funcs_align_out + "1080p_origin_300.hevc") == 0);
        }
        if (file_cmp != std::string(UT_RES_PATH) + funcs_align_out + "bitRateBalanceLevel_0.")
            REQUIRE( compareFiles(file_cmp + "h264", std::string(UT_RES_PATH) + funcs_align_out + "1080p_origin_300.h264") == 0);
    }

    SECTION( "video_P2B_same_with_origin()" ) { 
    std::string file_cmp, file_origin;
    std::tie(file_cmp, file_origin) = GENERATE(
        table<std::string, std::string>({
            std::make_tuple(std::string(UT_RES_PATH) + funcs_align_out + "P2B_gopSize_1_P2B_1.ivf", std::string(UT_RES_PATH) + funcs_align_out + "1080p_origin.ivf"), 
            std::make_tuple(std::string(UT_RES_PATH) + funcs_align_out + "P2B_gopSize_1_P2B_1.hevc", std::string(UT_RES_PATH) + funcs_align_out + "1080p_origin.hevc"), 
            std::make_tuple(std::string(UT_RES_PATH) + funcs_align_out + "P2B_gopSize_1_P2B_0.h264", std::string(UT_RES_PATH) + funcs_align_out + "1080p_origin.h264"), 

        }));
        REQUIRE( compareFiles(file_cmp, file_origin) == 0);
    }

    SECTION( "video_sameWithDiff_Park_1920x1080_30fps_loop_8M()" ) { 
        std::string file_cmp, file_origin;
        std::tie(file_cmp, file_origin) = GENERATE(
            table<std::string, std::string>({
                // std::make_tuple(std::string(UT_RES_PATH) + funcs_align_out + "P2B_gopSize_1_P2B_0.ivf", std::string(UT_RES_PATH) + funcs_align_out + "1080p_origin.ivf"),
                // std::make_tuple(std::string(UT_RES_PATH) + funcs_align_out + "P2B_gopSize_1_P2B_0.hevc", std::string(UT_RES_PATH) + funcs_align_out + "1080p_origin.hevc"), 
                std::make_tuple(std::string(UT_RES_PATH) + funcs_align_out + "bitRateBalanceLevel_0.", std::string(UT_RES_PATH) + funcs_align_out + "bitRateBalanceLevel_1.")

            }));
        
        // REQUIRE( compareFiles(file_cmp + "ivf", std::string(UT_RES_PATH) + funcs_align_out + "lookaheadDepth_4_100.ivf") == 1);
        REQUIRE( compareFiles(file_cmp + "h264", file_origin + "h264") == 1);
        REQUIRE( compareFiles(file_cmp + "h264", file_origin + "h264") == 1);
    }

    SECTION( "video_P2B_not_same_with_origin()" ) { 
    std::string file_cmp, file_origin;
    std::tie(file_cmp, file_origin) = GENERATE(
        table<std::string, std::string>({
            std::make_tuple(std::string(UT_RES_PATH) + funcs_align_out + "P2B_gopSize_1_P2B_0.ivf", std::string(UT_RES_PATH) + funcs_align_out + "1080p_origin.ivf"),
            std::make_tuple(std::string(UT_RES_PATH) + funcs_align_out + "P2B_gopSize_1_P2B_0.hevc", std::string(UT_RES_PATH) + funcs_align_out + "1080p_origin.hevc"), 
            std::make_tuple(std::string(UT_RES_PATH) + funcs_align_out + "P2B_gopSize_1_P2B_1.h264", std::string(UT_RES_PATH) + funcs_align_out + "1080p_origin.h264")

        }));
        REQUIRE( compareFiles(file_cmp, file_origin) == 1);
    }

    SECTION( "video_diff()" ) {
    auto file_cmp = GENERATE(
        // std::string(UT_RES_PATH) + funcs_align_out + "1080p_origin.", // default
        std::string(UT_RES_PATH) + funcs_align_out + "vbv_MaxRate_800_BufSize_200.", // vbv
        std::string(UT_RES_PATH) + funcs_align_out + "vbv_MaxRate_800_BufSize_800.",
        std::string(UT_RES_PATH) + funcs_align_out + "vbv_bitRate_2000k_MaxRate_2000_BufSize_3000.", 
        std::string(UT_RES_PATH) + funcs_align_out + "vbv_bitRate_2000k_MaxRate_4000_BufSize_3000.",
        std::string(UT_RES_PATH) + funcs_align_out + "roi_qpmap_roiType_2_roiInt_1_roiMapDQpBU_0.", // roi:qpmap
        // std::string(UT_RES_PATH) + funcs_align_out + "roi_qpmap_roiType_2_roiInt_1_roiMapDQpBU_1.", //                             av1 not support !!!
        // std::string(UT_RES_PATH) + funcs_align_out + "roi_qpmap_roiType_2_roiInt_1_roiMapDQpBU_2.", //                             av1 not support !!!
        // std::string(UT_RES_PATH) + funcs_align_out + "lookahead_0.", // 1_2pass
        // // std::string(UT_RES_PATH) + funcs_align_out + "lookahead_20.", 
        // // std::string(UT_RES_PATH) + funcs_align_out + "lookahead_40."),
        std::string(UT_RES_PATH) + funcs_align_out + "gopSize_0.", // gopSize
        // std::string(UT_RES_PATH) + funcs_align_out + "gopSize_1.",
        std::string(UT_RES_PATH) + funcs_align_out + "gopSize_8.", 
        std::string(UT_RES_PATH) + funcs_align_out + "crf_0.", // crf
        std::string(UT_RES_PATH) + funcs_align_out + "crf_1.",
        std::string(UT_RES_PATH) + funcs_align_out + "crf_51.",
        
        std::string(UT_RES_PATH) + funcs_align_out + "bBPyramid_0.", //bBPyramid
        // std::string(UT_RES_PATH) + funcs_align_out + "bBPyramid_1.",
        std::string(UT_RES_PATH) + funcs_align_out + "forceIDR_30.", // forceIDR
        std::string(UT_RES_PATH) + funcs_align_out + "forceIDRInt_keyInt_12_forceIDR_30.",
        std::string(UT_RES_PATH) + funcs_align_out + "forceIDRInt_keyInt_30_forceIDR_12.", 
        // std::string(UT_RES_PATH) + funcs_align_out + "picSkip_0.", // pskip
        std::string(UT_RES_PATH) + funcs_align_out + "picSkip_1.",
        // std::string(UT_RES_PATH) + funcs_align_out + "llRc_0.", // llRc
        std::string(UT_RES_PATH) + funcs_align_out + "llRc_1.",
        std::string(UT_RES_PATH) + funcs_align_out + "llRc_5.",
        std::string(UT_RES_PATH) + funcs_align_out + "gdr_gop_1_keyInt_10_gdr_0.", // GDR
        std::string(UT_RES_PATH) + funcs_align_out + "gdr_gop_1_keyInt_10_gdr_2.",
        std::string(UT_RES_PATH) + funcs_align_out + "gdr_gop_1_keyInt_10_gdr_8.",
        // std::string(UT_RES_PATH) + funcs_align_out + "P2B_gopSize_1_P2B_0.", // P2B
        // std::string(UT_RES_PATH) + funcs_align_out + "P2B_gopSize_1_P2B_1.",
        // std::string(UT_RES_PATH) + funcs_align_out + "extSEIInt_0.", // extSEIInt
        // std::string(UT_RES_PATH) + funcs_align_out + "extSEIInt_1.", //                                                            av1 not support !!!
        // std::string(UT_RES_PATH) + funcs_align_out + "extSEIInt_8.", //                                                            av1 not support !!!
        std::string(UT_RES_PATH) + funcs_align_out + "maxFrameSize_llrc_2_maxFrameSizeM_0.", // maxFrameSizeMultiple
        std::string(UT_RES_PATH) + funcs_align_out + "maxFrameSize_llrc_2_maxFrameSizeM_1.",
        
        std::string(UT_RES_PATH) + funcs_align_out + "yuv2rgb_1080p.",
        std::string(UT_RES_PATH) + funcs_align_out + "svcTLayers_3_svcExtractMaxTLayer_2.",
        std::string(UT_RES_PATH) + funcs_align_out + "svcTLayers_3_svcExtractMaxTLayer_1.",
        std::string(UT_RES_PATH) + funcs_align_out + "svcTLayers_3_svcExtractMaxTLayer_0.",
        std::string(UT_RES_PATH) + funcs_align_out + "rotation_1.",
        std::string(UT_RES_PATH) + funcs_align_out + "smartEnc_1.",
        std::string(UT_RES_PATH) + funcs_align_out + "sliceSize_8.", // sliceSize 8   av1 not support!!!
        std::string(UT_RES_PATH) + funcs_align_out + "enableSpsCropInfo_1_cropRect_720p.",  // enableSpsCropInfo
        std::string(UT_RES_PATH) + funcs_align_out + "enableRdoQuant_1.",
        std::string(UT_RES_PATH) + funcs_align_out + "psyFactor_1.",
        std::string(UT_RES_PATH) + funcs_align_out + "rdoLevel_3.",
        std::string(UT_RES_PATH) + funcs_align_out + "rcMode_3."
        );

        if (file_cmp != std::string(UT_RES_PATH) + funcs_align_out + "sliceSize_8." && file_cmp != std::string(UT_RES_PATH) + funcs_align_out + "enableSpsCropInfo_1_cropRect_720p." && file_cmp != std::string(UT_RES_PATH) + funcs_align_out + "enableRdoQuant_1.")
            REQUIRE( compareFiles(file_cmp + "ivf",  std::string(UT_RES_PATH) + funcs_align_out + "1080p_origin.ivf")  == 1);
        REQUIRE( compareFiles(file_cmp + "hevc", std::string(UT_RES_PATH) + funcs_align_out + "1080p_origin.hevc") == 1);
        if (file_cmp != std::string(UT_RES_PATH) + funcs_align_out + "rdoLevel_3.")
            REQUIRE( compareFiles(file_cmp + "h264", std::string(UT_RES_PATH) + funcs_align_out + "1080p_origin.h264") == 1);
    }

    SECTION( "video_diff_lookaheadDepth_4()" ) { 
    auto file_cmp = GENERATE(
        std::string(UT_RES_PATH) + funcs_align_out + "aqMode_1_lookaheadDepth_4.",
        std::string(UT_RES_PATH) + funcs_align_out + "inLoopDSRatio_0_lookaheadDepth_4.",
        std::string(UT_RES_PATH) + funcs_align_out + "qCompress_1_lookaheadDepth_4."
        
        );
        
        REQUIRE( compareFiles(file_cmp + "ivf", std::string(UT_RES_PATH) + funcs_align_out + "lookaheadDepth_4_100.ivf") == 1);
        REQUIRE( compareFiles(file_cmp + "hevc", std::string(UT_RES_PATH) + funcs_align_out + "lookaheadDepth_4_100.hevc") == 1);
        REQUIRE( compareFiles(file_cmp + "h264", std::string(UT_RES_PATH) + funcs_align_out + "lookaheadDepth_4_100.h264") == 1);
    }

    SECTION( "video_diff_much_frames()" ) {
    auto file_cmp = GENERATE(
        
        // std::string(UT_RES_PATH) + funcs_align_out + "vbv_bitRate_2000k_MaxRate_2000_BufSize_3000.", 
        // std::string(UT_RES_PATH) + funcs_align_out + "vbv_bitRate_2000k_MaxRate_4000_BufSize_3000.",
        
        std::string(UT_RES_PATH) + funcs_align_out + "keyInt_0.", // keyInt
        std::string(UT_RES_PATH) + funcs_align_out + "keyInt_10.",
        // std::string(UT_RES_PATH) + funcs_align_out + "dynamic_bit_0.", // dynamic bitrate
        std::string(UT_RES_PATH) + funcs_align_out + "dynamic_bit_1.",
        // std::string(UT_RES_PATH) + funcs_align_out + "dynamicFPS_0.", // dynamic FPS
        std::string(UT_RES_PATH) + funcs_align_out + "dynamicFPS_1.",
        std::string(UT_RES_PATH) + funcs_align_out + "dynamicCrf_1.", // dynamic Crf
        std::string(UT_RES_PATH) + funcs_align_out + "enableDynamicKeyInt_1.", // enableDynamicKeyInt
        std::string(UT_RES_PATH) + funcs_align_out + "disableMMCO_1.", // disableMMCO
        std::string(UT_RES_PATH) + funcs_align_out + "bitRateBalanceLevel_1."
        
        );
        if (file_cmp != std::string(UT_RES_PATH) + funcs_align_out + "disableMMCO_0." && file_cmp != std::string(UT_RES_PATH) + funcs_align_out + "disableMMCO_1.") {
            REQUIRE( compareFiles(file_cmp + "ivf",  std::string(UT_RES_PATH) + funcs_align_out + "1080p_origin_300.ivf")  == 1);
            REQUIRE( compareFiles(file_cmp + "hevc", std::string(UT_RES_PATH) + funcs_align_out + "1080p_origin_300.hevc") == 1);
        }
        if (file_cmp != std::string(UT_RES_PATH) + funcs_align_out + "bitRateBalanceLevel_1.")
            REQUIRE( compareFiles(file_cmp + "h264", std::string(UT_RES_PATH) + funcs_align_out + "1080p_origin_300.h264") == 1);
    }

    SECTION( "video_diff_ltr()" ) {
    auto file_cmp = GENERATE(
        std::string(UT_RES_PATH) + funcs_align_out + "ltr_gop_1_ltrInterval_10_ltrQpDelta_0_ltrRefGap_3."

        );
        
        REQUIRE( compareFiles(file_cmp + "ivf",  std::string(UT_RES_PATH) + funcs_align_out + "ltr_gop_1_ltrInterval_0_ltrQpDelta_0_ltrRefGap_0.ivf")  == 1);
        REQUIRE( compareFiles(file_cmp + "hevc", std::string(UT_RES_PATH) + funcs_align_out + "ltr_gop_1_ltrInterval_0_ltrQpDelta_0_ltrRefGap_0.hevc") == 1);
        REQUIRE( compareFiles(file_cmp + "h264", std::string(UT_RES_PATH) + funcs_align_out + "ltr_gop_1_ltrInterval_0_ltrQpDelta_0_ltrRefGap_0.h264") == 1);
    }

}

int multicore_enc_num = 0;
TEST_CASE( "unitest_video_enc_sample_resolutionStandard_enc_multicore", "[sample][enc][resolutionStandard][unitest][multicore]" ) {
    params_video_enc_t params_video_enc = {0};
    std::string input_file;
    int status = 0;
    multicore_enc_num++;
    if(multicore_enc_num <= 6 || multicore_enc_num > 32) {
#if defined  __aarch64__
        printf("host is arm cpu\n");
        status = system("/video-case/lowlevel_SDK/vatools_SV100_arm/vasmi setvideomulticore 1 -d 0 -i 1"); // ,1
#elif defined __x86_64__
        printf("host is x86 cpu\n");
        status = system("/video-case/lowlevel_SDK/vatools_SV100/vasmi setvideomulticore 1 -d 0 -i 1"); // ,1
#endif
        if (status == -1) {
            perror("system");
        } else if (WIFEXITED(status)) {
            printf("Command exited with status %d\n", WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("Command terminated by signal %d\n", WTERMSIG(status));
        }
        REQUIRE( status == 0 );
    }
    std::tie(input_file, params_video_enc.width, params_video_enc.height, params_video_enc.pixel_fmt) = GENERATE(
    table<std::string, int, int, char*>({
        std::make_tuple(std::string(UT_RES_PATH) + "resolutions_multicore/cdzj_multicore_8k.yuv", 7680, 4320, (char *)"nv12"),
        std::make_tuple(std::string(UT_RES_PATH) + "resolutions_multicore/cdzj_multicore_4k.yuv", 3840, 2160, (char *)"nv12"), 
        std::make_tuple(std::string(UT_RES_PATH) + "resolutions_multicore/cdzj_multicore_1080p.yuv", 1920, 1080, (char *)"nv12"),

        std::make_tuple(std::string(UT_RES_PATH) + "resolutions_rgb/7680x4320.rgb", 7680, 4320, (char *)"rgba"),
        std::make_tuple(std::string(UT_RES_PATH) + "resolutions_rgb/3840x2160.rgb", 3840, 2160, (char *)"rgba"), 
        std::make_tuple(std::string(UT_RES_PATH) + "resolutions_rgb/1920x1080.rgb", 1920, 1080, (char *)"rgba"),
        
    }));
    
    std::string output_file;
    std::tie(params_video_enc.codec, output_file) = GENERATE(
    table<int, std::string>({
        std::make_tuple(2, std::string(UT_RES_PATH) + "enc/"),  // note: AV1 dec MULTI_CORE_MODE 8k not support
        std::make_tuple(1, std::string(UT_RES_PATH) + "enc/"), 
        std::make_tuple(0, std::string(UT_RES_PATH) + "enc/")
    }));
    params_video_enc.naltype_check_num = GENERATE(0); // check naltypes of previous frames, av1 not support
    params_video_enc.core_mode = GENERATE(0);
    params_video_enc.vframes = GENERATE(0);
    params_video_enc.stride = GENERATE(0);
    params_video_enc.device = GENERATE((char *)"/dev/vastai_video1"); // , (char *)"/dev/vastai_video3"
    params_video_enc.loop = GENERATE(1);
    params_video_enc.store = GENERATE(1);
    params_video_enc.buffer_count = GENERATE(0);
    params_video_enc.idr_indices = GENERATE((char *)":");
    params_video_enc.firstFrame_time_test = GENERATE(0);
    auto json_params = GENERATE(std::string(UT_RES_PATH) + "json/enc/video_enc.json", std::string(UT_RES_PATH) + "json/enc/video_enc_openGop_1.json");
    
    params_video_enc.input_file = (char *)input_file.c_str();
    params_video_enc.output_file = (char *)output_file.c_str();
    params_video_enc.json_params = (char *)json_params.c_str();
    REQUIRE( video_enc(&params_video_enc) == 0 );
    
    if(multicore_enc_num < 6 || multicore_enc_num >= 32) {
#if defined  __aarch64__
        printf("this is arm cpu\n");
        status = system("/video-case/lowlevel_SDK/vatools_SV100_arm/vasmi setvideomulticore 0 -d 0 -i 1"); // ,1
#elif defined __x86_64__
        printf("this is x86 cpu\n");
        status = system("/video-case/lowlevel_SDK/vatools_SV100/vasmi setvideomulticore 0 -d 0 -i 1"); // ,1
#endif
        if (status == -1) {
            perror("system");
        } else if (WIFEXITED(status)) {
            printf("Command exited with status %d\n", WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("Command terminated by signal %d\n", WTERMSIG(status));
        }
        REQUIRE( status == 0 );
    }
    sleep(0.5);
}

TEST_CASE( "unitest_video_enc_sample_resolutionStandard_enc", "[sample][enc][resolutionStandard][unitest]" ) {
    params_video_enc_t params_video_enc = {0};
    std::string input_file;
    std::tie(input_file, params_video_enc.width, params_video_enc.height, params_video_enc.pixel_fmt) = GENERATE(
        table<std::string, int, int, char*>({
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_yuv/7680x4320.yuv", 7680, 4320, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_yuv/3840x2160.yuv", 3840, 2160, (char *)"yuv420p"), 
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_yuv/1920x1080.yuv", 1920, 1080, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_yuv/1280x960.yuv", 1280, 960, (char *)"yuv420p"), 
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_yuv/1280x720.yuv", 1280, 720, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_yuv/1024x768.yuv", 1024, 768, (char *)"yuv420p"), 
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_yuv/832x480.yuv", 832, 480, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_yuv/704x576.yuv", 704, 576, (char *)"yuv420p"), 
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_yuv/640x480.yuv", 640, 480, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_yuv/416x240.yuv", 416, 240, (char *)"yuv420p"), 
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_yuv/352x288.yuv", 352, 288, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_yuv/320x240.yuv", 320, 240, (char *)"yuv420p"), 
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_yuv/176x144.yuv", 176, 144, (char *)"yuv420p"),

            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_rgb/7680x4320.rgb", 7680, 4320, (char*)"rgba"),
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_rgb/3840x2160.rgb", 3840, 2160, (char*)"rgba"), 
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_rgb/1920x1080.rgb", 1920, 1080, (char*)"rgba"),
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_rgb/1280x960.rgb", 1280, 960, (char*)"rgba"), 
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_rgb/1280x720.rgb", 1280, 720, (char*)"rgba"),
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_rgb/1024x768.rgb", 1024, 768, (char*)"rgba"), 
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_rgb/832x480.rgb", 832, 480, (char*)"rgba"),
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_rgb/704x576.rgb", 704, 576, (char*)"rgba"), 
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_rgb/640x480.rgb", 640, 480, (char*)"rgba"),
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_rgb/416x240.rgb", 416, 240, (char*)"rgba"), 
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_rgb/352x288.rgb", 352, 288, (char*)"rgba"),
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_rgb/320x240.rgb", 320, 240, (char*)"rgba"), 
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_rgb/176x144.rgb", 176, 144, (char*)"rgba"),
        }));
    
    std::string output_file;
    std::tie(params_video_enc.codec, output_file) = GENERATE(
        table<int, std::string>({
            std::make_tuple(2, std::string(UT_RES_PATH) + "enc/"), 
            std::make_tuple(1, std::string(UT_RES_PATH) + "enc/"), 
            std::make_tuple(0, std::string(UT_RES_PATH) + "enc/")
        }));
    params_video_enc.naltype_check_num = GENERATE(0); // check naltypes of previous frames, av1 not support
    params_video_enc.core_mode = GENERATE(0);
    params_video_enc.vframes = GENERATE(0);
    params_video_enc.stride = GENERATE(0);
    params_video_enc.device = GENERATE((char *)"/dev/vastai_video0");
    params_video_enc.loop = GENERATE(1);
    params_video_enc.store = GENERATE(1);
    params_video_enc.buffer_count = GENERATE(0);
    params_video_enc.idr_indices = GENERATE((char *)":");
    params_video_enc.firstFrame_time_test = GENERATE(0);
    auto json_params = GENERATE(std::string(UT_RES_PATH) + "json/enc/video_enc.json");
    
    params_video_enc.input_file = (char *)input_file.c_str();
    params_video_enc.output_file = (char *)output_file.c_str();
    params_video_enc.json_params = (char *)json_params.c_str();
    SECTION( "video_enc()" ) {
        REQUIRE( video_enc(&params_video_enc) == 0 );
    }
}

TEST_CASE( "unitest_video_enc_sample_params_exception_enc", "[sample][enc][params_exception][unitest]" ) {
    params_video_enc_t params_video_enc = {0};
    std::string input_file;
    std::tie(input_file, params_video_enc.width, params_video_enc.height, params_video_enc.pixel_fmt) = GENERATE(
        table<std::string, int, int, char*>({
            std::make_tuple(std::string(UT_RES_PATH) + "cdzj_1080p_nv12.yuv", 1920, 1080, (char*)"nv12"),
        }));
    
    std::string output_file;
    std::string json_params;
    params_video_enc.codec = GENERATE(0, 1, 2); 
    params_video_enc.naltype_check_num = GENERATE(0); // check naltypes of previous frames, av1 not support
    params_video_enc.core_mode = GENERATE(0);
    params_video_enc.vframes = GENERATE(0);
    std::tie(output_file, json_params, params_video_enc.device) = GENERATE(
        table<std::string, std::string, char*>({
            std::make_tuple(std::string(UT_RES_PATH) + "enc/", std::string(UT_RES_PATH) + "json/enc/params_exception/video_enc.json", (char *)"/dev/vastai_videox"), // render err x
            std::make_tuple(std::string(UT_RES_PATH) + "enc/", std::string(UT_RES_PATH) + "json/enc/params_exception/lookahead.json", (char *)"/dev/vastai_video0"), // lookahead err 2
            std::make_tuple(std::string(UT_RES_PATH) + "enc/", std::string(UT_RES_PATH) + "json/enc/params_exception/gop_size.json", (char *)"/dev/vastai_video0"),  //gop_size err 9
            std::make_tuple(std::string(UT_RES_PATH) + "enc/", std::string(UT_RES_PATH) + "json/enc/params_exception/gdrDuration01.json", (char *)"/dev/vastai_video0"), // gdrDuration err 01
            std::make_tuple(std::string(UT_RES_PATH) + "enc/", std::string(UT_RES_PATH) + "json/enc/params_exception/gdrDuration11.json", (char *)"/dev/vastai_video0"), // gdrDuration err 11
            std::make_tuple(std::string(UT_RES_PATH) + "enc/", std::string(UT_RES_PATH) + "json/enc/params_exception/intra.json", (char *)"/dev/vastai_video0"), // intra err //ok
            std::make_tuple(std::string(UT_RES_PATH) + "enc/", std::string(UT_RES_PATH) + "json/enc/params_exception/dynamic01.json", (char *)"/dev/vastai_video0"), // dynamic err miniGopSize=0，lookaheadLength=1
            std::make_tuple(std::string(UT_RES_PATH) + "enc/", std::string(UT_RES_PATH) + "json/enc/params_exception/dynamic11.json", (char *)"/dev/vastai_video0"), // dynamic err miniGopSize=1，lookaheadLength=1

            // The following parameters only report errors and do not cause program exceptions
            // std::make_tuple(std::string(UT_RES_PATH) + "enc/", std::string(UT_RES_PATH) + "json/enc/params_exception/tune.json", "/dev/vastai_video0"), // tune err 8 // [Out of range]=0
            // std::make_tuple(std::string(UT_RES_PATH) + "enc/", std::string(UT_RES_PATH) + "json/enc/params_exception/roi.json", "/dev/vastai_video0"), // roiType err // [Out of range]=0
            // std::make_tuple(std::string(UT_RES_PATH) + "enc/", std::string(UT_RES_PATH) + "json/enc/params_exception/idr.json", "/dev/vastai_video0"), // idr err // idr: uint
            // std::make_tuple(std::string(UT_RES_PATH) + "enc/", std::string(UT_RES_PATH) + "json/enc/params_exception/preset.json", "/dev/vastai_video0"), // qualityMode err //[Out of range]=2（bronze）
            // std::make_tuple(std::string(UT_RES_PATH) + "enc/", std::string(UT_RES_PATH) + "json/enc/params_exception/keyint.json", "/dev/vastai_video0"), // keyint err // WARN had
        }));
    params_video_enc.stride = GENERATE(0);
    params_video_enc.loop = GENERATE(1);
    params_video_enc.store = GENERATE(1);
    params_video_enc.buffer_count = GENERATE(0);
    params_video_enc.idr_indices = GENERATE((char *)":");
    params_video_enc.firstFrame_time_test = GENERATE(0);

    params_video_enc.input_file = (char *)input_file.c_str();
    params_video_enc.output_file = (char *)output_file.c_str();
    params_video_enc.json_params = (char *)json_params.c_str();
    REQUIRE( video_enc(&params_video_enc) == -1 );
}

TEST_CASE( "unitest_video_enc_edge_resolution_sample_exceptionEdgeResolution_enc", "[sample][enc][exception_edge_resolution][unitest]" ) { // 16 cases
    params_video_enc_t params_video_enc = {0};
    std::string input_file;
    std::string output_file = std::string(UT_RES_PATH) + "enc/";
    
    params_video_enc.stride = GENERATE(0);
    params_video_enc.device = GENERATE((char *)"/dev/vastai_video0");
    params_video_enc.loop = GENERATE(1);
    params_video_enc.store = GENERATE(1);
    params_video_enc.buffer_count = GENERATE(0);
    params_video_enc.idr_indices = GENERATE((char *)":");
    params_video_enc.naltype_check_num = GENERATE(0); // check naltypes of previous frames, av1 not support
    params_video_enc.core_mode = GENERATE(0);
    params_video_enc.vframes = GENERATE(0);
    params_video_enc.firstFrame_time_test = GENERATE(0);
    auto json_params = GENERATE(std::string(UT_RES_PATH) + "json/enc/video_enc.json");
    params_video_enc.json_params = (char *)json_params.c_str();
    params_video_enc.output_file = (char *)output_file.c_str();
    //h264
    SECTION( "video_enc_edge_resolution_h264_out" ) {   //h264
        std::tie(input_file, params_video_enc.width, params_video_enc.height, params_video_enc.pixel_fmt) = GENERATE(
        table<std::string, int, int, char *>({ // 64-8192
            std::make_tuple(std::string(UT_RES_PATH) + "edge_resolution_enc/176x8194.yuv", 176, 8194, (char *)"yuv420p"), 
            std::make_tuple(std::string(UT_RES_PATH) + "edge_resolution_enc/176x142.yuv", 176, 142, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "edge_resolution_enc/174x176.yuv", 174, 176, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "edge_resolution_enc/8194x176.yuv", 8194, 176, (char *)"yuv420p"),
        }));
        params_video_enc.codec = GENERATE(0);
        
        params_video_enc.input_file = (char *)input_file.c_str();
        REQUIRE( video_enc(&params_video_enc) == -1 );
    }
    SECTION( "video_enc_edge_resolution_h264_in" ) {   //h264
        std::tie(input_file, params_video_enc.width, params_video_enc.height, params_video_enc.pixel_fmt) = GENERATE(
        table<std::string, int, int, char *>({ // 64-8192
            std::make_tuple(std::string(UT_RES_PATH) + "edge_resolution_enc/176x8192.yuv", 176, 8192, (char *)"yuv420p"), 
            std::make_tuple(std::string(UT_RES_PATH) + "edge_resolution_enc/176x144.yuv", 176, 144, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "edge_resolution_enc/176x176.yuv", 176, 176, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "edge_resolution_enc/8192x176.yuv", 8192, 176, (char *)"yuv420p"),
        }));
        params_video_enc.codec = GENERATE(0);
        
        params_video_enc.input_file = (char *)input_file.c_str();
        REQUIRE( video_enc(&params_video_enc) == 0 );
    }
    
    // hevc
    SECTION( "video_enc_edge_resolution_hevc_out" ) {   //hevc
        std::tie(input_file, params_video_enc.width, params_video_enc.height, params_video_enc.pixel_fmt) = GENERATE(
        table<std::string, int, int, char *>({ // width: 64-8192 height: 66-8192
            std::make_tuple(std::string(UT_RES_PATH) + "edge_resolution_enc/176x142.yuv", 176, 142, (char *)"yuv420p"), //64-8192
            std::make_tuple(std::string(UT_RES_PATH) + "edge_resolution_enc/176x8194.yuv", 176, 8194, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "edge_resolution_enc/174x176.yuv", 174, 176, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "edge_resolution_enc/8194x176.yuv", 8194, 176, (char *)"yuv420p"),
        }));
        params_video_enc.codec = GENERATE(1);
        
        params_video_enc.input_file = (char *)input_file.c_str();
        REQUIRE( video_enc(&params_video_enc) == -1 );
    }
    SECTION( "video_enc_edge_resolution_hevc_in" ) {   //hevc
        std::tie(input_file, params_video_enc.width, params_video_enc.height, params_video_enc.pixel_fmt) = GENERATE(
        table<std::string, int, int, char *>({ // width: 64-8192 height: 66-8192
            std::make_tuple(std::string(UT_RES_PATH) + "edge_resolution_enc/176x144.yuv", 176, 144, (char *)"yuv420p"), //64-8192
            std::make_tuple(std::string(UT_RES_PATH) + "edge_resolution_enc/176x8192.yuv", 176, 8192, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "edge_resolution_enc/176x176.yuv", 176, 176, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "edge_resolution_enc/8192x176.yuv", 8192, 176, (char *)"yuv420p"),
        }));
        params_video_enc.codec = GENERATE(1);
        
        params_video_enc.input_file = (char *)input_file.c_str();
        REQUIRE( video_enc(&params_video_enc) == 0 );
    }

    // av1
    SECTION( "video_enc_edge_resolution_hevc_out" ) {   //hevc
        std::tie(input_file, params_video_enc.width, params_video_enc.height, params_video_enc.pixel_fmt) = GENERATE(
        table<std::string, int, int, char *>({ // width: 176 ~ 4096 height: 144 ~ 8192
            std::make_tuple(std::string(UT_RES_PATH) + "edge_resolution_enc/176x142.yuv", 176, 142, (char *)"yuv420p"), // 176 ~ 8192
            std::make_tuple(std::string(UT_RES_PATH) + "edge_resolution_enc/176x8194.yuv", 176, 8194, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "edge_resolution_enc/174x176.yuv", 174, 176, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "edge_resolution_enc/4098x176.yuv", 4098, 176, (char *)"yuv420p"),
        }));
        params_video_enc.codec = GENERATE(2);
        
        params_video_enc.input_file = (char *)input_file.c_str();
        REQUIRE( video_enc(&params_video_enc) == -1 );
    }
    SECTION( "video_enc_edge_resolution_hevc_in" ) {   //hevc
        std::tie(input_file, params_video_enc.width, params_video_enc.height, params_video_enc.pixel_fmt) = GENERATE(
        table<std::string, int, int, char *>({ // width: 176 ~ 4096 height: 144 ~ 8192
            std::make_tuple(std::string(UT_RES_PATH) + "edge_resolution_enc/176x144.yuv", 176, 144, (char *)"yuv420p"), //64-8192
            std::make_tuple(std::string(UT_RES_PATH) + "edge_resolution_enc/176x8192.yuv", 176, 8192, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "edge_resolution_enc/176x176.yuv", 176, 176, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "edge_resolution_enc/4096x176.yuv", 4096, 176, (char *)"yuv420p"),
        }));
        params_video_enc.codec = GENERATE(2);
        
        params_video_enc.input_file = (char *)input_file.c_str();
        REQUIRE( video_enc(&params_video_enc) == 0 );
    }
}

TEST_CASE( "unitest_video_enc_odd_resolution_sample_exceptionOddResolution_enc", "[sample][enc][exception_odd_resolution][unitest]" ) { // 168 cases
    params_video_enc_t params_video_enc = {0};
    std::string input_file;
    std::tie(input_file, params_video_enc.width, params_video_enc.height, params_video_enc.pixel_fmt) = GENERATE(
        table<std::string, int, int, char*>({
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/7681x4320.yuv", 7681, 4320, (char *)"yuv420p"), //7680x4320
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/7680x4321.yuv", 7680, 4321, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/7680x4319.yuv", 7680, 4319, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/7679x4320.yuv", 7679, 4320, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/7679x4320.yuv", 7679, 0, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/7679x4320.yuv", 0, 4320, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/3841x2160.yuv", 3841, 2160, (char *)"yuv420p"), //3820x2160
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/3840x2161.yuv", 3840, 2161, (char *)"yuv420p"), 
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/3840x2159.yuv", 3840, 2159, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/3839x2160.yuv", 3839, 2160, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/3839x2160.yuv", 3840, 0, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/3839x2160.yuv", 0, 2160, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/1921x1080.yuv", 1921, 1080, (char *)"yuv420p"), //1920x1080
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/1920x1081.yuv", 1920, 1081, (char *)"yuv420p"), 
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/1920x1079.yuv", 1920, 1079, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/1919x1080.yuv", 1919, 1080, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/1920x1079.yuv", 1920, 0, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/1919x1080.yuv", 0, 1080, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/1281x960.yuv", 1281, 960, (char *)"yuv420p"), //1280x960
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/1280x961.yuv", 1280, 961, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/1280x959.yuv", 1280, 959, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/1279x960.yuv", 1279, 960, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/1280x959.yuv", 1280, 0, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/1279x960.yuv", 0, 960, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/1281x720.yuv",  1281, 720, (char *)"yuv420p"), //1280x720
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/1280x721.yuv",  1280, 721, (char *)"yuv420p"), 
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/1280x719.yuv",  1280, 719, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/1279x720.yuv", 1279, 720, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/1280x719.yuv",  1280, 0, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/1279x720.yuv", 0, 720, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/1025x768.yuv", 1025, 768, (char *)"yuv420p"), //1024x768
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/1024x769.yuv", 1024, 769, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/1024x767.yuv", 1024, 767, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/1023x768.yuv", 1023, 768, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/1024x767.yuv", 1024, 0, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/1023x768.yuv", 0, 768, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/833x480.yuv", 833, 480, (char *)"yuv420p"), //832x480
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/832x481.yuv", 832, 481, (char *)"yuv420p"), 
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/832x479.yuv", 832, 479, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/831x480.yuv", 831, 480, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/832x479.yuv", 832, 0, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/831x480.yuv", 0, 480, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/721x576.yuv", 721, 576, (char *)"yuv420p"), //720x576
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/720x577.yuv", 720, 577, (char *)"yuv420p"), 
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/720x575.yuv", 720, 575, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/719x576.yuv", 719, 576, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/720x575.yuv", 720, 0, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/719x576.yuv", 0, 576, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/705x576.yuv", 705, 576, (char *)"yuv420p"), //704x576
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/704x577.yuv", 704, 577, (char *)"yuv420p"), 
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/704x575.yuv", 704, 575, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/703x576.yuv", 703, 576, (char *)"yuv420p"), 
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/704x575.yuv", 704, 0, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/703x576.yuv", 0, 576, (char *)"yuv420p"), 
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/641x480.yuv", 641, 480, (char *)"yuv420p"), //640x480
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/640x481.yuv", 640, 481, (char *)"yuv420p"), 
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/640x479.yuv", 640, 479, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/639x480.yuv", 639, 480, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/640x479.yuv", 640, 0, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/639x480.yuv", 0, 480, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/417x240.yuv", 417, 240, (char *)"yuv420p"), //416x240
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/416x241.yuv", 416, 241, (char *)"yuv420p"), 
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/416x239.yuv", 416, 239, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/415x240.yuv", 415, 240, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/416x239.yuv", 416, 0, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/415x240.yuv", 0, 240, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/353x288.yuv", 353, 288, (char *)"yuv420p"), //352x288
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/352x289.yuv", 352, 289, (char *)"yuv420p"), 
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/352x287.yuv", 352, 287, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/351x288.yuv", 351, 288, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/352x287.yuv", 352, 0, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/351x288.yuv", 0, 288, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/321x240.yuv", 321, 240, (char *)"yuv420p"), //320x240
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/320x241.yuv", 320, 241, (char *)"yuv420p"), 
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/320x239.yuv", 320, 239, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/319x240.yuv", 319, 240, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/320x239.yuv", 320, 0, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/319x240.yuv", 0, 240, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/177x144.yuv", 177, 144, (char *)"yuv420p"), //177x144
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/176x145.yuv", 176, 145, (char *)"yuv420p"), 
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/176x143.yuv", 176, 143, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/175x144.yuv", 175, 144, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/176x143.yuv", 176, 0, (char *)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/175x144.yuv", 0, 144, (char *)"yuv420p"),
        }));
    
    std::string output_file;
    std::tie(params_video_enc.codec, output_file) = GENERATE(
        table<int, std::string>({
            std::make_tuple(2, std::string(UT_RES_PATH) + "enc/"), 
            std::make_tuple(1, std::string(UT_RES_PATH) + "enc/"), 
            std::make_tuple(0, std::string(UT_RES_PATH) + "enc/")
        }));
    params_video_enc.naltype_check_num = GENERATE(0); // check naltypes of previous frames, av1 not support
    params_video_enc.core_mode = GENERATE(0);
    params_video_enc.vframes = GENERATE(0);
    params_video_enc.stride = GENERATE(0);
    params_video_enc.device = GENERATE((char *)"/dev/vastai_video0");
    params_video_enc.loop = GENERATE(1);
    params_video_enc.store = GENERATE(1);
    params_video_enc.buffer_count = GENERATE(0);
    params_video_enc.idr_indices = GENERATE((char *)":");
    params_video_enc.firstFrame_time_test = GENERATE(0);
    auto json_params = GENERATE(std::string(UT_RES_PATH) + "json/enc/video_enc.json");
    
    params_video_enc.json_params = (char *)json_params.c_str();
    SECTION( "video_enc_odd_resolution" ) {
        params_video_enc.input_file = (char *)input_file.c_str();
        params_video_enc.output_file = (char *)output_file.c_str();
        REQUIRE( video_enc(&params_video_enc) == -1 );
    }
}

// for ltr case
char *params[110];
int params_num = 0, out_num = 0;

TEST_CASE( "unitest_video_enc_sample_LTR_enc", "[sample][enc][LTR][unitest]" ) {
    params_video_enc_t params_video_enc = {0};
    int bitDepth;
    std::string input_file;
    std::string output_file;

    auto ltrInsertTest = GENERATE(0, 1);
    auto ltrInterval = GENERATE(range(0, 200, 60));
    auto ltrQpDelta = GENERATE(-20, 20); //signed int
    auto ltrRefGap = GENERATE(range(0, 20, 12));

    params_video_enc.out_name_check = GENERATE(1);
    params_video_enc.codec = GENERATE(0, 1, 2);//, 1, 2
    params_video_enc.naltype_check_num = GENERATE(0); // check naltypes of previous frames, av1 not support
    params_video_enc.core_mode = GENERATE(0);
    params_video_enc.vframes = GENERATE(0);
    params_video_enc.stride = GENERATE(0);
    params_video_enc.device = GENERATE((char *)"/dev/vastai_video0");
    params_video_enc.loop = GENERATE(0);
    params_video_enc.store = GENERATE(1);
    params_video_enc.buffer_count = GENERATE(0);
    params_video_enc.idr_indices = GENERATE((char *)":");   
    params_video_enc.firstFrame_time_test = GENERATE(0);
    params_video_enc.json_params = GENERATE((char *)NULL);
    SECTION( "video_enc_base()" ) {
        std::tie(input_file, params_video_enc.width, params_video_enc.height, params_video_enc.pixel_fmt, bitDepth, output_file) = GENERATE(
        table<std::string, int, int, char*, int, std::string>({
            std::make_tuple("/opt/funcs_align_src/1080p_nv12_100.yuv", 1920, 1080, (char*)"nv12", 8, std::string(UT_RES_PATH) + "ltr_out/1080p_nv12_" + std::to_string(out_num++) + ".hxxx"),
            // std::make_tuple(std::string(UT_RES_PATH) + "resolutions_rgb/1920x1080.rgb", 1920, 1080, "rgba", 10, "ltr_out/1080p_rgb.hxxx"),
            // std::make_tuple(std::string(UT_RES_PATH) + "10bit_stream/1080p_yuv420p10le.yuv", 1920, 1080, "yuv420p_10bit", 10, "ltr_out/1080p_yuv420p10le..hxxx"),

        }));

        auto P2B = GENERATE(0); //VMPP_ENC_DEFAULT_PAR for auto decided by encoder

        params_video_enc.json_params = GENERATE((char *)NULL);
        
        int i = 0;
        params[i++] = (char *)"video_enc";

        char params_arr[5][10];
        sprintf(params_arr[0], "%d", P2B);
        params[i++] = (char *)"--P2B"; params[i++] = params_arr[0];
        sprintf(params_arr[1], "%d", ltrInterval);
        params[i++] = (char *)"--ltrInterval"; params[i++] = params_arr[1];
        sprintf(params_arr[2], "%d", ltrQpDelta);
        params[i++] = (char *)"--ltrQpDelta"; params[i++] = params_arr[2];
        sprintf(params_arr[3], "%d", ltrRefGap);
        params[i++] = (char *)"--ltrRefGap"; params[i++] = params_arr[3];
        sprintf(params_arr[4], "%d", ltrInsertTest);
        params[i++] = (char *)"--ltrInsertTest"; params[i++] = params_arr[4];

        printf("num of all params: %d;\n", params_num = i);
        for (int i=0; i<params_num; i++)
            printf("%s ", params[i]);
        printf("output_file: %s; \n", (char *)output_file.c_str());

        params_video_enc.input_file = (char *)input_file.c_str();
        params_video_enc.output_file = (char *)output_file.c_str();
        CHECK( video_enc(&params_video_enc) == 0 );
    }
}

// for large number of params
TEST_CASE( "params_traversal_video_enc_sample_enc", "[sample][enc][params_traversal]" ) {
    params_video_enc_t params_video_enc = {0};
    int bitDepth;
    std::string input_file;
    std::string output_file;
    params_video_enc.out_name_check = GENERATE(1);
    params_video_enc.codec = GENERATE(0, 1, 2);//, 1, 2
    params_video_enc.naltype_check_num = GENERATE(0); // check naltypes of previous frames, av1 not support
    params_video_enc.core_mode = GENERATE(0);
    params_video_enc.vframes = GENERATE(0);
    params_video_enc.stride = GENERATE(0);
    params_video_enc.device = GENERATE((char *)"/dev/vastai_video0");
    params_video_enc.loop = GENERATE(0);
    params_video_enc.store = GENERATE(1);
    params_video_enc.buffer_count = GENERATE(0);    
    params_video_enc.idr_indices = GENERATE((char *)":");
    params_video_enc.firstFrame_time_test = GENERATE(0);
    SECTION( "video_enc_base()" ) {
        std::tie(input_file, params_video_enc.width, params_video_enc.height, params_video_enc.pixel_fmt, bitDepth, output_file) = GENERATE(
        table<std::string, int, int, char*, int, std::string>({
            std::make_tuple("/opt/funcs_align_src/1080p_nv12_100.yuv", 1920, 1080, (char*)"nv12", 8, std::string(UT_RES_PATH) + "10bit_stream/output/1080p_nv12_" + std::to_string(out_num++) + ".hxxx"),
            // std::make_tuple(std::string(UT_RES_PATH) + "resolutions_rgb/1920x1080.rgb", 1920, 1080, "rgba", 10, "10bit_stream/output/1080p_rgb.hxxx"),
            // std::make_tuple(std::string(UT_RES_PATH) + "10bit_stream/1080p_yuv420p10le.yuv", 1920, 1080, "yuv420p_10bit", 10, "10bit_stream/output/1080p_yuv420p10le..hxxx"),

        }));

        auto profile = GENERATE(range(0, 12));//, [0, 12]
        auto level = GENERATE(range(10, 186));//, [10, 186]
        auto frameRateNum = GENERATE(24, 30, 60);//24, 30, 60   auto frameRateDen = GENERATE(1);
        auto bitDepthLuma = bitDepth;//
        auto bitDepthChroma = bitDepth;//
        auto gopSize = GENERATE(range(0, 16)); // [0, 8] and 16, 0 for adaptive GOP
        auto gdrDuration = GENERATE(0, 10);
        auto lookaheadDepth = GENERATE(0, range(4, 40));// 0 for 1 pass, [4, 40] for 2 pass
        auto qualityMode = GENERATE(range(0, 3));// [0, 3]
        auto tune = GENERATE(range(0, 4));// [0, 4]

        auto keyInt = GENERATE(50, 250);
        auto crf = GENERATE(range(0, 51)); // signed int [0, 51], VMPP_ENC_DEFAULT_PAR for auto decided
        auto cqp = GENERATE(0, 1); // VMPP_ENC_DEFAULT_PAR for auto decided
        auto llRc = GENERATE(range(0, 5)); // [0, 5]
        auto bitRate = GENERATE(2000000);// 
        auto initQp = GENERATE(range(0, 51)); //[0, 51], VMPP_ENC_DEFAULT_PAR for auto decided
        auto vbvBufSize = GENERATE(0, 1000); //default set to be 0
        auto vbvMaxRate = GENERATE(0, 1000); //default set to be 0
        auto intraQpDelta = GENERATE(range(-12, 12)); //signed int [-12, 12], VMPP_ENC_DEFAULT_PAR for auto decided
        auto qpMinI = GENERATE(range(0, 51)); //[0, 51], VMPP_ENC_DEFAULT_PAR for auto decided

        auto qpMaxI = GENERATE(range(0, 51)); //[0, 51], VMPP_ENC_DEFAULT_PAR for auto decided
        auto qpMinPB = GENERATE(range(0, 51)); //[0, 51], VMPP_ENC_DEFAULT_PAR for auto decided
        auto qpMaxPB = GENERATE(range(0, 51)); //[0, 51], VMPP_ENC_DEFAULT_PAR for auto decided
        
        auto aqStrength = GENERATE(0, 1);// float
        auto P2B = GENERATE(0, 1); //VMPP_ENC_DEFAULT_PAR for auto decided by encoder
        auto bBPyramid = GENERATE(0, 1); //0: non-referece B Frames, 1: reference B Frames
        auto maxFrameSizeMultiple = GENERATE(0, 1, 10);//, 1, 12
        auto maxFrameSize = GENERATE(-10, 10); //signed int, only valid in llrc mode
        auto outbufNum = GENERATE(0, 4);

        auto roiType = GENERATE(range(0, 2)); // [0, 2]
        auto roiInt = GENERATE(0, 1, 10);
        char *roiParam = GENERATE((char *)"top=0,left=0,bottom=200,right=200,qpType=0,qpValue=1");//char *
        auto extSEIInt = GENERATE(0, 1, 10);
        auto forceIDRInt = GENERATE(0, 1, 10);
        auto logLevel = GENERATE(range(1, 4)); // [1, 4]
        auto roiMapDeltaQpBlockUnit = GENERATE(range(1, 3)); // [1, 3]
        auto roiMapQpDeltaVersion = GENERATE(0); // reserved
        auto enableDynamicBitrate = GENERATE(0, 1);
        auto enableDynamicFrameRate = GENERATE(0, 1);
        auto maxBFrames = GENERATE(range(0, 7)); // [0, 7]

        auto hrd = GENERATE(0,1); //[0,1] restricts the instantaneous bitrate and total bit amount of every coded picture.
        auto picSkip = GENERATE(0,1);
        // auto colorConversionType = GENERATE(0, 1); // define color conversion type for RGB input convert to yuv
        auto vfr = GENERATE(0, 1);//
        auto svcTLayers = GENERATE(range(1, 4));// [1, 4]
        auto svcExtractMaxTLayer = GENERATE(0, 1, 3);
        auto sliceSize = GENERATE(0, 1, 8);
        auto enableDynamicCrf = GENERATE(0, 1);
        auto psnr = GENERATE(0, 1); //signed int
        auto ltrInterval = GENERATE(0, 10);
        auto ltrQpDelta = GENERATE(0, 10); //signed int

        auto ltrRefGap = GENERATE(0, 3);//, 30, 60
        auto ltrInsertTest = GENERATE(0, 1);//, 30, 60
        auto rotation = GENERATE(0, 1, 2 ,3);//, 30, 60

        params_video_enc.json_params = GENERATE((char *)NULL);
        
        int i = 0;
        params[i++] = (char *)"video_enc";

        params[i++] = (char *)"--profile"; params[i++] = (char *)std::to_string(profile).c_str();
        params[i++] = (char *)"--level"; params[i++] = (char *)std::to_string(level).c_str();
        params[i++] = (char *)"--frameRateNum"; params[i++] = (char *)std::to_string(frameRateNum).c_str();
        params[i++] = (char *)"--bitDepthLuma"; params[i++] = (char *)std::to_string(bitDepthLuma).c_str();
        params[i++] = (char *)"--bitDepthChroma"; params[i++] = (char *)std::to_string(bitDepthChroma).c_str();
        params[i++] = (char *)"--gopSize"; params[i++] = (char *)std::to_string(gopSize).c_str();
        params[i++] = (char *)"--gdrDuration"; params[i++] = (char *)std::to_string(gdrDuration).c_str();
        params[i++] = (char *)"--lookaheadDepth"; params[i++] = (char *)std::to_string(lookaheadDepth).c_str();
        params[i++] = (char *)"--qualityMode"; params[i++] = (char *)std::to_string(qualityMode).c_str();
        params[i++] = (char *)"--tune"; params[i++] = (char *)std::to_string(tune).c_str();

        params[i++] = (char *)"--keyInt"; params[i++] = (char *)std::to_string(keyInt).c_str();
        params[i++] = (char *)"--crf"; params[i++] = (char *)std::to_string(crf).c_str();
        params[i++] = (char *)"--cqp"; params[i++] = (char *)std::to_string(cqp).c_str();
        params[i++] = (char *)"--llRc"; params[i++] = (char *)std::to_string(llRc).c_str();
        params[i++] = (char *)"--bitRate"; params[i++] = (char *)std::to_string(bitRate).c_str();
        params[i++] = (char *)"--initQp"; params[i++] = (char *)std::to_string(initQp).c_str();
        params[i++] = (char *)"--vbvBufSize"; params[i++] = (char *)std::to_string(vbvBufSize).c_str();
        params[i++] = (char *)"--vbvMaxRate"; params[i++] = (char *)std::to_string(vbvMaxRate).c_str();
        params[i++] = (char *)"--intraQpDelta"; params[i++] = (char *)std::to_string(intraQpDelta).c_str();
        params[i++] = (char *)"--qpMinI"; params[i++] = (char *)std::to_string(qpMinI).c_str();

        params[i++] = (char *)"--qpMaxI"; params[i++] = (char *)std::to_string(qpMaxI).c_str();
        params[i++] = (char *)"--qpMinPB"; params[i++] = (char *)std::to_string(qpMinPB).c_str();
        params[i++] = (char *)"--qpMaxPB"; params[i++] = (char *)std::to_string(qpMaxPB).c_str();
        params[i++] = (char *)"--aqStrength"; params[i++] = (char *)std::to_string(aqStrength).c_str();
        params[i++] = (char *)"--P2B"; params[i++] = (char *)std::to_string(P2B).c_str();
        params[i++] = (char *)"--bBPyramid"; params[i++] = (char *)std::to_string(bBPyramid).c_str();
        params[i++] = (char *)"--maxFrameSizeMultiple"; params[i++] = (char *)std::to_string(maxFrameSizeMultiple).c_str();
        params[i++] = (char *)"--maxFrameSize"; params[i++] = (char *)std::to_string(maxFrameSize).c_str();
        params[i++] = (char *)"--outbufNum"; params[i++] = (char *)std::to_string(outbufNum).c_str();
        params[i++] = (char *)"--roiType"; params[i++] = (char *)std::to_string(roiType).c_str();

        params[i++] = (char *)"--roiInt"; params[i++] = (char *)std::to_string(roiInt).c_str();
        params[i++] = (char *)"--roiParam"; params[i++] = roiParam;
        params[i++] = (char *)"--extSEIInt"; params[i++] = (char *)std::to_string(extSEIInt).c_str();
        params[i++] = (char *)"--forceIDRInt"; params[i++] = (char *)std::to_string(forceIDRInt).c_str();
        params[i++] = (char *)"--logLevel"; params[i++] = (char *)std::to_string(logLevel).c_str();
        params[i++] = (char *)"--roiMapDeltaQpBlockUnit"; params[i++] = (char *)std::to_string(roiMapDeltaQpBlockUnit).c_str();
        params[i++] = (char *)"--roiMapQpDeltaVersion"; params[i++] = (char *)std::to_string(roiMapQpDeltaVersion).c_str();
        params[i++] = (char *)"--enableDynamicBitrate"; params[i++] = (char *)std::to_string(enableDynamicBitrate).c_str();
        params[i++] = (char *)"--enableDynamicFrameRate"; params[i++] = (char *)std::to_string(enableDynamicFrameRate).c_str();
        params[i++] = (char *)"--maxBFrames"; params[i++] = (char *)std::to_string(maxBFrames).c_str();

        params[i++] = (char *)"--hrd"; params[i++] = (char *)std::to_string(hrd).c_str();
        params[i++] = (char *)"--picSkip"; params[i++] = (char *)std::to_string(picSkip).c_str();
        // params[i++] = (char *)"--colorConversionType"; params[i++] = (char *)std::to_string(colorConversionType).c_str();
        params[i++] = (char *)"--vfr"; params[i++] = (char *)std::to_string(vfr).c_str();
        params[i++] = (char *)"--svcTLayers"; params[i++] = (char *)std::to_string(svcTLayers).c_str();
        params[i++] = (char *)"--svcExtractMaxTLayer"; params[i++] = (char *)std::to_string(svcExtractMaxTLayer).c_str();
        params[i++] = (char *)"--sliceSize"; params[i++] = (char *)std::to_string(sliceSize).c_str();
        params[i++] = (char *)"--enableDynamicCrf"; params[i++] = (char *)std::to_string(enableDynamicCrf).c_str();
        params[i++] = (char *)"--psnr"; params[i++] = (char *)std::to_string(psnr).c_str();
        params[i++] = (char *)"--ltrInterval"; params[i++] = (char *)std::to_string(ltrInterval).c_str();
        
        params[i++] = (char *)"--ltrQpDelta"; params[i++] = (char *)std::to_string(ltrQpDelta).c_str();
        params[i++] = (char *)"--ltrRefGap"; params[i++] = (char *)std::to_string(ltrRefGap).c_str();
        params[i++] = (char *)"--ltrInsertTest"; params[i++] = (char *)std::to_string(ltrInsertTest).c_str();
        params[i++] = (char *)"--rotation"; params[i++] = (char *)std::to_string(rotation).c_str();

        printf("num of all params: %d;\n", params_num = i);
            
        for (int i=0; i<107; i++)
            printf("%s ", params[i]);
        printf("output_file: %s; \n", (char *)output_file.c_str());

        params_video_enc.input_file = (char *)input_file.c_str();
        params_video_enc.output_file = (char *)output_file.c_str();
        CHECK( video_enc(&params_video_enc) == 0 );
    }
}
