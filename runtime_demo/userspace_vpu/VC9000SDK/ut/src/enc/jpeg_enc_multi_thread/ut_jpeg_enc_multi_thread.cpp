#include "jpeg_enc_multi_thread_catch2.c"

#include "catch2/catch.hpp"

// --------------------------------------------------------------------------------------

TEST_CASE( "unitest_jpeg_enc_multi_thread_sample_normal_enc", "[sample][enc][normal][unitest]" ) {
    enc_options params_jpeg_enc_mt = {0};
    std::string input_file;
    // std::string pixel_fmt;
    std::tie(input_file, params_jpeg_enc_mt.width, params_jpeg_enc_mt.height, params_jpeg_enc_mt.stride, params_jpeg_enc_mt.pixel_fmt) = GENERATE(
        table<std::string, int, int, int, char*>({
            
            std::make_tuple(std::string(UT_RES_PATH) + "cdzj_1080p_nv12.yuv", 1920, 1080, 0, (char*)"nv12"),
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_yuv/1920x1080.yuv", 1920, 1080, 0, (char*)"yuv420p"), 
        }));
    std::string output_file;
    std::tie(params_jpeg_enc_mt.store, output_file) = GENERATE(
        table<int, std::string>({
            std::make_tuple(0, std::string(UT_RES_PATH) + "enc"), 
            // std::make_tuple(1, "/root/bozhang/video/yuv")
        }));
    params_jpeg_enc_mt.device = GENERATE((char *)"/dev/vastai_video0");
    params_jpeg_enc_mt.loop = GENERATE(500);
    params_jpeg_enc_mt.md5 = GENERATE(1);
    params_jpeg_enc_mt.store = GENERATE(0);
    params_jpeg_enc_mt.thread_num = GENERATE(100);
    params_jpeg_enc_mt.rotation = GENERATE(0, 1);

    params_jpeg_enc_mt.input_file = (char *)input_file.c_str();
    params_jpeg_enc_mt.output_file = (char *)output_file.c_str();
    
    
    SECTION( "jpeg_dec()" ) {
        REQUIRE( jpeg_enc_multi_thread_catch2(&params_jpeg_enc_mt) == 0 );
    }
}
