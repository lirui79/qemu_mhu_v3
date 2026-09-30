#include "jpeg_enc_catch2.c"

#include "catch2/catch.hpp"

// --------------------------------------------------------------------------------------

TEST_CASE( "unitest_jpeg_enc_sample_normal_enc", "[sample][enc][normal][unitest]" ) {
    enc_options params_jpeg_enc = {0};
    std::string input_file;
    // std::string pixel_fmt;
    std::tie(input_file, params_jpeg_enc.width, params_jpeg_enc.height, params_jpeg_enc.stride, params_jpeg_enc.pixel_fmt) = GENERATE(
        table<std::string, int, int, int, char*>({
            
            std::make_tuple(std::string(UT_RES_PATH) + "cdzj_1080p_nv12.yuv", 1920, 1080, 0, (char*)"nv12"),
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_yuv/1920x1080.yuv", 1920, 1080, 0, (char*)"yuv420p"),
        }));
    std::string output_file;
    std::tie(params_jpeg_enc.store, output_file) = GENERATE(
        table<int, std::string>({
            std::make_tuple(1, std::string(UT_RES_PATH) + "enc"), 
            // std::make_tuple(1, "/root/bozhang/video/yuv")
        }));
    params_jpeg_enc.device = GENERATE((char *)"/dev/vastai_video0");
    params_jpeg_enc.loop = GENERATE(1);
    params_jpeg_enc.md5 = GENERATE(1);
    params_jpeg_enc.rotation = GENERATE(0, 1, 2, 3);
    params_jpeg_enc.qLevel = GENERATE(1, 50, 100, 101); //Quantization level (1 - 101).
                                                        // 101 for user quantization table, qTableLuma and qTableChroma will take effect, otherwise not.

    params_jpeg_enc.input_file = (char *)input_file.c_str();
    params_jpeg_enc.output_file = (char *)output_file.c_str();
    
    SECTION( "jpeg_enc()" ) {
        REQUIRE( jpeg_enc(&params_jpeg_enc) == 0 );
    }
}

TEST_CASE( "unitest_jpeg_enc_edge_resolution_sample_exceptionEdgeResolution_enc", "[sample][enc][exception_edge_resolution][unitest]" ) {
    enc_options params_jpeg_enc = {0};
    std::string input_file;
    std::string output_file;
    std::tie(params_jpeg_enc.store, output_file) = GENERATE(
        table<int, std::string>({
            std::make_tuple(1, std::string(UT_RES_PATH) + "enc"), 
            // std::make_tuple(1, "/root/bozhang/video/yuv")
        }));
    params_jpeg_enc.device = GENERATE((char *)"/dev/vastai_video0");
    params_jpeg_enc.loop = GENERATE(1);
    params_jpeg_enc.md5 = GENERATE(1);
    params_jpeg_enc.rotation = GENERATE(0);

    params_jpeg_enc.output_file = (char *)output_file.c_str();
    
    SECTION( "out_of_edge" ) {
        std::tie(input_file, params_jpeg_enc.width, params_jpeg_enc.height, params_jpeg_enc.stride, params_jpeg_enc.pixel_fmt) = GENERATE(
        table<std::string, int, int, int, char*>({ //32-32768
            std::make_tuple(std::string(UT_RES_PATH) + "edge_resolution_enc/176x30.yuv", 176, 30, 0, (char*)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "edge_resolution_enc/176x32770.yuv", 176, 32770, 0, (char*)"yuv420p"), 
            std::make_tuple(std::string(UT_RES_PATH) + "edge_resolution_enc/30x176.yuv", 30, 176, 0, (char*)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "edge_resolution_enc/32770x176.yuv", 32770, 176, 0, (char*)"yuv420p"), 
        }));

        params_jpeg_enc.input_file = (char *)input_file.c_str();
        REQUIRE( jpeg_enc(&params_jpeg_enc) == -1 );
    }
    SECTION( "on_edge" ) {
        std::tie(input_file, params_jpeg_enc.width, params_jpeg_enc.height, params_jpeg_enc.stride, params_jpeg_enc.pixel_fmt) = GENERATE(
        table<std::string, int, int, int, char*>({ //32-32768
            std::make_tuple(std::string(UT_RES_PATH) + "edge_resolution_enc/176x32.yuv", 176, 32, 0, (char*)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "edge_resolution_enc/176x32768.yuv", 176, 32768, 0, (char*)"yuv420p"), 
            std::make_tuple(std::string(UT_RES_PATH) + "edge_resolution_enc/32x176.yuv", 32, 176, 0, (char*)"yuv420p"),
            std::make_tuple(std::string(UT_RES_PATH) + "edge_resolution_enc/32768x176.yuv", 32768, 176, 0, (char*)"yuv420p"), 
        }));

        params_jpeg_enc.input_file = (char *)input_file.c_str();
        REQUIRE( jpeg_enc(&params_jpeg_enc) == 0 );
    }
}

TEST_CASE( "unitest_jpeg_enc_zero_resolution_sample_exceptionZeroResolution_enc", "[sample][enc][exception_zero_resolution][unitest]" ) {
    enc_options params_jpeg_enc = {0};

    std::string input_file;
    std::string output_file;
    std::tie(params_jpeg_enc.store, output_file) = GENERATE(
        table<int, std::string>({
            std::make_tuple(1, std::string(UT_RES_PATH) + "enc"), 
            // std::make_tuple(1, "/root/bozhang/video/yuv")
        }));
    params_jpeg_enc.device = GENERATE((char *)"/dev/vastai_video0");
    params_jpeg_enc.loop = GENERATE(1);
    params_jpeg_enc.md5 = GENERATE(1);
    params_jpeg_enc.rotation = GENERATE(0);
    std::tie(input_file, params_jpeg_enc.width, params_jpeg_enc.height, params_jpeg_enc.stride, params_jpeg_enc.pixel_fmt) = GENERATE(
    table<std::string, int, int, int, char*>({ 
        std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/7680x4321.yuv", 7680, 0, 0, (char*)"yuv420p"),
        std::make_tuple(std::string(UT_RES_PATH) + "odd_resolution_yuv/7679x4320.yuv", 0, 4320, 0, (char*)"yuv420p"),
        
    }));
    
    params_jpeg_enc.output_file = (char *)output_file.c_str();
    params_jpeg_enc.input_file = (char *)input_file.c_str();

    REQUIRE( jpeg_enc(&params_jpeg_enc) == -1 );
}
