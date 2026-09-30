#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
#include "multi_thread_jpeg_dec_catch2.c"
#ifdef __cplusplus
}
#endif /* __cplusplus */
#include "catch2/catch.hpp"

// --------------------------------------------------------------------------------------

TEST_CASE( "unitest_multi_thread_jpeg_dec_sample_normal_dec", "[sample][dec][normal][unitest]" ) {
    
    auto loop = GENERATE(1);
    auto loop_in_loop = GENERATE(1, 2);
    auto save = GENERATE(1);
    std::string json = std::string(UT_RES_PATH) + "json/dec/multi_thread_jpeg_dec.json";
    
    SECTION( "multi_thread_jpeg_dec()" ) {
        REQUIRE( decode((char *)json.c_str(), save, loop, loop_in_loop) == 0 );
    }
}
