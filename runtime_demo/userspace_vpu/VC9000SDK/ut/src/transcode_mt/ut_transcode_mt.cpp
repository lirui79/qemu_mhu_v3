#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
#include "transcode_mt_catch2.c"
#ifdef __cplusplus
}
#endif /* __cplusplus */

#include "catch2/catch.hpp"

// --------------------------------------------------------------------------------------
// for parms of encoder
char *params_enc[36];
int params_enc_num = 0;

TEST_CASE( "unitest_transcode_mt_sample_normal_transcode", "[sample][transcode_mt][normal][unitest]" ) {
    params_transcode_mt_t params_transcode_mt;
    std::string input, codec, enc_codec, device_name;
    // int memory_mode;
    // enc_codec: default_enc_opts.enc_codec if not json
    //render device name if you have spicified it in a json file,  this option will be ignored
    std::tie(input, codec, enc_codec, device_name, params_transcode_mt.memory_mode) = GENERATE(
        table<std::string, std::string, std::string, std::string, int>({
            std::make_tuple(std::string(UT_RES_PATH) + "json/transcode/transcode.json", "h264", "hevc", "/dev/vastai_video0", 0),
            std::make_tuple(std::string(UT_RES_PATH) + "json/transcode/transcode_multicore.json", "h264", "hevc", "/dev/vastai_video0", 0),
            std::make_tuple(std::string(UT_RES_PATH) + "json/transcode/transcode_longStream.json", "h264", "hevc", "/dev/vastai_video0", 0),
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_multicore/cdzj_1080p.h264", "h264", "hevc", "/dev/vastai_video0", 0),
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_multicore/cdzj_1080p.hevc", "hevc", "h264", "/dev/vastai_video0", 0),
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_multicore/cdzj_1080p.ivf", "av1", "hevc", "/dev/vastai_video0", 0),
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_multicore/cdzj_1080p.vp9", "vp9", "jpeg", "/dev/vastai_video0", 0),
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_multicore/cdzj_1080p.avs", "avs2", "av1", "/dev/vastai_video0", 0),
            std::make_tuple(std::string(UT_RES_PATH) + "dec/stream1.jpg", "jpeg", "hevc", "/dev/vastai_video0", 0),
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_multicore/cdzj_1080p.h264", "h264", "hevc", "/dev/vastai_video0", 4),
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_multicore/cdzj_1080p.hevc", "hevc", "h264", "/dev/vastai_video0", 4),
            std::make_tuple(std::string(UT_RES_PATH) + "dec/stream1.jpg", "jpeg", "hevc", "/dev/vastai_video0", 4),
              
    }));

    params_transcode_mt.params_log.log_into_file = GENERATE(0);
    params_transcode_mt.params_log.log_level = GENERATE(2);
    params_transcode_mt.params_log.log_level_sdk = GENERATE(3);
    params_transcode_mt.params_log.log_level_ffmpeg = GENERATE(3);
    params_transcode_mt.params_log.log_callback = GENERATE(0);
    params_transcode_mt.params_log.enable_error_assert = GENERATE(0);

    std::string output_directory, output_file;
    // One of output_directory, output_file will work, and output_directory has higher priority.
    std::tie(output_directory, output_file) = GENERATE(
        table<std::string, std::string>({
            std::make_tuple(std::string(UT_RES_PATH) + "trans_out/", std::string(UT_RES_PATH) + "trans_out/trans_mt.out"),
    }));

    params_transcode_mt.loop = GENERATE(1);
    params_transcode_mt.main_loop = GENERATE(1);
    params_transcode_mt.save = GENERATE(1);
    params_transcode_mt.check_md5 = GENERATE(1);
    // auto device_name = GENERATE((char *)"/dev/vastai_video0"); // codec must be set when not using ffmpeg!
    params_transcode_mt.using_ffmpeg = GENERATE(0); // using_ffmpeg can not be set when !X86 or no ffmpeg compiled
    params_transcode_mt.log_period = GENERATE(100);
    params_transcode_mt.perf_period = GENERATE(1000);
    params_transcode_mt.vframes = GENERATE(320);
    params_transcode_mt.bitDepth = GENERATE(8);
    // auto memory_mode = GENERATE(4); // just 0, 4 effect !
    params_transcode_mt.thread_count = GENERATE(1, 10); // 1, 10
    params_transcode_mt.dec_output_align = GENERATE(0);
    params_transcode_mt.argc = 1;
    params_transcode_mt.argv = params_enc;

    params_transcode_mt.input = (char *)input.c_str();
    params_transcode_mt.codec = (char *)codec.c_str();
    params_transcode_mt.enc_codec = (char *)enc_codec.c_str();
    params_transcode_mt.device_name = (char *)device_name.c_str();
    params_transcode_mt.output_directory = (char *)output_directory.c_str();
    params_transcode_mt.output_file = (char *)output_file.c_str();
    
    SECTION( "transcode_mt()" ) {
        REQUIRE( transcode_mt(&params_transcode_mt) == 0);
    }
}

