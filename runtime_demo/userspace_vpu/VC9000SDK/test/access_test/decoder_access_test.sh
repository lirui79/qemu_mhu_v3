#!/bin/bash

#source ./export.sh
cur_dir=`pwd`
#indir=/opt/vastai/access_test/decode_input/
indir=/home/vastai/resource/dataset/decoder_access_test/
#indir=/data/resource/decoder_access_test/
out_dir=$cur_dir/out/decode_out
> error_log_dec.txt

mkdir -p $out_dir

# platform: sv100 or sg100, default: sv100
platform=$1
# render_mode default: /dev/vastai_video0
render_node=$2

#decode_md5=("0b808986c001439350f3f5fb9c5ee3e2"
#            "b0bd7e4c9542eaadd415b8b67962d0c4"
#        "1dd3d17d7ae6759cb37f867257f29e68"
#        "ac9d5d91a96d0532c577a75051a0fc19"
#        )

decode_md5=(
        "0b808986c001439350f3f5fb9c5ee3e2"    #0
        "78ae12671e49f22a47e26b1c71f93144"    #1
        "5247dcd0edb6312133b0068d132a689f"    #2
        "87b00a6b3ae108570221e2ad4feb55eb"    #3
        "1699687e20b959323e950ca62975a470"    #4
        "b25ae8d135370a3eb70796c3f9dae07d"    #5
        "ccaa72175ae2230c854b16e18c21fea0"    #6
        "c16f0944e159c89fa558448b077fcc82"    #7

        "db3be78ddec047c0327b98d39bdad7fa"    #8
        "e163acbc68d0a30fc776539f4b2c2061"    #9
        "42d8d6f3b974b1778e345151064a3d18"    #10

        # add 480P
        "580ebfa984a0cfa867d82c7beda81442"    #11
        "588904aa07ba8602616bd6e9fb1e3959"    #12
        "af6807f5885af45905ea6c3b0e686d5f"    #13

        # multi-slice
        "edc2a72fad47eaf210068691f4f4674d"    #14
        "10c40af9a2317e388dbd1c00b90b0bf6"    #15

        # add VP9
        "ec241a8cb0c4b400cdb940c3840be32a"    #16
        "4eced115494a621aa3b60a6f74e2c072"    #17
        "890644193307bc871c39ec413ffdd44b"    #18
        "7639fe2b0ac83f4f7efaaa011e5305de"    #19

        # add avs2
        "4de97763c222a1816fac5e88aac73ccc"    #20
        "13cbe002236b9175bfbb8f17f9961ea5"    #21
        "642478be92a36fc55768c3f97189868d"    #22
        "2fbaffe07a951067c9aa84fd5228ee93"    #23

        #crop
        "b60aa21be11c2d012f4971c703f63120"    #24
        "995f931cd16d202719abc0e50668a6b8"    #25
        )


function run_single_test(){

    input_file=$1
    res=$2
    dec_type=$3
    md5_index=$4

    out_name=$out_dir/out.yuv

    echo "<<< $md5_index:$dec_type decode: $input_file >>>"
    echo

    #ffmpeg -loglevel info -hwaccel:v vaapi -hwaccel_device:v  $render_node -i $input_file  -y  $out_name
    #../../sample/build/out/video_dec -i $input_file -o $out_name -d $render_node -c $dec_type -s 1 -m 1 -l 1
    ../../sample/build/out/transcode_mt -i $input_file -o $out_name -s 1 -r $render_node -f 0 -c $dec_type #-u 0

    hw_md5=`md5sum $out_name`
    hw_md5=${hw_md5:0:32}

    sw_md5=${decode_md5[$md5_index]}

    echo " "
    echo "hw_md5=$hw_md5, sw_md5=$sw_md5"

    echo " "
    if [ "$hw_md5" != "$sw_md5" ] ; then
        echo "$input_file result mismatch, failed!!!"
        # Logging error messages to variables or files
        echo "err: md5index=$md5index, $input_file (dec_type: $dec_type) hw_md5=$hw_md5, sw_md5=$sw_md5" >> error_log_dec.txt
        return 1
    else
        echo "result match， success!!!"
        rm -rf $out_name
        return 0
    fi
}

