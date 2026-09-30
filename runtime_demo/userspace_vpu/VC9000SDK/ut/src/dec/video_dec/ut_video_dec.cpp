#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
#include "video_dectestbench_catch2.c"
#ifdef __cplusplus
}
#endif /* __cplusplus */
#include "catch2/catch.hpp"

// --------------------------------------------------------------------------------------
int loop_process_dec=0;
TEST_CASE( "unitest_video_dec_sample_normal_dec", "[sample][dec][normal][unitest]" ) {
    ++loop_process_dec;
    params_video_dec_t params_video_dec = {0};
    params_video_dec.option.loop = GENERATE(1);
    params_video_dec.option.device = GENERATE((char *)"/dev/vastai_video0");
    std::string input;
    std::string output;
    std::string codec;
    std::tie(input, codec, output, params_video_dec.option.no_output_reordering, params_video_dec.option.buf_slim_mode, params_video_dec.option.stream_repeat_times, params_video_dec.option.vframes, params_video_dec.firstFrame_time_test) = GENERATE( // if(firstFrame_time_test > 0) option->memory_mode = 0; other memory_mode is not adapt
        table<std::string, std::string, std::string, int, int, int, int, int>({
            
            std::make_tuple( std::string(UT_RES_PATH) + "dec/cdzj.h264", "h264",  std::string(UT_RES_OUT) + "yuv/cdzj.yuv", 1, 1, 2, 0, 0), 
            std::make_tuple( std::string(UT_RES_PATH) + "dec/cdzj.hevc", "hevc", std::string(UT_RES_OUT) + "yuv/cdzj_hevc.yuv", 1, 0, 2, 0, 0),
            std::make_tuple( std::string(UT_RES_PATH) + "dec/cdzj.ivf", "av1", std::string(UT_RES_OUT) + "yuv/cdzj_av1.yuv", 0, 1, 2, 0, 0),
            std::make_tuple( std::string(UT_RES_PATH) + "dec/cdzj_vp9.ivf", "vp9", std::string(UT_RES_OUT) + "yuv/cdzj_av1.yuv", 0, 0, 2, 0, 0),
            std::make_tuple( std::string(UT_RES_PATH) + "dec/cdzj.avs", "avs2",  std::string(UT_RES_OUT) + "yuv/cdzj_avs.yuv", 0, 1, 2, 0, 0),

            std::make_tuple( std::string(UT_RES_PATH) + "resolutions_multicore/cdzj_7680x4320.hevc", "hevc",  std::string(UT_RES_OUT) + "yuv/cdzj_multicore_8k.yuv", 0, 0, 0, 10, 0), 
            std::make_tuple( std::string(UT_RES_PATH) + "resolutions_multicore/cdzj_3840x2160.h264", "h264", std::string(UT_RES_OUT) + "yuv/cdzj_multicore_4k.yuv", 0, 0, 0, 10, 0),
            std::make_tuple( std::string(UT_RES_PATH) + "resolutions_multicore/cdzj_7680x4320.ivf", "av1",  std::string(UT_RES_OUT) + "yuv/cdzj_multicore_8k_av1.yuv", 0, 0, 0, 10, 0), 
            std::make_tuple( std::string(UT_RES_PATH) + "resolutions_multicore/cdzj_7680x4320_vp9.ivf", "vp9",  std::string(UT_RES_OUT) + "yuv/cdzj_multicore_8k_vp9.yuv", 0, 0, 0, 10, 0), 

            // // // // interlaced stream
            std::make_tuple( std::string(UT_RES_PATH) + "dec/t800_1080p_264bp_main.264", "h264", std::string(UT_RES_OUT) + "yuv/t800_720p_264bp_main.yuv", 1, 1, 0, 10, 0),
            std::make_tuple( std::string(UT_RES_PATH) + "dec/t800_720p_264hp_assist.264", "h264", std::string(UT_RES_OUT) + "yuv/t800_1080p_264bp_assist.yuv", 1, 0, 0, 10, 0),
            std::make_tuple( std::string(UT_RES_PATH) + "dec/t800_1080p_264hp_main.264", "h264", std::string(UT_RES_OUT) + "yuv/t800_1080p_264hp_main.yuv", 0, 1, 0, 10, 0),
            std::make_tuple( std::string(UT_RES_PATH) + "dec/t800_720p_264bp_assist.264", "h264", std::string(UT_RES_OUT) + "yuv/t800_720p_264hp_assist.yuv", 0, 0, 0, 10, 0),
            
        }));
    params_video_dec.option.save = GENERATE(1);
    params_video_dec.option.core_mode = GENERATE(0);
    params_video_dec.option.memory_mode = GENERATE(0, 1, 2, 3, 4); //5 only supported on certain version of driver for sg100.
    if(params_video_dec.firstFrame_time_test > 0) option->memory_mode = 0; //other memory_mode is not adapt
    params_video_dec.option.output_align = GENERATE(0);
    params_video_dec.switch_drop = GENERATE(0);
    params_video_dec.switch_distort = GENERATE(0);
    params_video_dec.option.decode_mode = GENERATE(0, 1, 2, 3);
    params_video_dec.option.log_level = GENERATE(2);
    params_video_dec.option.crop = GENERATE(0);
    params_video_dec.option.apiMode = GENERATE(0, 1);
    
    if ((codec == "av1" || codec == "vp9" || codec == "avs2") && (params_video_dec.option.memory_mode == 4))
        return;

    params_video_dec.option.input = (char *)input.c_str();
    params_video_dec.option.output = (char *)output.c_str();
    params_video_dec.option.codec= (char *)codec.c_str();
    SECTION( "video_dec()" ) {
        REQUIRE( vid_decode(&params_video_dec) == 0 );
    }
}

