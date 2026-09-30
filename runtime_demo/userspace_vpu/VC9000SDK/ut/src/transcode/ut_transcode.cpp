#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
#include "transcode_catch2.c"
#ifdef __cplusplus
}
#endif /* __cplusplus */

#include "catch2/catch.hpp"

// --------------------------------------------------------------------------------------

TEST_CASE( "unitest_transcode_sample_normal_transcode", "[sample][transcode][normal][unitest]" ) {
    std::string input, codec, codec_enc, json_params;
    std::tie(input, codec, codec_enc, json_params) = GENERATE(
        table<std::string, std::string, std::string, std::string>({
            std::make_tuple(std::string(UT_RES_PATH) + "json/transcode/transcode.json", "h264", "hevc", std::string(UT_RES_PATH) + "json/transcode/transcode_video_enc.json"), //second use method
            std::make_tuple(std::string(UT_RES_PATH) + "json/transcode/transcode_multicore.json", "h264", "hevc", std::string(UT_RES_PATH) + "json/transcode/transcode_video_enc.json"),
            std::make_tuple(std::string(UT_RES_PATH) + "json/transcode/transcode_longStream.json", "h264", "hevc", std::string(UT_RES_PATH) + "json/transcode/transcode_video_enc.json"),
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_multicore/cdzj_1080p.h264", "h264", "hevc", std::string(UT_RES_PATH) + "json/transcode/transcode_video_enc.json"),
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_multicore/cdzj_1080p.hevc", "hevc", "h264", std::string(UT_RES_PATH) + "json/transcode/transcode_video_enc.json"),
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_multicore/cdzj_1080p.ivf", "av1", "hevc", std::string(UT_RES_PATH) + "json/transcode/transcode_video_enc.json"),
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_multicore/cdzj_1080p.vp9", "vp9", "jpeg", std::string(UT_RES_PATH) + "json/transcode/transcode_video_enc.json"),
            std::make_tuple(std::string(UT_RES_PATH) + "resolutions_multicore/cdzj_1080p.avs", "avs2", "av1", std::string(UT_RES_PATH) + "json/transcode/transcode_video_enc.json"),
            std::make_tuple(std::string(UT_RES_PATH) + "dec/stream1.jpg", "jpeg", "hevc", std::string(UT_RES_PATH) + "json/transcode/transcode_video_enc.json"),
            
              
    }));
    auto output = GENERATE(std::string(UT_RES_PATH) + "trans_out/"); 
    auto device = GENERATE((char *)"/dev/vastai_video0"); //render device name if you have spicified it in a json file,  this option will be ignored
    auto save = GENERATE(1);
    auto loopp = GENERATE(0);
    auto main_loop = GENERATE(0);
    auto check_md5 = GENERATE(1);
    auto using_ffmpeg = GENERATE(0);
    auto log_period = GENERATE(100);
    auto vframes = GENERATE(1200);
    auto perf_period = GENERATE(1000);
    auto bitDepth = GENERATE(8);
    auto memory_mode = GENERATE(0);
    
    SECTION( "transcode()" ) {
        REQUIRE( transcode((char *)input.c_str(), (char *)output.c_str(), device, loopp, main_loop, save, check_md5,  (char *)codec.c_str(), (char *)codec_enc.c_str(), using_ffmpeg, log_period, perf_period, vframes, bitDepth, memory_mode, (char *)json_params.c_str()) == 0 );
    }
}

            // ### for decode itu only ###
            // std::make_tuple("../../../itu_plus/Aladin.mpg", "h264", "", "../../../itu_plus/yuv_sdk_trans/Aladin.yuv"),
            // std::make_tuple("../../../itu_plus/avchd_lite_14s_playback_speed_problems.MTS", "h264", "", "../../../itu_plus/yuv_sdk_trans/avchd_lite_14s_playback_speed_problems.yuv"),
            // std::make_tuple("../../../itu_plus/BUMPING_A_ericsson_1.bit", "hevc", "", "../../../itu_plus/yuv_sdk_trans/BUMPING_A_ericsson_1.yuv"),
            // std::make_tuple("../../../itu_plus/CABACI3_Sony_B.jsv", "h264", "", "../../../itu_plus/yuv_sdk_trans/CABACI3_Sony_B.yuv"),
            // std::make_tuple("../../../itu_plus/CABAST3_Sony_E.jsv", "h264", "", "../../../itu_plus/yuv_sdk_trans/CABAST3_Sony_E.yuv"),
            // std::make_tuple("../../../itu_plus/CAINIT_G_SHARP_3.bit", "hevc", "", "../../../itu_plus/yuv_sdk_trans/CAINIT_G_SHARP_3.yuv"),
            // std::make_tuple("../../../itu_plus/CAINIT_H_SHARP_3.bit", "hevc", "", "../../../itu_plus/yuv_sdk_trans/CAINIT_H_SHARP_3.yuv"),
            // std::make_tuple("../../../itu_plus/CAMASL3_Sony_B.jsv", "h264", "", "../../../itu_plus/yuv_sdk_trans/CAMASL3_Sony_B.yuv"),
            // std::make_tuple("../../../itu_plus/Campbells_Test_Default.mp4", "h264", "", "../../../itu_plus/yuv_sdk_trans/Campbells_Test_Default.yuv"),
            // std::make_tuple("../../../itu_plus/cathedral-beta2-400extra-crop-avc.mp4", "h264", "", "../../../itu_plus/yuv_sdk_trans/cathedral-beta2-400extra-crop-avc.yuv"),
            // std::make_tuple("../../../itu_plus/CIP_C_Panasonic_2.bit", "hevc", "", "../../../itu_plus/yuv_sdk_trans/CIP_C_Panasonic_2.yuv"),
            // std::make_tuple("../../../itu_plus/CVFI1_Sony_D.jsv", "h264", "", "../../../itu_plus/yuv_sdk_trans/CVFI1_Sony_D.yuv"),
            // std::make_tuple("../../../itu_plus/CVFI2_Sony_H.jsv", "h264", "", "../../../itu_plus/yuv_sdk_trans/CVFI2_Sony_H.yuv"),
            // std::make_tuple("../../../itu_plus/CVNLFI2_Sony_H.jsv", "h264", "", "../../../itu_plus/yuv_sdk_trans/CVNLFI2_Sony_H.yuv"),
            // std::make_tuple("../../../itu_plus/DBLK_A_SONY_3.bit", "hevc", "", "../../../itu_plus/yuv_sdk_trans/DBLK_A_SONY_3.yuv"),
            // std::make_tuple("../../../itu_plus/DBLK_B_SONY_3.bit", "hevc", "", "../../../itu_plus/yuv_sdk_trans/DBLK_B_SONY_3.yuv"),
            // std::make_tuple("../../../itu_plus/DBLK_C_SONY_3.bit", "hevc", "", "../../../itu_plus/yuv_sdk_trans/DBLK_C_SONY_3.yuv"),
            // std::make_tuple("../../../itu_plus/DBLK_D_VIXS_2.bit", "hevc", "", "../../../itu_plus/yuv_sdk_trans/DBLK_D_VIXS_2.yuv"),
            // std::make_tuple("../../../itu_plus/DBLK_E_VIXS_2.bit", "hevc", "", "../../../itu_plus/yuv_sdk_trans/DBLK_E_VIXS_2.yuv"),
            // std::make_tuple("../../../itu_plus/DBLK_F_VIXS_2.bit", "hevc", "", "../../../itu_plus/yuv_sdk_trans/DBLK_F_VIXS_2.yuv"),
            // std::make_tuple("../../../itu_plus/DBLK_G_VIXS_2.bit", "hevc", "", "../../../itu_plus/yuv_sdk_trans/DBLK_G_VIXS_2.yuv"),
            // std::make_tuple("../../../itu_plus/DELTAQP_A_BRCM_4.bit", "hevc", "", "../../../itu_plus/yuv_sdk_trans/DELTAQP_A_BRCM_4.yuv"),
            // std::make_tuple("../../../itu_plus/DELTAQP_B_SONY_3.bit", "hevc", "", "../../../itu_plus/yuv_sdk_trans/DELTAQP_B_SONY_3.yuv"),
            // std::make_tuple("../../../itu_plus/DELTAQP_C_SONY_3.bit", "hevc", "", "../../../itu_plus/yuv_sdk_trans/DELTAQP_C_SONY_3.yuv"),
            // std::make_tuple("../../../itu_plus/dmbts2.ts", "h264", "", "../../../itu_plus/yuv_sdk_trans/dmbts2.yuv"),
            // std::make_tuple("../../../itu_plus/dmbts3.ts", "h264", "", "../../../itu_plus/yuv_sdk_trans/dmbts3.yuv"),
            // std::make_tuple("../../../itu_plus/dmbts4.ts", "h264", "", "../../../itu_plus/yuv_sdk_trans/dmbts4.yuv"),
            // std::make_tuple("../../../itu_plus/dmbts8.ts", "h264", "", "../../../itu_plus/yuv_sdk_trans/dmbts8.yuv"),
            // std::make_tuple("../../../itu_plus/dmbts.ts", "h264", "", "../../../itu_plus/yuv_sdk_trans/dmbts.yuv"),
            // std::make_tuple("../../../itu_plus/DSLICE_A_HHI_5.bin", "hevc", "", "../../../itu_plus/yuv_sdk_trans/DSLICE_A_HHI_5.yuv"),
            // std::make_tuple("../../../itu_plus/DSLICE_B_HHI_5.bin", "hevc", "", "../../../itu_plus/yuv_sdk_trans/DSLICE_B_HHI_5.yuv"),
            // std::make_tuple("../../../itu_plus/DSLICE_C_HHI_5.bin", "hevc", "", "../../../itu_plus/yuv_sdk_trans/DSLICE_C_HHI_5.yuv"),
            // std::make_tuple("../../../itu_plus/envivio-h264.mp4", "h264", "", "../../../itu_plus/yuv_sdk_trans/envivio-h264.yuv"),
            // std::make_tuple("../../../itu_plus/H_264+AAC_A-V_sync.mkv", "h264", "", "../../../itu_plus/yuv_sdk_trans/H_264+AAC_A-V_sync.yuv"),
            // std::make_tuple("../../../itu_plus/h264_bframe_crash2.mp4", "h264", "", "../../../itu_plus/yuv_sdk_trans/h264_bframe_crash2.yuv"),
            // std::make_tuple("../../../itu_plus/h264_bframe_crash.mp4", "h264", "", "../../../itu_plus/yuv_sdk_trans/h264_bframe_crash.yuv"),
            // std::make_tuple("../../../itu_plus/hbc9.avi", "h264", "", "../../../itu_plus/yuv_sdk_trans/hbc9.yuv"),
            // std::make_tuple("../../../itu_plus/hellsing-h264-blocking.mkv", "h264", "", "../../../itu_plus/yuv_sdk_trans/hellsing-h264-blocking.yuv"),
            // std::make_tuple("../../../itu_plus/HRD_A_Fujitsu_3.bin", "hevc", "", "../../../itu_plus/yuv_sdk_trans/HRD_A_Fujitsu_3.yuv"),
            // std::make_tuple("../../../itu_plus/indiana_jones_4-tlr3_h640w.mov", "h264", "", "../../../itu_plus/yuv_sdk_trans/indiana_jones_4-tlr3_h640w.yuv"),
            // std::make_tuple("../../../itu_plus/interlaced_crop.mp4", "h264", "", "../../../itu_plus/yuv_sdk_trans/interlaced_crop.yuv"),
            // std::make_tuple("../../../itu_plus/mbc.ts", "h264", "", "../../../itu_plus/yuv_sdk_trans/mbc.yuv"),
            // std::make_tuple("../../../itu_plus/moonlight1.264", "h264", "", "../../../itu_plus/yuv_sdk_trans/moonlight1.yuv"),
            // std::make_tuple("../../../itu_plus/moonlight.264", "h264", "", "../../../itu_plus/yuv_sdk_trans/moonlight.yuv"),
            // std::make_tuple("../../../itu_plus/MVDL1ZERO_A_docomo_4.bit", "hevc", "", "../../../itu_plus/yuv_sdk_trans/MVDL1ZERO_A_docomo_4.yuv"),
            // std::make_tuple("../../../itu_plus/NeroAVC.mp4", "h264", "", "../../../itu_plus/yuv_sdk_trans/NeroAVC.yuv"),
            // std::make_tuple("../../../itu_plus/qt7_x264.mp4", "h264", "", "../../../itu_plus/yuv_sdk_trans/qt7_x264.yuv"),
            // std::make_tuple("../../../itu_plus/RPLM_B_qualcomm_4.bit", "hevc", "", "../../../itu_plus/yuv_sdk_trans/RPLM_B_qualcomm_4.yuv"),
            // std::make_tuple("../../../itu_plus/saikano-ova-h264-blocking-01.mkv", "h264", "", "../../../itu_plus/yuv_sdk_trans/saikano-ova-h264-blocking-01.yuv"),
            // std::make_tuple("../../../itu_plus/saikano-ova-h264-blocking-02.mkv", "h264", "", "../../../itu_plus/yuv_sdk_trans/saikano-ova-h264-blocking-02.yuv"),
            // std::make_tuple("../../../itu_plus/SAO_H_Parabola_1.bit", "hevc", "", "../../../itu_plus/yuv_sdk_trans/SAO_H_Parabola_1.yuv"),
            // std::make_tuple("../../../itu_plus/SLICES_A_Rovi_3.bin", "hevc", "", "../../../itu_plus/yuv_sdk_trans/SLICES_A_Rovi_3.yuv"),
            // std::make_tuple("../../../itu_plus/sony-hdr-cx-6-avchd-1080i-3-seconds.mts", "h264", "", "../../../itu_plus/yuv_sdk_trans/sony-hdr-cx-6-avchd-1080i-3-seconds.yuv"),
            // std::make_tuple("../../../itu_plus/sony-hdr-cx-6-avchd-1080i-3-seconds-transcoded-x264.mp4", "h264", "", "../../../itu_plus/yuv_sdk_trans/sony-hdr-cx-6-avchd-1080i-3-seconds-transcoded-x264.yuv"),
            // std::make_tuple("../../../itu_plus/sony-hdr-cx-6-avchd-1080i-9-seconds.mts", "h264", "", "../../../itu_plus/yuv_sdk_trans/sony-hdr-cx-6-avchd-1080i-9-seconds.yuv"),
            // std::make_tuple("../../../itu_plus/sony-hdr-cx-6-avchd-1080i-9-seconds-transcoded-x264.mp4", "h264", "", "../../../itu_plus/yuv_sdk_trans/sony-hdr-cx-6-avchd-1080i-9-seconds-transcoded-x264.yuv"),
            // std::make_tuple("../../../itu_plus/str.bin", "hevc", "", "../../../itu_plus/yuv_sdk_trans/str.yuv"),
            // std::make_tuple("../../../itu_plus/tij-h264.avi", "h264", "", "../../../itu_plus/yuv_sdk_trans/tij-h264.yuv"),
            // std::make_tuple("../../../itu_plus/TILES_B_Cisco_1.bin", "hevc", "", "../../../itu_plus/yuv_sdk_trans/TILES_B_Cisco_1.yuv"),
            // std::make_tuple("../../../itu_plus/vss1.264", "h264", "", "../../../itu_plus/yuv_sdk_trans/vss1.yuv"),
            // std::make_tuple("../../../itu_plus/vss.264", "h264", "", "../../../itu_plus/yuv_sdk_trans/vss.yuv"),
            // std::make_tuple("../../../itu_plus/WPP_A_ericsson_MAIN_2.bit", "hevc", "", "../../../itu_plus/yuv_sdk_trans/WPP_A_ericsson_MAIN_2.yuv"),
            // std::make_tuple("../../../itu_plus/WPP_B_ericsson_MAIN_2.bit", "hevc", "", "../../../itu_plus/yuv_sdk_trans/WPP_B_ericsson_MAIN_2.yuv"),
            // std::make_tuple("../../../itu_plus/WPP_C_ericsson_MAIN_2.bit", "hevc", "", "../../../itu_plus/yuv_sdk_trans/WPP_C_ericsson_MAIN_2.yuv"),
            // std::make_tuple("../../../itu_plus/WPP_F_ericsson_MAIN_2.bit", "hevc", "", "../../../itu_plus/yuv_sdk_trans/WPP_F_ericsson_MAIN_2.yuv"),
            // std::make_tuple("../../../itu_plus/x2641.264", "h264", "", "../../../itu_plus/yuv_sdk_trans/x2641.yuv"),
            // std::make_tuple("../../../itu_plus/x264_264.264", "h264", "", "../../../itu_plus/yuv_sdk_trans/x264_264.yuv"),
            // std::make_tuple("../../../itu_plus/x264_Overflow_Sample.mkv", "h264", "", "../../../itu_plus/yuv_sdk_trans/x264_Overflow_Sample.yuv"),