int files_num = 0;
TEST_CASE( "unitest_transcode_mt_sample_decorenc_transcode", "[sample][transcode_mt][dec_or_enc_only][unitest]" ) {
    params_transcode_mt_t params_transcode_mt;
    std::string input, codec;

    params_transcode_mt.params_log.log_into_file = GENERATE(0);
    params_transcode_mt.params_log.log_level = GENERATE(2);
    params_transcode_mt.params_log.log_level_sdk = GENERATE(3);
    params_transcode_mt.params_log.log_level_ffmpeg = GENERATE(3);
    params_transcode_mt.params_log.log_callback = GENERATE(0);
    params_transcode_mt.params_log.enable_error_assert = GENERATE(0);

    params_transcode_mt.loop = GENERATE(1);
    params_transcode_mt.main_loop = GENERATE(1);
    params_transcode_mt.save = GENERATE(1);
    params_transcode_mt.check_md5 = GENERATE(1);
    // auto device_name = GENERATE((char *)"/dev/vastai_video0"); // codec must be set when not using ffmpeg!
    params_transcode_mt.using_ffmpeg = GENERATE(0); // using_ffmpeg can not be set when !X86 or no ffmpeg compiled
    params_transcode_mt.log_period = GENERATE(100);
    params_transcode_mt.perf_period = GENERATE(1000);
    params_transcode_mt.vframes = GENERATE(20);
    params_transcode_mt.bitDepth = GENERATE(8);
    params_transcode_mt.memory_mode = GENERATE(0);
    params_transcode_mt.thread_count = GENERATE(1); // 10
    params_transcode_mt.dec_output_align = GENERATE(0);

    SECTION( "transcode_mt_params_1080p()" ) {
        // enc_codec: default_enc_opts.enc_codec if not json
        std::tie(input, codec, params_transcode_mt.enc_codec) = GENERATE(
            table<std::string, std::string, char *>({
                std::make_tuple(std::string(UT_RES_PATH) + "resolutions_multicore/cdzj_1080p.h264", "h264", (char *)NULL),
                std::make_tuple(std::string(UT_RES_PATH) + "resolutions_multicore/cdzj_1080p.hevc", "hevc", (char *)NULL),
                std::make_tuple(std::string(UT_RES_PATH) + "resolutions_multicore/cdzj_1080p.ivf", "av1", (char *)NULL),
                std::make_tuple(std::string(UT_RES_PATH) + "resolutions_multicore/cdzj_1080p.vp9", "vp9", (char *)NULL),
                std::make_tuple(std::string(UT_RES_PATH) + "resolutions_multicore/cdzj_1080p.avs", "avs2", (char *)NULL),
                std::make_tuple(std::string(UT_RES_PATH) + "cdzj_1080p_nv12.jpg", "jpeg", (char *)NULL),
                std::make_tuple(std::string("/opt/funcs_align_src/1080p_nv12_100.yuv"), "yuv", (char *)"h264"),
                std::make_tuple(std::string("/opt/funcs_align_src/1080p_nv12_100.yuv"), "yuv", (char *)"hevc"),
                std::make_tuple(std::string("/opt/funcs_align_src/1080p_nv12_100.yuv"), "yuv", (char *)"av1"),
                std::make_tuple(std::string("/opt/funcs_align_src/1080p_nv12_100.yuv"), "yuv", (char *)"jpeg"),
                
        }));
        auto width = GENERATE(1920);
        auto height = GENERATE(1080);
        auto pixelFormat = GENERATE("nv12");
        auto separate_luma_chroma = GENERATE(0, 1);

        char params_arr[36][8];
        int i = 0, params_i = 0;
        params_enc[i++] = (char *)"transcode_mt";

        sprintf(params_arr[params_i++], "%d", width);
        params_enc[i++] = (char *)"--width"; params_enc[i++] = params_arr[params_i-1];

        sprintf(params_arr[params_i++], "%d", height);
        params_enc[i++] = (char *)"--height"; params_enc[i++] = params_arr[params_i-1];

        sprintf(params_arr[params_i++], "%s", pixelFormat);
        params_enc[i++] = (char *)"--pixelFormat"; params_enc[i++] = params_arr[params_i-1];

        sprintf(params_arr[params_i++], "%d", separate_luma_chroma);
        params_enc[i++] = (char *)"--separateLumaChroma"; params_enc[i++] = params_arr[params_i-1];

        printf("num of all params_enc: %d;\n", params_enc_num = i);
        for (int i=0; i<params_enc_num; i++)
            printf("%s ", params_enc[i]);

        std::string output_directory, output_file;
        // One of output_directory, output_file will work, and output_directory has higher priority.
        std::tie(output_directory, output_file) = GENERATE(
            table<std::string, std::string>({
                std::make_tuple(std::string(UT_RES_PATH) + "trans_out/", std::string(UT_RES_PATH) + "trans_out/trans_mt_" + std::to_string(++files_num) + ".hxxx"),
                
        }));
        printf("\n======= file_num: %d\n", files_num);
        printf("output_file: %s; \n", (char *)output_file.c_str());

        params_transcode_mt.argc = params_enc_num;
        params_transcode_mt.argv = params_enc;
        
        params_transcode_mt.input = (char *)input.c_str();
        params_transcode_mt.codec = (char *)codec.c_str();
        params_transcode_mt.device_name = NULL;
        params_transcode_mt.output_directory = (char *)output_directory.c_str();
        params_transcode_mt.output_file = (char *)output_file.c_str();

        if (separate_luma_chroma==1) { // separate_luma_chroma Only effective when 'decMemoryMode' is 4, decMemoryMode 4 Only effective when save 0
            params_transcode_mt.memory_mode = 4;
            params_transcode_mt.save = 0;
        }
        REQUIRE( transcode_mt(&params_transcode_mt) == 0);
    }

    SECTION( "transcode_mt_params_8k()" ) {
        int width, height;
        // enc_codec: default_enc_opts.enc_codec if not json
        std::tie(input, width, height, codec, params_transcode_mt.enc_codec) = GENERATE(
            table<std::string, int, int, std::string, char *>({
                std::make_tuple(std::string(UT_RES_PATH) + "resolutions_multicore/cdzj_7680x4320.h264", 7680, 4320, "h264", (char *)NULL),
                std::make_tuple(std::string(UT_RES_PATH) + "resolutions_multicore/cdzj_7680x4320.hevc", 7680, 4320, "hevc", (char *)NULL),
                std::make_tuple(std::string(UT_RES_PATH) + "resolutions_multicore/cdzj_7680x4320.ivf", 7680, 4320, "av1", (char *)NULL),
                std::make_tuple(std::string(UT_RES_PATH) + "resolutions_multicore/cdzj_7680x4320_vp9.ivf", 7680, 4320, "vp9", (char *)NULL),
                std::make_tuple(std::string(UT_RES_PATH) + "resolutions_multicore/cdzj_7680x4320.avs", 7680, 4320, "avs2", (char *)NULL),
                std::make_tuple(std::string(UT_RES_PATH) + "resolutions_jpeg/7680x4320.jpeg", 7680, 4320, "jpeg", (char *)NULL),
                std::make_tuple(std::string("/opt/funcs_align_src/cdzj_multicore_8k.yuv"), 7680, 4320, "yuv", (char *)"h264"),
                std::make_tuple(std::string("/opt/funcs_align_src/cdzj_multicore_8k.yuv"), 7680, 4320, "yuv", (char *)"hevc"),
                std::make_tuple(std::string("/opt/funcs_align_src/3840x2160.yuv"), 3840, 2160, "yuv", (char *)"av1"),
                std::make_tuple(std::string("/opt/funcs_align_src/cdzj_multicore_8k.yuv"), 7680, 4320, "yuv", (char *)"jpeg"),
                
        }));
        auto pixelFormat = GENERATE("nv12");

        char params_arr[36][8];
        int i = 0, params_i = 0;
        params_enc[i++] = (char *)"transcode_mt";

        sprintf(params_arr[params_i++], "%d", width);
        params_enc[i++] = (char *)"--width"; params_enc[i++] = params_arr[params_i-1];

        sprintf(params_arr[params_i++], "%d", height);
        params_enc[i++] = (char *)"--height"; params_enc[i++] = params_arr[params_i-1];

        sprintf(params_arr[params_i++], "%s", pixelFormat);
        params_enc[i++] = (char *)"--pixelFormat"; params_enc[i++] = params_arr[params_i-1];

        printf("num of all params_enc: %d;\n", params_enc_num = i);
        for (int i=0; i<params_enc_num; i++)
            printf("%s ", params_enc[i]);

        std::string output_directory, output_file;
        // One of output_directory, output_file will work, and output_directory has higher priority.
        std::tie(output_directory, output_file) = GENERATE(
            table<std::string, std::string>({
                std::make_tuple(std::string(UT_RES_PATH) + "trans_out/", std::string(UT_RES_PATH) + "trans_out/trans_mt_" + std::to_string(++files_num) + ".hxxx"),
                
        }));
        printf("\n======= file_num: %d\n", files_num);
        printf("output_file: %s; \n", (char *)output_file.c_str());

        params_transcode_mt.argc = params_enc_num;
        params_transcode_mt.argv = params_enc;
        
        params_transcode_mt.input = (char *)input.c_str();
        params_transcode_mt.codec = (char *)codec.c_str();
        params_transcode_mt.device_name = NULL;
        params_transcode_mt.output_directory = (char *)output_directory.c_str();
        params_transcode_mt.output_file = (char *)output_file.c_str();
        REQUIRE( transcode_mt(&params_transcode_mt) == 0);
    }
}