function run_single_decode_test(){

    input_file=$1
    res=$2
    dec_type=$3
    crop_mode=$4
    md5_index=$5

    out_name=$out_dir/out.yuv

    echo "<<< $md5_index:$dec_type decode: $input_file >>>"
    echo

    #ffmpeg -loglevel info -hwaccel:v vaapi -hwaccel_device:v  $render_node -i $input_file  -y  $out_name
    #../../sample/build/out/video_dec -i $input_file -o $out_name -d $render_node -c $dec_type -s 1 -m 1 -l 1
    ../../sample/build/out/video_dec -i $input_file -o $out_name -s 1 -d $render_node -c $dec_type -P $crop_mode #-u 0

    hw_md5=`md5sum $out_name`
    hw_md5=${hw_md5:0:32}

    sw_md5=${decode_md5[$md5_index]}

    echo " "
    echo "hw_md5=$hw_md5, sw_md5=$sw_md5"

    echo " "
    if [ "$hw_md5" != "$sw_md5" ] ; then
        echo "$input_file result mismatch, failed!!!"
        # Logging error messages to variables or files
        echo "err: md5index=$md5index, $input_file (dec_type: $dec_type) hw_md5=$hw_md5, sw_md5=$sw_md5" >> error_log_dec.txt
        return 1
    else
        echo "result match， success!!!"
        rm -rf $out_name
        return 0
    fi
}

function run_multi_process_test(){

    input_file=$1
    dec_type=$2
    out_name="none"


    for i in $(seq 1 20)
    do
        #ffmpeg -loglevel info -hwaccel:v vaapi -hwaccel_output_format vaapi -hwaccel_device:v  $render_node  -i $input_file  -f null 0 &
        #../../build/out/video_dec -i $input_file -o $out_name -d /dev/vastai_renderD0 -c h264 -s 0 -m 0 -l 1
        if [ "$dec_type"x = "jpeg"x ] ; then
            ../../sample/build/out/transcode_mt -i $input_file -o $out_name -s 0 -r $render_node -l 1000 -f 0 -c $dec_type &
        else
            ../../sample/build/out/transcode_mt -i $input_file -o $out_name -s 0 -r $render_node -f 0 -c $dec_type &
        fi
    done

    count=0
    while [ 1 ]
    do
        result=`ps -ef |grep  "transcode_mt " |grep -v grep`

        if [ $count -gt 120 ] ; then
            echo " "
            echo "run 20 channel decode test failed!!!"
            echo " "
            # Logging error messages to variables or files
            echo "err: $input_file (dec_type: $dec_type) run 20 channel decode test failed!!!" >> error_log_dec.txt
            return 1
        fi

            if [ -z "$result" ]; then
                echo " "
                echo "run 20 channel $dec_type decode test success!!!"
                echo " "
                break
            else
                #echo "waiting for ffmpeg exit!!!"
                let count+=1
                sleep 1
            fi
    done
}

function run_multi_thread_test(){
    input_file=$1
    dec_type=$2
    out_name="none"

    if [ "$platform"x = "sv100"x ] ; then
	if [ "$dec_type"x = "avs2"x ] ; then
            thread_num=50
	else
	    thread_num=100
	fi
    else
        thread_num=28
    fi

    if [ "$dec_type"x = "jpeg"x ] ; then
        ../../sample/build/out/jpeg_dec_mt  $input_file ./test.yuv $render_node  $thread_num || {
            echo "Error: jpeg decode failed!"
            # Logging error messages to variables or files
            echo "err: $input_file (dec_type: $dec_type) run 20 channel decode test failed!!!" >> error_log_dec.txt
            return 1
        }
    else
        ../../sample/build/out/video_dec_mt -i $input_file  -o $out_dir -d $render_node -c $dec_type -s 0 -l 1 -T $thread_num || {
            echo "Error: $dec_type decode failed!"
            # Logging error messages to variables or files
            echo "err: $input_file (dec_type: $dec_type) run 20 channel decode test failed!!!" >> error_log_dec.txt
            return 1
        }
    fi

    echo "run $thread_num thread $dec_type decode test success!!!"

}

