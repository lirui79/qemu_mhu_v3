#include <iostream>
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
#include "video_enc_multi_thread_catch2.c"
#ifdef __cplusplus
}
#endif /* __cplusplus */
#include "catch2/catch.hpp"

// --------------------------------------------------------------------------------------

TEST_CASE( "unitest_video_enc_multi_thread_sample_normal_enc", "[sample][enc][normal][unitest]" ) {
    params_video_enc_mt_t params_video_enc_mt = {0};
    std::string input_file;
    std::string json_params;
    std::tie(input_file, params_video_enc_mt.width, params_video_enc_mt.height, params_video_enc_mt.pixel_fmt, json_params) = GENERATE(
        table<std::string, int, int, char*, std::string>({
            std::make_tuple(std::string(UT_RES_PATH) + "cdzj_1080p_nv12.yuv", 1920, 1080, (char*)"nv12", std::string(UT_RES_PATH) + "json/enc/video_enc_multi_thread.json"),
            std::make_tuple(std::string(UT_RES_PATH) + "cdzj_hevc_1080p_nv12.yuv", 1920, 1080, (char*)"nv12", std::string(UT_RES_PATH) + "json/enc/video_enc_multi_thread.json"), 
        }));
    
    std::string output_file;
    std::tie(params_video_enc_mt.codec, output_file) = GENERATE(
        table<int, std::string>({
            std::make_tuple(2, std::string(UT_RES_PATH) + "enc"), 
            std::make_tuple(1, std::string(UT_RES_PATH) + "enc"), 
            std::make_tuple(0, std::string(UT_RES_PATH) + "enc")
        }));
    params_video_enc_mt.stride = GENERATE(0);
    params_video_enc_mt.device = GENERATE((char *)"/dev/vastai_video0");
    params_video_enc_mt.loop = GENERATE(1);
    params_video_enc_mt.store = GENERATE(1);
    params_video_enc_mt.threads_count = GENERATE(48);
    params_video_enc_mt.core_mode = GENERATE(0, 1);
    params_video_enc_mt.vframes = GENERATE(0);
    params_video_enc_mt.auto_devices = GENERATE(0);

    params_video_enc_mt.input_file = (char*)input_file.c_str();
    params_video_enc_mt.output_file = (char*)output_file.c_str();
    params_video_enc_mt.json_params = (char*)json_params.c_str();
    
    SECTION( "video_enc_multi_thread()" ) {
        REQUIRE( video_enc_multi_thread(&params_video_enc_mt) == 0 );
    }
}

// for ltr case
TEST_CASE( "waitsupport_video_enc_multi_thread_sample_ltr_enc", "[sample][enc][ltr]" ) {
    params_video_enc_mt_t params_video_enc_mt = {0};
    std::string input_file;

    auto ltrInsertTest = GENERATE(0, 1);
    auto ltrInterval = GENERATE(range(0, 200, 60));
    auto ltrQpDelta = GENERATE(-20, 20); //signed int
    auto ltrRefGap = GENERATE(range(0, 20, 12));

    std::tie(input_file, params_video_enc_mt.width, params_video_enc_mt.height, params_video_enc_mt.pixel_fmt) = GENERATE(
        table<std::string, int, int, char*>({
            std::make_tuple(std::string(UT_RES_PATH) + "cdzj_1080p_nv12.yuv", 1920, 1080, (char*)"nv12"),
            std::make_tuple(std::string(UT_RES_PATH) + "cdzj_hevc_1080p_nv12.yuv", 1920, 1080, (char*)"nv12"), 
        }));
    
    std::string output_file;
    std::tie(params_video_enc_mt.codec, output_file) = GENERATE(
        table<int, std::string>({
            std::make_tuple(2, std::string(UT_RES_PATH) + "enc"), 
            std::make_tuple(1, std::string(UT_RES_PATH) + "enc"), 
            std::make_tuple(0, std::string(UT_RES_PATH) + "enc")
        }));
    params_video_enc_mt.stride = GENERATE(0);
    params_video_enc_mt.device = GENERATE((char *)"/dev/vastai_video0");
    params_video_enc_mt.loop = GENERATE(1);
    params_video_enc_mt.store = GENERATE(1);
    params_video_enc_mt.threads_count = GENERATE(48);
    params_video_enc_mt.core_mode = GENERATE(0);
    params_video_enc_mt.vframes = GENERATE(0);
    params_video_enc_mt.auto_devices = GENERATE(0);

    // for LTR
    auto P2B = GENERATE(0); //VMPP_ENC_DEFAULT_PAR for auto decided by encoder
    params_video_enc_mt.json_params = GENERATE((char *)NULL);
    
    int i = 0;
    params[i++] = (char *)"video_enc_multi_thread";

    params[i++] = (char *)"--P2B"; params[i++] = (char *)std::to_string(P2B).c_str();

    params[i++] = (char *)"--ltrInterval"; params[i++] = (char *)std::to_string(ltrInterval).c_str();
    params[i++] = (char *)"--ltrQpDelta"; params[i++] = (char *)std::to_string(ltrQpDelta).c_str();
    params[i++] = (char *)"--ltrRefGap"; params[i++] = (char *)std::to_string(ltrRefGap).c_str();
    params[i++] = (char *)"--ltrInsertTest"; params[i++] = (char *)std::to_string(ltrInsertTest).c_str();

    printf("num of all params: %d;", params_num = i);
    for (int i=0; i<params_num; i++)
        printf("%s ", params[i]);
    printf("output_file: %s; \n", (char *)output_file.c_str());
    
    params_video_enc_mt.input_file = (char*)input_file.c_str();
    params_video_enc_mt.output_file = (char*)output_file.c_str();
    REQUIRE( video_enc_multi_thread(&params_video_enc_mt) == 0 );
}