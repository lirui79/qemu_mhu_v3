#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
#include "multi_thread_video_dec_catch2.c"
#ifdef __cplusplus
}
#endif /* __cplusplus */
#include "catch2/catch.hpp"

// --------------------------------------------------------------------------------------

TEST_CASE( "unitest_multi_thread_video_dec_sample_normal_dec", "[sample][dec][normal][unitest]" ) {
    
    auto loop = GENERATE(1, 2);
    auto thread_count = GENERATE(60);
    auto memory_mode = GENERATE(0);
    auto output_align = GENERATE(0);
    auto stream_repeat_times = GENERATE(0, 2);
    auto drop = GENERATE(0);
    auto distort = GENERATE(0);
    std::string input = GENERATE(
        std::string(UT_RES_PATH) + "json/dec/multi_thread_video_dec.json",
        std::string(UT_RES_PATH) + "json/dec/multi_thread_video_dec_av1_sv100.json"
        
    );
    
    SECTION( "decoder_normal" ) {
        REQUIRE( decode((char *)input.c_str(), 1, loop, thread_count, memory_mode, output_align, stream_repeat_times, drop, distort) == 0 ); // 1: default save yuv
    }
}

TEST_CASE( "unitest_multi_thread_video_dec_exception_sample_exception_dec", "[sample][dec][exception_test][unitest]" ) {
    
    auto loop = GENERATE(1);
    auto thread_count = GENERATE(20);
    auto memory_mode = GENERATE(0);
    auto output_align = GENERATE(0);
    auto stream_repeat_times = GENERATE(0);
    auto drop = GENERATE(0,10);
    auto distort = GENERATE(0,10);
    std::string input = GENERATE(
        std::string(UT_RES_PATH) + "json/dec/multi_thread_video_dec.json",
        std::string(UT_RES_PATH) + "json/dec/multi_thread_video_dec_av1_sv100.json"
        
    );
    
    SECTION( "decoder_exception" ) {
        REQUIRE( decode((char *)input.c_str(), 1, loop, thread_count, memory_mode, output_align, stream_repeat_times, drop, distort) == 0 );
    }
}

TEST_CASE( "unitest_multi_thread_video_dec_threads_120_sample_threads120_dec", "[sample][dec][threads_120][unitest]" ) {
    
    auto loop = GENERATE(1);
    auto thread_count = GENERATE(60);
    auto memory_mode = GENERATE(0);
    auto output_align = GENERATE(0);
    auto stream_repeat_times = GENERATE(0);
    auto drop = GENERATE(0);
    auto distort = GENERATE(0);
    auto input = GENERATE(
        std::string(UT_RES_PATH) + "json/dec/multi_thread_video_dec_avs2_100.json",
        std::string(UT_RES_PATH) + "json/dec/multi_thread_video_dec_vp9_100.json",
        std::string(UT_RES_PATH) + "json/dec/multi_thread_video_dec_av1_100.json",
        std::string(UT_RES_PATH) + "json/dec/multi_thread_video_dec_hevc_100.json",
        std::string(UT_RES_PATH) + "json/dec/multi_thread_video_dec_h264_100.json"
        
        );
    
    
    SECTION( "decoder_100channels" ) {
        REQUIRE( decode((char *)input.c_str(), 0, loop, thread_count, memory_mode, output_align, stream_repeat_times, drop, distort) == 0 );
    }
    sleep(1);
}

// exclude case by name: ./build/out/dec_ut exclude:multi_thread_video_dec_stress_testing exclude:case_name
// exclude case by tag:  ./build/out/dec_ut ~[threads_120] ~[tag]