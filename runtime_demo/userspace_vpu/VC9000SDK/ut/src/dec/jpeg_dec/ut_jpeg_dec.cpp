#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
#include "jpeg_dectestbench_catch2.c"
#ifdef __cplusplus
}
#endif /* __cplusplus */
#include <string>
#include "catch2/catch.hpp"

// --------------------------------------------------------------------------------------

TEST_CASE( "unitest_jpeg_dec_sample_normal_dec", "[sample][dec][normal][functions_alignment][unitest]" ) {
    option_t params_jpeg_dec = {0};
    params_jpeg_dec.loop = GENERATE(1);
    params_jpeg_dec.device = GENERATE((char *)"/dev/vastai_video0");
    std::string input;
    std::string output;
    std::tie(input, output) = GENERATE(
        table<std::string, std::string>({
            std::make_tuple(std::string(UT_RES_PATH) + "dec/stream1.jpg", std::string(UT_RES_OUT) + "funcs_align_out/jpeg_dec_stream1.yuv"), 
            std::make_tuple(std::string(UT_RES_PATH) + "dec/stream2.jpg", std::string(UT_RES_OUT) + "funcs_align_out/jpeg_dec_stream2.yuv"),
            std::make_tuple(std::string(UT_RES_PATH) + "dec/stream3.jpg", std::string(UT_RES_OUT) + "funcs_align_out/jpeg_dec_stream3.yuv")
        }));
    params_jpeg_dec.md5 = GENERATE(1);
    params_jpeg_dec.save = GENERATE(1);
    params_jpeg_dec.memoryMode = GENERATE(0, 4);
    
    params_jpeg_dec.input = (char *)input.c_str();
    params_jpeg_dec.output = (char *)output.c_str();
    SECTION( "decoder()" ) {
        REQUIRE( decode(&params_jpeg_dec) == 0 );
    }
}

TEST_CASE( "unitest_jpeg_dec_edge_resolution_sample_exceptionEdgeResolution_dec", "[sample][dec][exception_edge_resolution][unitest]" ) {
    option_t params_jpeg_dec = {0};
    params_jpeg_dec.loop = GENERATE(1);
    params_jpeg_dec.device = GENERATE((char *)"/dev/vastai_video0");
    std::string input;
    std::string output;
    
    params_jpeg_dec.md5 = GENERATE(1);
    params_jpeg_dec.save = GENERATE(1);
    
    SECTION( "out_of_edge" ) {
        std::tie(input, output) = GENERATE(
        table<std::string, std::string>({ //width: 34-32768 height: 42-32768
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_jpeg/32x176.jpg", std::string(UT_RES_OUT) + "yuv/stream1.yuv"), 
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_jpeg/32770x176.jpg", std::string(UT_RES_OUT) + "yuv/stream2.yuv"),
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_jpeg/176x40.jpg", std::string(UT_RES_OUT) + "yuv/stream1.yuv"), 
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_jpeg/176x32770.jpg", std::string(UT_RES_OUT) + "yuv/stream2.yuv")
        }));
        params_jpeg_dec.input = (char *)input.c_str();
        params_jpeg_dec.output = (char *)output.c_str();
        REQUIRE( decode(&params_jpeg_dec) == -1 );
    }
    SECTION( "on_edge" ) {
        std::tie(input, output) = GENERATE(
        table<std::string, std::string>({ //width: 34-32768 height: 42-32768
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_jpeg/34x176.jpg", std::string(UT_RES_OUT) + "yuv/stream1.yuv"), 
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_jpeg/32768x176.jpg", std::string(UT_RES_OUT) + "yuv/stream2.yuv"),
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_jpeg/176x42.jpg", std::string(UT_RES_OUT) + "yuv/stream1.yuv"), 
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_jpeg/176x32768.jpg", std::string(UT_RES_OUT) + "yuv/stream2.yuv")
        }));
        params_jpeg_dec.input = (char *)input.c_str();
        params_jpeg_dec.output = (char *)output.c_str();
        REQUIRE( decode(&params_jpeg_dec) == 0 );
    }
}