function main(){
    if [ -z $render_node ];then
        render_node="/dev/vastai_video0"
    fi

    if [ -z $platform ];then
        platform="sv100"
    fi

    #480P  h264/hevc/av1/vp9/avs2

    run_single_test $indir/AndroidInSpace.480p.mq.h265 480p hevc 11
    run_single_test $indir/AndroidInSpace.480p.mq.h264 480p h264 12
    if [ "$platform"x = "sv100"x ] ; then
        run_single_test $indir/videodecode_852x480.ivf 480p av1 13
    fi
    run_single_test $indir/BasketballDrill_832x480_400_50.ivf 480p vp9 16
    run_single_test $indir/Quant_7.1_WHU_4_2.avs 480p avs2 20

    #720P  h264/hevc/av1/vp9/avs2
    run_single_test $indir/stream-720p-200f.hevc 720p hevc 0
    run_single_test $indir/wurenqu-720p-h264-0-9.h264 720p h264 1
    if [ "$platform"x = "sv100"x ] ; then
        run_single_test $indir/videodecode_720x1280.ivf 720p av1 2
    fi
    run_single_test $indir/FourPeople_1280x720_200_60.ivf 720p vp9 17
    run_single_test $indir/Motion_5.1_PKU_1_2.avs 720p avs2 21

    # 1080P h264/hevc/av1/vp9/avs2
    run_single_test $indir/cdzj-hevc-0-6.hevc 1080p hevc 3
    run_single_test $indir/tlst_1080p-h264-0-10.h264 1080p h264 4
    if [ "$platform"x = "sv100"x ] ; then
        run_single_test $indir/ParkScene_1920x1080_24_8M.ivf 1080p av1 5
    fi
    run_single_test $indir/Kimono1_1920x1080_100_24.ivf 1080p vp9 18
    run_single_test $indir/BBV_13.1_UESTC_1_1.avs 1080p avs2 22

    # hw crop
    run_single_decode_test $indir/Kimono1_crop_top_offset.h264 1080p h264 1 24
    run_single_decode_test $indir/xg1918x1078_crop_top_offset.hevc 1080p hevc 1 25

    # 4K h264/hevc/av1/vp9/avs2
    run_single_test $indir/Kimono1_4096x4096_24.h265 4K hevc 8
    run_single_test $indir/Kimono1_4096x4096_24.h264 4K h264 9
    if [ "$platform"x = "sv100"x ] ; then
        run_single_test $indir/videodecode_4096x3112.ivf 4K av1 10
    fi
    run_single_test $indir/shanghai_bund_3840x2160.ivf 4K vp9 19
    run_single_test $indir/kaoya_3840x2160_libxavs2.avs 4K avs2 23

    # small size     jpeg
    run_single_test $indir/alps_1202x676_yuv420.jpg  special jpeg 6
    # big size(32K)  jpeg
    run_single_test $indir/16384_part1_32768x32768.jpg 32K jpeg 7

    # multi-slice
    run_single_test $indir/t800_720p_264bp_assist_100.264 720p h264 14
    run_single_test $indir/t800_1080p_264bp_main_100.264 1080p h264 15

    # multi-process test  (20 processes)
    run_multi_process_test $indir/tlst_1080p-h264-0-10.h264 h264
    sleep 2
    run_multi_process_test $indir/cdzj-hevc-0-6.hevc hevc
    sleep 2
    if [ "$platform"x = "sv100"x ] ; then
        run_multi_process_test $indir/ParkScene_1920x1080_24_8M.ivf av1
        sleep 2
    fi
    run_multi_process_test $indir/Kimono1_1920x1080_100_24.ivf vp9
    sleep 2
    run_multi_process_test $indir/BasketballDrive_1920x1088_RA.avs avs2
    sleep 2

    run_multi_process_test $indir/alps_1202x676_yuv420.jpg jpeg
    sleep 2

    # multi-thread test (100 threads)
    run_multi_thread_test $indir/xg1920x1088_x264_10.h264 h264
    sleep 2
    run_multi_thread_test $indir/xg1920x1088_x264_10.h265 hevc
    sleep 2
    if [ "$platform"x = "sv100"x ] ; then
        run_multi_thread_test $indir/xg1920x1088_10.ivf av1
        sleep 2
    fi
    run_multi_thread_test $indir/Kimono1_1920x1080_100_24.ivf vp9
    sleep 2
    run_multi_thread_test $indir/BasketballDrive_1920x1088_RA.avs avs2
    sleep 2

    run_multi_thread_test $indir/alps_1202x676_yuv420.jpg jpeg

    if [ -s error_log_dec.txt ]; then
        echo " "
        echo "error_log_dec.txt is generated and not empty, read it!"
        cat error_log_dec.txt
    elif [ -f error_log_dec.txt ]; then
        echo " "
        echo "Pass all decodec test cases!"
    fi
    exit 0
}

main