int multicore_dec_num = 0;
TEST_CASE( "unitest_video_dec_sample_resolutionStandard_dec_multicore", "[sample][dec][resolutionStandard][unitest][multicore]" ) {
    int status = 0;
    multicore_dec_num++;
    params_video_dec_t params_video_dec = {0};
    if(multicore_dec_num <= 10 || multicore_dec_num > 208) {
#if defined  __aarch64__
        printf("this is arm cpu\n");
        status = system("/video-case/lowlevel_SDK/vatools_SV100_arm/vasmi setvideomulticore 1 -d 0 -i 1"); // ,1
#elif defined __x86_64__
        printf("this is x86 cpu\n");
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
    
    params_video_dec.option.loop = GENERATE(1);
    params_video_dec.option.device = GENERATE((char *)"/dev/vastai_video1"); // , (char *)"/dev/vastai_video3"
    params_video_dec.option.vframes = GENERATE(5);
    std::string input;
    std::string codec;
    std::string output;
    std::tie(input, codec, output) = GENERATE(
        table<std::string, std::string, std::string>({
            std::make_tuple( std::string(UT_RES_PATH) + "resolutions_multicore/cdzj_7680x4320.h264", "h264", std::string(UT_RES_OUT) + "yuv/cdzj_multicore.yuv"), 
            std::make_tuple( std::string(UT_RES_PATH) + "resolutions_multicore/cdzj_7680x4320.hevc", "hevc", std::string(UT_RES_OUT) + "yuv/cdzj_multicore.yuv"),
            std::make_tuple( std::string(UT_RES_PATH) + "resolutions_multicore/cdzj_7680x4320.ivf", "av1", std::string(UT_RES_OUT) + "yuv/cdzj_multicore.yuv"),
            std::make_tuple( std::string(UT_RES_PATH) + "resolutions_multicore/cdzj_7680x4320_vp9.ivf", "vp9", std::string(UT_RES_OUT) + "yuv/cdzj_multicore.yuv"),
            std::make_tuple( std::string(UT_RES_PATH) + "resolutions_multicore/cdzj_7680x4320.avs", "avs2", std::string(UT_RES_OUT) + "yuv/cdzj_multicore.yuv"),

            std::make_tuple( std::string(UT_RES_PATH) + "resolutions_multicore/cdzj_3840x2160.h264", "h264", std::string(UT_RES_OUT) + "yuv/cdzj_multicore.yuv"), 
            std::make_tuple( std::string(UT_RES_PATH) + "resolutions_multicore/cdzj_3840x2160.hevc", "hevc", std::string(UT_RES_OUT) + "yuv/cdzj_multicore.yuv"),
            // std::make_tuple( std::string(UT_RES_PATH) + "resolutions_multicore/cdzj_3840x2160.ivf", "av1", std::string(UT_RES_OUT) + "yuv/cdzj_multicore.yuv"),
            // std::make_tuple( std::string(UT_RES_PATH) + "resolutions_multicore/cdzj_3840x2160_vp9.ivf", "vp9", std::string(UT_RES_OUT) + "yuv/cdzj_multicore.yuv"),
            // std::make_tuple( std::string(UT_RES_PATH) + "resolutions_multicore/cdzj_3840x2160.avs", "avs2", std::string(UT_RES_OUT) + "yuv/cdzj_multicore.yuv"),

            std::make_tuple( std::string(UT_RES_PATH) + "resolutions_multicore/cdzj_1080p.h264", "h264", std::string(UT_RES_OUT) + "yuv/cdzj_multicore.yuv"), 
            std::make_tuple( std::string(UT_RES_PATH) + "resolutions_multicore/cdzj_1080p.hevc", "hevc", std::string(UT_RES_OUT) + "yuv/cdzj_multicore.yuv")
            
        }));
    params_video_dec.option.core_mode = GENERATE(0);
    params_video_dec.option.memory_mode = GENERATE(0, 1, 2);
    params_video_dec.option.output_align = GENERATE(0);
    params_video_dec.switch_drop = GENERATE(0);
    params_video_dec.switch_distort = GENERATE(0);
    params_video_dec.option.decode_mode = GENERATE(0, 1, 2, 3);
    params_video_dec.option.log_level = GENERATE(2);
    params_video_dec.option.crop = GENERATE(0);
    params_video_dec.option.no_output_reordering = GENERATE(0);
    params_video_dec.option.buf_slim_mode = GENERATE(0);
    params_video_dec.option.stream_repeat_times = GENERATE(0, 2);
    params_video_dec.option.apiMode = GENERATE(0, 1);
    params_video_dec.firstFrame_time_test = GENERATE(0);

    params_video_dec.option.input = (char *)input.c_str();
    params_video_dec.option.output = (char *)output.c_str();
    params_video_dec.option.codec= (char *)codec.c_str();
    SECTION( "video_dec()" ) {
        REQUIRE( vid_decode(&params_video_dec) == 0 );
    }

    if(multicore_dec_num < 10 || multicore_dec_num >= 208) {
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
    sleep(0.2);
}

TEST_CASE( "unitest_video_dec_sample_normal_exception_dec", "[sample][dec][normal_exce][unitest]" ) {
    params_video_dec_t params_video_dec = {0};
    params_video_dec.option.loop = GENERATE(1);
    params_video_dec.option.device = GENERATE((char *)"/dev/vastai_video0");
    params_video_dec.option.vframes = GENERATE(0);
    std::string input;
    std::string codec;
    std::string output;
    std::tie(input, codec, output) = GENERATE(
        table<std::string, std::string, std::string>({
            std::make_tuple( std::string(UT_RES_PATH) + "dec/cdzj.h264", "h264",  std::string(UT_RES_OUT) + "yuv/cdzj_h264_excep.yuv"), 
            std::make_tuple( std::string(UT_RES_PATH) + "dec/cdzj.hevc", "hevc", std::string(UT_RES_OUT) + "yuv/cdzj_hevc_excep.yuv"),
            std::make_tuple( std::string(UT_RES_PATH) + "dec/cdzj.ivf", "av1", std::string(UT_RES_OUT) + "yuv/cdzj_av1_excep.yuv"),
            std::make_tuple( std::string(UT_RES_PATH) + "dec/cdzj_vp9.ivf", "vp9", std::string(UT_RES_OUT) + "yuv/cdzj_av1_excep.yuv"),
            std::make_tuple( std::string(UT_RES_PATH) + "dec/cdzj.avs", "avs2", std::string(UT_RES_OUT) + "yuv/cdzj_avs2_excep.yuv")
            
        }));
    params_video_dec.option.core_mode = GENERATE(0);
    params_video_dec.option.memory_mode = GENERATE(0, 1, 2, 3, 4);
    params_video_dec.option.output_align = GENERATE(0);
    params_video_dec.switch_drop = GENERATE(0, 10);     //frames drop by 0% ,10%
    params_video_dec.switch_distort = GENERATE(0, 10);  //frame distort by 0% ,10%
    params_video_dec.option.decode_mode = GENERATE(0, 1, 2, 3);
    params_video_dec.option.log_level = GENERATE(2);
    params_video_dec.option.crop = GENERATE(0);
    params_video_dec.option.no_output_reordering = GENERATE(0);
    params_video_dec.option.buf_slim_mode = GENERATE(0);
    params_video_dec.option.stream_repeat_times = GENERATE(0);
    params_video_dec.option.apiMode = GENERATE(0, 1);
    params_video_dec.firstFrame_time_test = GENERATE(0);
    
    if (params_video_dec.option.memory_mode == 2 && params_video_dec.switch_distort > 0 && codec == std::string("avs2"))
        params_video_dec.option.memory_mode = 0;
    
    if ((codec == "av1" || codec == "vp9" || codec == "avs2") && (params_video_dec.option.memory_mode == 4))
        return;

    params_video_dec.option.input = (char *)input.c_str();
    params_video_dec.option.output = (char *)output.c_str();
    params_video_dec.option.codec= (char *)codec.c_str();
    SECTION( "video_dec()" ) {
        REQUIRE( vid_decode(&params_video_dec) == 0 );
    }
}

TEST_CASE( "unitest_video_dec_edge_resolution_sample_exceptionEdgeResolution_dec", "[sample][dec][exception_edge_resolution][unitest]" ) {
    params_video_dec_t params_video_dec = {0};
    params_video_dec.option.loop = GENERATE(1);
    params_video_dec.option.device = GENERATE((char *)"/dev/vastai_video0");
    params_video_dec.option.vframes = GENERATE(0);
    std::string input;
    std::string codec;
    std::string output;
    params_video_dec.option.core_mode = GENERATE(0);
    params_video_dec.option.memory_mode = GENERATE(0);
    params_video_dec.option.output_align = GENERATE(0);
    params_video_dec.switch_drop = GENERATE(0);
    params_video_dec.switch_distort = GENERATE(0);
    params_video_dec.option.decode_mode = GENERATE(0);
    params_video_dec.option.log_level = GENERATE(2);
    params_video_dec.option.crop = GENERATE(0);
    params_video_dec.option.no_output_reordering = GENERATE(0);
    params_video_dec.option.buf_slim_mode = GENERATE(0);
    params_video_dec.option.stream_repeat_times = GENERATE(0);
    params_video_dec.option.apiMode = GENERATE(0, 1);
    params_video_dec.firstFrame_time_test = GENERATE(0);
    SECTION( "out_of_edge" ) {
        std::tie(input, codec, output) = GENERATE(
        table<std::string, std::string, std::string>({ // 50-8192
            std::make_tuple( std::string(UT_RES_PATH) + "edge_resolution_dec/176x8194.h264", "h264",  std::string(UT_RES_OUT) + "yuv/cdzj_edge_176x8194_h264.yuv"), 
            std::make_tuple( std::string(UT_RES_PATH) + "edge_resolution_dec/176x48.h264", "h264", std::string(UT_RES_OUT) + "yuv/cdzj_hevc_edge_176x48_h264.yuv"),
            std::make_tuple( std::string(UT_RES_PATH) + "edge_resolution_dec/8194x176.h264", "h264",  std::string(UT_RES_OUT) + "yuv/cdzj_edge_8194x176_h264.yuv"), 
            std::make_tuple( std::string(UT_RES_PATH) + "edge_resolution_dec/48x176.h264", "h264", std::string(UT_RES_OUT) + "yuv/cdzj_hevc_edge_48x176_h264.yuv"),

            // 58-8192
            std::make_tuple( std::string(UT_RES_PATH) + "edge_resolution_dec/176x8194.hevc", "hevc",  std::string(UT_RES_OUT) + "yuv/cdzj_edge_176x8194_hevc.yuv"), 
            std::make_tuple( std::string(UT_RES_PATH) + "edge_resolution_dec/176x56.hevc", "hevc", std::string(UT_RES_OUT) + "yuv/cdzj_hevc_edge_176x56_hevc.yuv"),
            std::make_tuple( std::string(UT_RES_PATH) + "edge_resolution_dec/8194x176.hevc", "hevc",  std::string(UT_RES_OUT) + "yuv/cdzj_edge_8194x176_hevc.yuv"), 
            std::make_tuple( std::string(UT_RES_PATH) + "edge_resolution_dec/56x176.hevc", "hevc", std::string(UT_RES_OUT) + "yuv/cdzj_hevc_edge_56x176_hevc.yuv"),

            // 66-8192
            std::make_tuple( std::string(UT_RES_PATH) + "edge_resolution_dec/176x8194.ivf", "av1",  std::string(UT_RES_OUT) + "yuv/cdzj_edge_176x8194_av1.yuv"), 
            std::make_tuple( std::string(UT_RES_PATH) + "edge_resolution_dec/176x64.ivf", "av1", std::string(UT_RES_OUT) + "yuv/cdzj_hevc_edge_176x64_av1.yuv"),
            std::make_tuple( std::string(UT_RES_PATH) + "edge_resolution_dec/8194x176.ivf", "av1",  std::string(UT_RES_OUT) + "yuv/cdzj_edge_8194x176_av1.yuv"), 
            std::make_tuple( std::string(UT_RES_PATH) + "edge_resolution_dec/64x176.ivf", "av1", std::string(UT_RES_OUT) + "yuv/cdzj_hevc_edge_64x176_av1.yuv"),

            // 66-8192
            std::make_tuple( std::string(UT_RES_PATH) + "edge_resolution_dec/176x8194_vp9.ivf", "vp9",  std::string(UT_RES_OUT) + "yuv/cdzj_edge_176x4354_vp9.yuv"),
            std::make_tuple( std::string(UT_RES_PATH) + "edge_resolution_dec/176x64_vp9.ivf", "vp9", std::string(UT_RES_OUT) + "yuv/cdzj_hevc_edge_176x64_vp9.yuv"),
            std::make_tuple( std::string(UT_RES_PATH) + "edge_resolution_dec/8194x176_vp9.ivf", "vp9",  std::string(UT_RES_OUT) + "yuv/cdzj_edge_8194x176_vp9.yuv"), 
            std::make_tuple( std::string(UT_RES_PATH) + "edge_resolution_dec/64x176_vp9.ivf", "vp9", std::string(UT_RES_OUT) + "yuv/cdzj_hevc_edge_64x176_vp9.yuv"),

            // 58- , 58- avs2
            std::make_tuple( std::string(UT_RES_PATH) + "avs_edge/176x56.avs", "avs2",  std::string(UT_RES_OUT) + "yuv/cdzj_edge_176x56_avs2.yuv"),
            // std::make_tuple( std::string(UT_RES_PATH) + "avs_edge/176x8194.avs", "vp9", std::string(UT_RES_OUT) + "yuv/cdzj_hevc_edge_176x64_avs2.yuv"),
            std::make_tuple( std::string(UT_RES_PATH) + "avs_edge/56x176.avs", "avs2",  std::string(UT_RES_OUT) + "yuv/cdzj_edge_56x176_avs2.yuv"), 
            // std::make_tuple( std::string(UT_RES_PATH) + "avs_edge/8194x176.avs", "vp9", std::string(UT_RES_OUT) + "yuv/cdzj_hevc_edge_64x176_avs2.yuv")

        }));
        params_video_dec.option.input = (char *)input.c_str();
        params_video_dec.option.output = (char *)output.c_str();
        params_video_dec.option.codec= (char *)codec.c_str();
        REQUIRE( vid_decode(&params_video_dec) == -1 );
    };
    SECTION( "on_edge" ) {
        std::tie(input, codec, output) = GENERATE(
        table<std::string, std::string, std::string>({ // 50-8192
            std::make_tuple( std::string(UT_RES_PATH) + "edge_resolution_dec/176x8192.h264", "h264",  std::string(UT_RES_OUT) + "yuv/cdzj_edge_176x8192_h264.yuv"), 
            std::make_tuple( std::string(UT_RES_PATH) + "edge_resolution_dec/176x50.h264", "h264", std::string(UT_RES_OUT) + "yuv/cdzj_hevc_edge_176x50_h264.yuv"),
            std::make_tuple( std::string(UT_RES_PATH) + "edge_resolution_dec/8192x176.h264", "h264",  std::string(UT_RES_OUT) + "yuv/cdzj_edge_8192x176_h264.yuv"), 
            std::make_tuple( std::string(UT_RES_PATH) + "edge_resolution_dec/50x176.h264", "h264", std::string(UT_RES_OUT) + "yuv/cdzj_hevc_edge_50x176_h264.yuv"),

            // 58-8192
            std::make_tuple( std::string(UT_RES_PATH) + "edge_resolution_dec/176x8192.hevc", "hevc",  std::string(UT_RES_OUT) + "yuv/cdzj_edge_176x8192_hevc.yuv"), 
            std::make_tuple( std::string(UT_RES_PATH) + "edge_resolution_dec/176x58.hevc", "hevc", std::string(UT_RES_OUT) + "yuv/cdzj_hevc_edge_176x58_hevc.yuv"),
            std::make_tuple( std::string(UT_RES_PATH) + "edge_resolution_dec/8192x176.hevc", "hevc",  std::string(UT_RES_OUT) + "yuv/cdzj_edge_8192x176_hevc.yuv"), 
            std::make_tuple( std::string(UT_RES_PATH) + "edge_resolution_dec/58x176.hevc", "hevc", std::string(UT_RES_OUT) + "yuv/cdzj_hevc_edge_58x176_hevc.yuv"),

            // w:66-8192 h:66-4352
            std::make_tuple( std::string(UT_RES_PATH) + "edge_resolution_dec/176x4352.ivf", "av1",  std::string(UT_RES_OUT) + "yuv/cdzj_edge_176x4352_av1.yuv"), 
            std::make_tuple( std::string(UT_RES_PATH) + "edge_resolution_dec/176x66.ivf", "av1", std::string(UT_RES_OUT) + "yuv/cdzj_hevc_edge_176x66_av1.yuv"),
            std::make_tuple( std::string(UT_RES_PATH) + "edge_resolution_dec/8192x176.ivf", "av1",  std::string(UT_RES_OUT) + "yuv/cdzj_edge_8192x176_av1.yuv"), 
            std::make_tuple( std::string(UT_RES_PATH) + "edge_resolution_dec/66x176.ivf", "av1", std::string(UT_RES_OUT) + "yuv/cdzj_hevc_edge_66x176_av1.yuv"),

            // 66-8192
            std::make_tuple( std::string(UT_RES_PATH) + "edge_resolution_dec/176x8192_vp9.ivf", "vp9",  std::string(UT_RES_OUT) + "yuv/cdzj_edge_176x4352_vp9.yuv"),
            std::make_tuple( std::string(UT_RES_PATH) + "edge_resolution_dec/176x66_vp9.ivf", "vp9", std::string(UT_RES_OUT) + "yuv/cdzj_hevc_edge_176x66_vp9.yuv"),
            std::make_tuple( std::string(UT_RES_PATH) + "edge_resolution_dec/8192x176_vp9.ivf", "vp9",  std::string(UT_RES_OUT) + "yuv/cdzj_edge_8192x176_vp9.yuv"), 
            std::make_tuple( std::string(UT_RES_PATH) + "edge_resolution_dec/66x176_vp9.ivf", "vp9", std::string(UT_RES_OUT) + "yuv/cdzj_hevc_edge_66x176_vp9.yuv"),

             // 66-8192 avs2
            std::make_tuple( std::string(UT_RES_PATH) + "avs_edge/176x4608.avs", "avs2",  std::string(UT_RES_OUT) + "yuv/cdzj_edge_176x4608_avs2.yuv"),
            std::make_tuple( std::string(UT_RES_PATH) + "avs_edge/176x58.avs", "avs2", std::string(UT_RES_OUT) + "yuv/cdzj_hevc_edge_176x58_avs2.yuv"),
            std::make_tuple( std::string(UT_RES_PATH) + "avs_edge/8192x176.avs", "avs2",  std::string(UT_RES_OUT) + "yuv/cdzj_edge_4608x176_avs2.yuv"),
            std::make_tuple( std::string(UT_RES_PATH) + "avs_edge/58x176.avs", "avs2", std::string(UT_RES_OUT) + "yuv/cdzj_hevc_edge_58x176_avs2.yuv")

            
        }));
        params_video_dec.option.input = (char *)input.c_str();
        params_video_dec.option.output = (char *)output.c_str();
        params_video_dec.option.codec= (char *)codec.c_str();
        REQUIRE( vid_decode(&params_video_dec) == 0 );
    }
}

TEST_CASE( "unitest_video_dec_sample_functions_alignment_dec", "[sample][dec][functions_alignment][unitest]" ) {
    params_video_dec_t params_video_dec = {0};
    params_video_dec.option.loop = GENERATE(1);
    params_video_dec.option.device = GENERATE((char *)"/dev/vastai_video0");
    params_video_dec.option.vframes = GENERATE(10);
    std::string input;
    std::string codec;
    std::string output;
    std::tie(input, codec, output) = GENERATE(
        table<std::string, std::string, std::string>({
            std::make_tuple( std::string(UT_RES_PATH) + "dec/cdzj.h264", "h264",  std::string(UT_RES_OUT) + "funcs_align_out/dec_cdzj_h264.yuv"), 
            std::make_tuple( std::string(UT_RES_PATH) + "dec/cdzj.hevc", "hevc", std::string(UT_RES_OUT) + "funcs_align_out/dec_cdzj_hevc.yuv"),
            std::make_tuple( std::string(UT_RES_PATH) + "dec/cdzj.ivf", "av1", std::string(UT_RES_OUT) + "funcs_align_out/dec_cdzj_av1.yuv"),
            std::make_tuple( std::string(UT_RES_PATH) + "dec/cdzj_vp9.ivf",  "vp9", std::string(UT_RES_OUT) + "funcs_align_out/dec_cdzj_av1.yuv"),
            std::make_tuple( std::string(UT_RES_PATH) + "dec/cdzj.avs", "avs2", std::string(UT_RES_OUT) + "funcs_align_out/dec_cdzj_avs2.yuv"),
            // field stream
            std::make_tuple( std::string(UT_RES_PATH) + "funcs_align_src/field_720x480.jvt", "h264", std::string(UT_RES_OUT) + "funcs_align_out/dec_field_720x480.yuv"),
            std::make_tuple( std::string(UT_RES_PATH) + "dec/H265_4_non-standard_SEI.hevc", "hevc",  std::string(UT_RES_OUT) + "funcs_align_out/non-standard_SEI.yuv"),
            std::make_tuple( std::string(UT_RES_PATH) + "10bit_stream/PE2_Leopard_4K.hevc", "hevc",  std::string(UT_RES_OUT) + "funcs_align_out/PE2_Leopard_4K.yuv"),
            
        }));
    params_video_dec.option.pix_fmt = GENERATE((char *)"nv12");
    params_video_dec.option.core_mode = GENERATE(0);
    params_video_dec.option.memory_mode = GENERATE(0);
    params_video_dec.option.output_align = GENERATE(0);
    params_video_dec.switch_drop = GENERATE(0);
    params_video_dec.switch_distort = GENERATE(0);
    params_video_dec.option.decode_mode = GENERATE(0);
    params_video_dec.option.log_level = GENERATE(2);
    params_video_dec.option.crop = GENERATE(0);
    params_video_dec.option.no_output_reordering = GENERATE(0);
    params_video_dec.option.buf_slim_mode = GENERATE(0);
    params_video_dec.option.stream_repeat_times = GENERATE(0);
    params_video_dec.option.apiMode = GENERATE(0, 1);
    params_video_dec.firstFrame_time_test = GENERATE(0);
    
    if (input == std::string(UT_RES_PATH) + "10bit_stream/PE2_Leopard_4K.hevc") {
        params_video_dec.option.pix_fmt = (char *)"I010"; // 如果条件满足，则设置为 "I010"
    }
    params_video_dec.option.input = (char *)input.c_str();
    params_video_dec.option.output = (char *)output.c_str();
    params_video_dec.option.codec= (char *)codec.c_str();
    SECTION( "video_dec()" ) {
        REQUIRE( vid_decode(&params_video_dec) == 0 );
    }
}

// test bitmatch for itu stream
// TEST_CASE( "video_dec_sample_dec_itu_dec", "[sample][dec][dec_itu]" ) {
    
//     auto loop = GENERATE(1);
//     auto device = GENERATE((char *)"/dev/vastai_video0");
//     auto vframes = GENERATE(0);
//     std::string input;
//     std::string codec;
//     std::string output;
//     std::tie(input, codec, output) = GENERATE(
//         table<std::string, std::string, std::string>({
            
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/3DHC_C_A.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/3DHC_C_A.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/3DHC_C_B.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/3DHC_C_B.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/3DHC_C_C.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/3DHC_C_C.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/3DHC_D1_A.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/3DHC_D1_A.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/3DHC_D1_B.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/3DHC_D1_B.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/3DHC_D1_C.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/3DHC_D1_C.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/3DHC_D1_D.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/3DHC_D1_D.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/3DHC_D1_E.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/3DHC_D1_E.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/3DHC_D1_F.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/3DHC_D1_F.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/3DHC_D1_G.bin", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/3DHC_D1_G.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/3DHC_D1_H.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/3DHC_D1_H.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/3DHC_D2_A.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/3DHC_D2_A.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/3DHC_D2_B.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/3DHC_D2_B.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/3DHC_DT_A.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/3DHC_DT_A.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/3DHC_DT_B.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/3DHC_DT_B.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/3DHC_DT_C.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/3DHC_DT_C.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/3DHC_DT_D.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/3DHC_DT_D.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/3DHC_T_A.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/3DHC_T_A.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/3DHC_T_B.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/3DHC_T_B.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/3DHC_T_C.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/3DHC_T_C.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/3DHC_TD_A.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/3DHC_TD_A.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/3DHC_TD_B.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/3DHC_TD_B.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/3DHC_T_D.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/3DHC_T_D.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/3DHC_TD_C.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/3DHC_TD_C.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/3DHC_TD_D.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/3DHC_TD_D.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/3DHC_TD_E.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/3DHC_TD_E.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/3DHC_T_E.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/3DHC_T_E.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/ADAPTRES_A_ERICSSON_1.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/ADAPTRES_A_ERICSSON_1.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/AMP_A_Samsung_7.bin", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/AMP_A_Samsung_7.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/AMP_B_Samsung_7.bin", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/AMP_B_Samsung_7.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/AMP_D_Hisilicon.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/AMP_D_Hisilicon.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/AMP_E_Hisilicon.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/AMP_E_Hisilicon.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/AMP_F_Hisilicon_3.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/AMP_F_Hisilicon_3.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/AMVP_A_MTK_4.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/AMVP_A_MTK_4.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/AMVP_B_MTK_4.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/AMVP_B_MTK_4.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/AMVP_C_Samsung_7.bin", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/AMVP_C_Samsung_7.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/BA1_Sony_D.jsv", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/BA1_Sony_D.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/BA2_Sony_F.jsv", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/BA2_Sony_F.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/BASQP1_Sony_C.jsv", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/BASQP1_Sony_C.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/CABA1_Sony_D.jsv", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/CABA1_Sony_D.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/CABA2_Sony_E.jsv", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/CABA2_Sony_E.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/CABA3_Sony_C.jsv", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/CABA3_Sony_C.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/CACQP3_Sony_D.jsv", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/CACQP3_Sony_D.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/CAINIT_A_SHARP_4.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/CAINIT_A_SHARP_4.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/CAINIT_B_SHARP_4.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/CAINIT_B_SHARP_4.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/CAINIT_C_SHARP_3.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/CAINIT_C_SHARP_3.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/CAINIT_D_SHARP_3.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/CAINIT_D_SHARP_3.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/CAINIT_E_SHARP_3.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/CAINIT_E_SHARP_3.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/CAINIT_F_SHARP_3.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/CAINIT_F_SHARP_3.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/cama1_vtc_c.avc", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/cama1_vtc_c.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/cama2_vtc_b.avc", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/cama2_vtc_b.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/cama3_vtc_b.avc", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/cama3_vtc_b.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/CAMACI3_Sony_C.jsv", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/CAMACI3_Sony_C.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/camp_mot_fld0_full.26l", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/camp_mot_fld0_full.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/camp_mot_frm0_full.26l", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/camp_mot_frm0_full.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/CAMP_MOT_MBAFF_L30.26l", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/CAMP_MOT_MBAFF_L30.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/camp_mot_picaff0_full.26l", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/camp_mot_picaff0_full.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/CANL1_Sony_E.jsv", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/CANL1_Sony_E.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/CANL2_Sony_E.jsv", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/CANL2_Sony_E.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/CANL3_Sony_C.jsv", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/CANL3_Sony_C.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/CANLMA2_Sony_C.jsv", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/CANLMA2_Sony_C.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/CANLMA3_Sony_C.jsv", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/CANLMA3_Sony_C.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/CAPM3_Sony_D.jsv", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/CAPM3_Sony_D.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/CAQP1_Sony_B.jsv", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/CAQP1_Sony_B.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/CH20_h264.mp4", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/CH20_h264.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/CIP_A_Panasonic_3.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/CIP_A_Panasonic_3.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/cip_B_NEC_3.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/cip_B_NEC_3.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/CVBS3_Sony_C.jsv", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/CVBS3_Sony_C.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/CVCANLMA2_Sony_C.jsv", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/CVCANLMA2_Sony_C.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/CVMA1_Sony_D.jsv", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/CVMA1_Sony_D.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/CVMAPAQP3_Sony_E.jsv", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/CVMAPAQP3_Sony_E.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/CVMAQP3_Sony_D.jsv", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/CVMAQP3_Sony_D.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/cvmp_mot_fld0_full_B.26l", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/cvmp_mot_fld0_full_B.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/CVMP_MOT_FLD_L30_B.26l", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/CVMP_MOT_FLD_L30_B.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/cvmp_mot_frm0_full_B.26l", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/cvmp_mot_frm0_full_B.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/CVMP_MOT_FRM_L31_B.26l", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/CVMP_MOT_FRM_L31_B.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/cvmp_mot_mbaff0_full_B.26l", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/cvmp_mot_mbaff0_full_B.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/cvmp_mot_picaff0_full_B.26l", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/cvmp_mot_picaff0_full_B.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/CVSE3_Sony_H.jsv", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/CVSE3_Sony_H.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/CVSEFDFT3_Sony_E.jsv", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/CVSEFDFT3_Sony_E.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/ENTP_A_Qualcomm_1.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/ENTP_A_Qualcomm_1.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/ENTP_B_Qualcomm_1.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/ENTP_B_Qualcomm_1.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/ENTP_C_Qualcomm_1.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/ENTP_C_Qualcomm_1.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/EXT_A_ericsson_4.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/EXT_A_ericsson_4.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/FI1_Sony_E.jsv", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/FI1_Sony_E.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/FILLER_A_Sony_1.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/FILLER_A_Sony_1.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/foreman_p16x16.264", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/foreman_p16x16.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/FRExt1_Panasonic.avc", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/FRExt1_Panasonic.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/FRExt2_Panasonic.avc", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/FRExt2_Panasonic.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/FRExt3_Panasonic.avc", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/FRExt3_Panasonic.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/FRExt4_Panasonic.avc", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/FRExt4_Panasonic.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/GENERAL_8b_420_RExt_Sony_1.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/GENERAL_8b_420_RExt_Sony_1.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/H264_artifacts_motion.h264", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/H264_artifacts_motion.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/INITQP_A_Sony_1.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/INITQP_A_Sony_1.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/ipcm_A_NEC_3.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/ipcm_A_NEC_3.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/ipcm_B_NEC_3.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/ipcm_B_NEC_3.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/ipcm_C_NEC_3.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/ipcm_C_NEC_3.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/ipcm_D_NEC_3.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/ipcm_D_NEC_3.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/ipcm_E_NEC_2.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/ipcm_E_NEC_2.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/IPRED_A_docomo_2.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/IPRED_A_docomo_2.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/IPRED_B_Nokia_3.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/IPRED_B_Nokia_3.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/IPRED_C_Mitsubishi_3.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/IPRED_C_Mitsubishi_3.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/JM_cqm_cabac.264", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/JM_cqm_cabac.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/JM_cqm_cavlc.264", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/JM_cqm_cavlc.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/JM_defaultmatrix_cabac.264", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/JM_defaultmatrix_cabac.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/JM_defaultmatrix_cavlc.264", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/JM_defaultmatrix_cavlc.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/killer_cuts10f_16x16.264", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/killer_cuts10f_16x16.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/killer_cuts9f_32x32.264", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/killer_cuts9f_32x32.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/LAYERID_A_NOKIA_2.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/LAYERID_A_NOKIA_2.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/LS_A_Orange_2.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/LS_A_Orange_2.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/LS_B_Orange_4.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/LS_B_Orange_4.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/LTRPSPS_A_Qualcomm_1.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/LTRPSPS_A_Qualcomm_1.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/MAXBINS_A_TI_5.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/MAXBINS_A_TI_5.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/MAXBINS_B_TI_5.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/MAXBINS_B_TI_5.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/MAXBINS_C_TI_5.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/MAXBINS_C_TI_5.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/MERGE_A_TI_3.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/MERGE_A_TI_3.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/MERGE_B_TI_3.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/MERGE_B_TI_3.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/MERGE_C_TI_3.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/MERGE_C_TI_3.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/MERGE_D_TI_3.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/MERGE_D_TI_3.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/MERGE_E_TI_3.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/MERGE_E_TI_3.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/MERGE_F_MTK_4.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/MERGE_F_MTK_4.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/MERGE_G_HHI_4.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/MERGE_G_HHI_4.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/MVCLIP_A_qualcomm_3.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/MVCLIP_A_qualcomm_3.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/MVEDGE_A_qualcomm_3.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/MVEDGE_A_qualcomm_3.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/MVHEVCS_A.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/MVHEVCS_A.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/MVHEVCS_B.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/MVHEVCS_B.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/MVHEVCS_E.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/MVHEVCS_E.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/MVHEVCS_F.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/MVHEVCS_F.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/MVHEVCS_G.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/MVHEVCS_G.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/MVHEVCS_H.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/MVHEVCS_H.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/nature_704x576_25Hz_1500kbits.h264", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/nature_704x576_25Hz_1500kbits.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/NL1_Sony_D.jsv", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/NL1_Sony_D.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/NL2_Sony_H.jsv", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/NL2_Sony_H.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/NoOutPrior_A_Qualcomm_1.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/NoOutPrior_A_Qualcomm_1.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/NoOutPrior_B_Qualcomm_1.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/NoOutPrior_B_Qualcomm_1.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/NUT_A_ericsson_5.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/NUT_A_ericsson_5.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/OPFLAG_A_Qualcomm_1.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/OPFLAG_A_Qualcomm_1.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/OPFLAG_B_Qualcomm_1.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/OPFLAG_B_Qualcomm_1.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/OPFLAG_C_Qualcomm_1.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/OPFLAG_C_Qualcomm_1.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/PICSIZE_C_Bossen_1.bin", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/PICSIZE_C_Bossen_1.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/PICSIZE_D_Bossen_1.bin", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/PICSIZE_D_Bossen_1.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/PMERGE_A_TI_3.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/PMERGE_A_TI_3.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/PMERGE_B_TI_3.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/PMERGE_B_TI_3.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/PMERGE_C_TI_3.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/PMERGE_C_TI_3.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/PMERGE_D_TI_3.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/PMERGE_D_TI_3.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/PMERGE_E_TI_3.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/PMERGE_E_TI_3.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/POC_A_Bossen_3.bin", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/POC_A_Bossen_3.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/POC_A_Ericsson_1.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/POC_A_Ericsson_1.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/POC_B_Ericsson_1.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/POC_B_Ericsson_1.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/PPS_A_qualcomm_7.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/PPS_A_qualcomm_7.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/PS_B_VIDYO_3.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/PS_B_VIDYO_3.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/RAP_A_docomo_6.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/RAP_A_docomo_6.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/RAP_B_Bossen_2.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/RAP_B_Bossen_2.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/RPLM_A_qualcomm_4.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/RPLM_A_qualcomm_4.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/RPS_A_docomo_5.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/RPS_A_docomo_5.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/RPS_B_qualcomm_5.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/RPS_B_qualcomm_5.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/RPS_C_ericsson_5.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/RPS_C_ericsson_5.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/RPS_D_ericsson_6.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/RPS_D_ericsson_6.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/RPS_E_qualcomm_5.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/RPS_E_qualcomm_5.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/RPS_F_docomo_2.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/RPS_F_docomo_2.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/RQT_A_HHI_4.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/RQT_A_HHI_4.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/RQT_B_HHI_4.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/RQT_B_HHI_4.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/RQT_C_HHI_4.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/RQT_C_HHI_4.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/RQT_D_HHI_4.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/RQT_D_HHI_4.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/RQT_E_HHI_4.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/RQT_E_HHI_4.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/RQT_F_HHI_4.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/RQT_F_HHI_4.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/RQT_G_HHI_4.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/RQT_G_HHI_4.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/SAO_A_MediaTek_4.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/SAO_A_MediaTek_4.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/SAO_B_MediaTek_5.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/SAO_B_MediaTek_5.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/SAO_C_Samsung_5.bin", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/SAO_C_Samsung_5.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/SAO_D_Samsung_5.bin", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/SAO_D_Samsung_5.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/SAO_E_Canon_4.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/SAO_E_Canon_4.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/SAO_F_Canon_3.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/SAO_F_Canon_3.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/SAO_G_Canon_3.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/SAO_G_Canon_3.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/SDH_A_Orange_4.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/SDH_A_Orange_4.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/Sharp_MP_Field_1_B.jvt", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/Sharp_MP_Field_1_B.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/Sharp_MP_Field_2_B.jvt", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/Sharp_MP_Field_2_B.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/Sharp_MP_Field_3_B.jvt", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/Sharp_MP_Field_3_B.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/Sharp_MP_PAFF_1r2.jvt", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/Sharp_MP_PAFF_1r2.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/Sharp_MP_PAFF_2.jvt", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/Sharp_MP_PAFF_2.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/SIM_B_IDCC_1.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/SIM_B_IDCC_1.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/SLIST_A_Sony_5.bin", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/SLIST_A_Sony_5.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/SLIST_B_Sony_9.bin", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/SLIST_B_Sony_9.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/SLIST_C_Sony_4.bin", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/SLIST_C_Sony_4.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/SLPPLP_A_VIDYO_2.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/SLPPLP_A_VIDYO_2.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/src13_hrc7_525_420_2.264", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/src13_hrc7_525_420_2.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/STRUCT_A_Samsung_7.bin", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/STRUCT_A_Samsung_7.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/STRUCT_B_Samsung_7.bin", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/STRUCT_B_Samsung_7.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/TILES_A_Cisco_2.bin", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/TILES_A_Cisco_2.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/TMVP_A_MS_3.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/TMVP_A_MS_3.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/TSCL_A_VIDYO_5.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/TSCL_A_VIDYO_5.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/TSCL_B_VIDYO_4.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/TSCL_B_VIDYO_4.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/TSKIP_A_MS_3.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/TSKIP_A_MS_3.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/TUSIZE_A_Samsung_1.bin", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/TUSIZE_A_Samsung_1.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/VPSID_A_VIDYO_2.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/VPSID_A_VIDYO_2.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/WP_A_Toshiba_3.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/WP_A_Toshiba_3.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/WP_B_Toshiba_3.bit", "hevc", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/WP_B_Toshiba_3.yuv"),
//             std::make_tuple(std::string(UT_RES_PATH) + "itu_plus/x264_avi.avi", "h264", std::string(UT_RES_OUT) + "itu_plus/yuv_sdk/x264_avi.yuv"),
            
//         }));
//     auto mc_enable = GENERATE(0);
//     auto output_align = GENERATE(0);
//     auto drop = GENERATE(0);
//     auto distort = GENERATE(0);
//     auto decode_mode = GENERATE(0);
    
//     SECTION( "video_dec()" ) {
//         REQUIRE( vid_decode(1, loop, 1, device, (char *)codec.c_str(), (char *)"nv12", vframes, (char *)input.c_str(), (char *)output.c_str(), mc_enable, output_align, drop, distort, decode_mode) == 0 );
//     }
// }

            