TEST_CASE( "unitest_transcode_mt_sample_params2argv_transcode", "[sample][transcode_mt][params2argv][unitest]" ) {
    params_transcode_mt_t params_transcode_mt;
    std::string input, codec, enc_codec;
    // enc_codec: default_enc_opts.enc_codec if not json
    //render device name if you have spicified it in a json file,  this option will be ignored
    std::tie(input, codec, enc_codec) = GENERATE(
        table<std::string, std::string, std::string>({
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_multicore/cdzj_1080p.h264", "h264", "hevc"),
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_multicore/cdzj_1080p.hevc", "hevc", "h264"),
            // std::make_tuple(std::string(UT_RES_PATH) + "resolutions_multicore/cdzj_1080p.hevc", "hevc", "h264", "/dev/vastai_video0"),
            // std::make_tuple(std::string(UT_RES_PATH) + "resolutions_multicore/cdzj_1080p.ivf", "av1", "hevc", "/dev/vastai_video0"),
            // std::make_tuple(std::string(UT_RES_PATH) + "resolutions_multicore/cdzj_1080p.vp9", "vp9", "jpeg", "/dev/vastai_video0"),
            // std::make_tuple(std::string(UT_RES_PATH) + "resolutions_multicore/cdzj_1080p.avs", "avs2", "av1", "/dev/vastai_video0"),
            // std::make_tuple(std::string(UT_RES_PATH) + "dec/stream1.jpg", "jpeg", "hevc", "/dev/vastai_video0"),
              
    }));

    params_transcode_mt.params_log.log_into_file = GENERATE(0);
    params_transcode_mt.params_log.log_level = GENERATE(2);
    params_transcode_mt.params_log.log_level_sdk = GENERATE(3);
    params_transcode_mt.params_log.log_level_ffmpeg = GENERATE(3);
    params_transcode_mt.params_log.log_callback = GENERATE(0);
    params_transcode_mt.params_log.enable_error_assert = GENERATE(0);

    params_transcode_mt.loop = GENERATE(1);
    params_transcode_mt.main_loop = GENERATE(1);
    params_transcode_mt.save = GENERATE(1);
    params_transcode_mt.check_md5 = GENERATE(1);
    // auto device_name = GENERATE((char *)"/dev/vastai_video0"); // codec must be set when not using ffmpeg!
    params_transcode_mt.using_ffmpeg = GENERATE(0); // using_ffmpeg can not be set when !X86 or no ffmpeg compiled
    params_transcode_mt.log_period = GENERATE(100);
    params_transcode_mt.perf_period = GENERATE(1000);
    params_transcode_mt.vframes = GENERATE(120);
    params_transcode_mt.bitDepth = GENERATE(8);
    params_transcode_mt.memory_mode = GENERATE(0);
    params_transcode_mt.thread_count = GENERATE(3); // 10
    params_transcode_mt.dec_output_align = GENERATE(0);

    SECTION( "transcode_mt_params()" ) {
        
        auto rcMode = GENERATE(2, 4);
        auto decCrop = GENERATE(1, 3);
        auto decMode = GENERATE(0, 1);
        auto decNoOutputReordering = GENERATE(0);
        int multiDevice;  // default:0, should never used on sg100
        int randomDevice; // whether enable transcoding using random-device
        std::tie(multiDevice, randomDevice) = GENERATE(
            table<int, int>({
                std::make_tuple(0, 1),
                std::make_tuple(1, 0),
        }));
        // auto multicore = GENERATE(0, 1);    // enable multi-core encoding or not(1/0)
        auto decApiMode = GENERATE(0, 1);   // decoder api mode, default:0(parallel), 1-serial
        auto multiRuntime = GENERATE(1); // every channel with a runtime instance
        auto uniqueOutputFile = GENERATE(0); // whether save different resolution data into one single file, default:0
        auto maxQueuedFrame = GENERATE(3);  // max frame number in queue for encoder, default:3, only effective when decApiMode==serial
        auto maxQueuedStream = GENERATE(2); // max stream number in queue for encoder output thread, default:2, only effective when anotherThread4EncOut is enabled
        // auto outputCuInfo = GENERATE(0, 1);
        // auto parseCuInfo = GENERATE(0, 1);
        auto saveCuInfo = GENERATE(0); // 1 vemcu not update now !!!
        auto anotherThread4EncOut = GENERATE(0, 1);    // using a separate thread to save encoded data/cuinfo, default:0
        // auto forceHostBuffer = GENERATE(0, 1);Deprecated // force decoder output to host buffer, default:0, can't be used when decMemoryMode is 4(vmpp_DEC_MEM_USER_AS_HWOUT)"
        auto targetFPS = GENERATE(180);
        auto collectLatency = GENERATE(1);

        auto dec_recv_mem_type= GENERATE(0, 1); // To compatible with vmppMemoryType
        

        char params_arr[36][8];
        int i = 0, params_i = 0;
        params_enc[i++] = (char *)"transcode_mt";

        // enc option
        sprintf(params_arr[params_i++], "%d", rcMode);
        params_enc[i++] = (char *)"--rcMode"; params_enc[i++] = params_arr[params_i-1];

        // dec option
        sprintf(params_arr[params_i++], "%d", decCrop);
        params_enc[i++] = (char *)"--decCrop"; params_enc[i++] = params_arr[params_i-1];
        sprintf(params_arr[params_i++], "%d", decMode);
        params_enc[i++] = (char *)"--decMode"; params_enc[i++] = params_arr[params_i-1];
        sprintf(params_arr[params_i++], "%d", decNoOutputReordering);
        params_enc[i++] = (char *)"--decNoOutputReordering"; params_enc[i++] = params_arr[params_i-1];

        // other
        sprintf(params_arr[params_i++], "%d", multiDevice);
        params_enc[i++] = (char *)"--multiDevice"; params_enc[i++] = params_arr[params_i-1];
        sprintf(params_arr[params_i++], "%d", randomDevice);
        params_enc[i++] = (char *)"--randomDevice"; params_enc[i++] = params_arr[params_i-1];
        // sprintf(params_arr[params_i++], "%d", multicore);
        // params_enc[i++] = (char *)"--multicore"; params_enc[i++] = params_arr[params_i-1];
        sprintf(params_arr[params_i++], "%d", decApiMode);
        params_enc[i++] = (char *)"--decApiMode"; params_enc[i++] = params_arr[params_i-1];
        sprintf(params_arr[params_i++], "%d", multiRuntime);
        params_enc[i++] = (char *)"--multiRuntime"; params_enc[i++] = params_arr[params_i-1];
        sprintf(params_arr[params_i++], "%d", uniqueOutputFile);
        params_enc[i++] = (char *)"--uniqueOutputFile"; params_enc[i++] = params_arr[params_i-1];
        sprintf(params_arr[params_i++], "%d", maxQueuedFrame);
        params_enc[i++] = (char *)"--maxQueuedFrame"; params_enc[i++] = params_arr[params_i-1];
        sprintf(params_arr[params_i++], "%d", maxQueuedStream);
        params_enc[i++] = (char *)"--maxQueuedStream"; params_enc[i++] = params_arr[params_i-1];
        // sprintf(params_arr[params_i++], "%d", outputCuInfo);
        // params_enc[i++] = (char *)"--outputCuInfo"; params_enc[i++] = params_arr[params_i-1];
        // sprintf(params_arr[params_i++], "%d", parseCuInfo);
        // params_enc[i++] = (char *)"--parseCuInfo"; params_enc[i++] = params_arr[params_i-1];
        sprintf(params_arr[params_i++], "%d", saveCuInfo);
        params_enc[i++] = (char *)"--saveCuInfo"; params_enc[i++] = params_arr[params_i-1];
        sprintf(params_arr[params_i++], "%d", anotherThread4EncOut);
        params_enc[i++] = (char *)"--anotherThread4EncOut"; params_enc[i++] = params_arr[params_i-1];
        // sprintf(params_arr[params_i++], "%d", forceHostBuffer); Deprecated
        // params_enc[i++] = (char *)"--forceHostBuffer"; params_enc[i++] = params_arr[params_i-1];
        sprintf(params_arr[params_i++], "%d", targetFPS);
        params_enc[i++] = (char *)"--targetFPS"; params_enc[i++] = params_arr[params_i-1];
        sprintf(params_arr[params_i++], "%d", collectLatency);
        params_enc[i++] = (char *)"--collectLatency"; params_enc[i++] = params_arr[params_i-1];
        sprintf(params_arr[params_i++], "%d", dec_recv_mem_type);
        params_enc[i++] = (char *)"--decRecvMemoryType"; params_enc[i++] = params_arr[params_i-1];

        printf("num of all params_enc: %d;\n", params_enc_num = i);
        for (int i=0; i<params_enc_num; i++)
            printf("%s ", params_enc[i]);

        std::string output_directory, output_file;
        // One of output_directory, output_file will work, and output_directory has higher priority.
        std::tie(output_directory, output_file) = GENERATE(
            table<std::string, std::string>({
                std::make_tuple(std::string(UT_RES_PATH) + "trans_out/", std::string(UT_RES_PATH) + "trans_out/trans_mt_" + std::to_string(++files_num) + ".hxxx"),
                
        }));
        printf("======= file_num: %d\n", files_num);
        printf("output_file: %s; \n", (char *)output_file.c_str());

        params_transcode_mt.argc = params_enc_num;
        params_transcode_mt.argv = params_enc;
        
        params_transcode_mt.input = (char *)input.c_str();
        params_transcode_mt.codec = (char *)codec.c_str();
        params_transcode_mt.enc_codec = (char *)enc_codec.c_str();
        params_transcode_mt.device_name = NULL;
        params_transcode_mt.output_directory = (char *)output_directory.c_str();
        params_transcode_mt.output_file = (char *)output_file.c_str();
        REQUIRE( transcode_mt(&params_transcode_mt) == 0);
    }
}
