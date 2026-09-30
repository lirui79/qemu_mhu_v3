#!/bin/bash
dev_list=(/dev/vastai_video0 /dev/vastai_video1)
VIDEO_DATASET_PATH="/home/vastai/resource/dataset/jctvc"
yuvtype=(yuv420p nv12)
threadnum_param=10
codec_list=(0 1) #0 for h264, 1 for hevc
if [ -f "cmd.log" ];then
    rm cmd.log
fi
parallel=5
echo "start" >> cmd.log
for i in $(seq 1 $parallel)
do
    echo "paralle $i >>>" >> cmd.log
    for type in ${yuvtype[@]}
    do
        echo "yuv $type >>>>>>" >> cmd.log
        case $type in 
        "yuv420p")
            array=(BasketballDrill_832x480_50.yuv BasketballPass_416x240_50.yuv BQMall_832x480_60.yuv ChinaSpeed_1024x768_30.yuv PartyScene_832x480_50.yuv vidyo1_1280x720_60.yuv)
            width_array=(832 416 832 1024 832 1280)
            height_array=(480 240 480 768 480 720)
            video_path=${VIDEO_DATASET_PATH}/yuv420p
        ;;
        "nv12")
            array=(Tennis_1000x800_24.yuv Tennis_1024x576_24.yuv Tennis_1024x600_24.yuv Tennis_1024x640_24.yuv Tennis_1080x1920_24.yuv Tennis_1152x768_24.yuv Tennis_1280x720_24.yuv Tennis_1900x1000_24.yuv Tennis_720x1280_24.yuv Tennis_960x540_24.yuv)
            width_array=(1000 1024 1024 1024 1080 1152 1280 1900 720  960)
            height_array=(800 576  600  640  1920 768  720  1000 1280 540)
            video_path=${VIDEO_DATASET_PATH}/nv12
        ;;
        esac

        for i in ${array[@]} 
        do
            echo "file $i >>>>>>>>>" >> cmd.log
            echo "$SOURCE_STREAM" >> cmd.log
            SOURCE_STREAM=${video_path}/$i
            WIDTH_YUV=${width_array}/$i
            HEIGHT_YUV=${height_array}/$i
            for codec_param in ${codec_list[@]}
            do
                echo "codec $codec_param >>>>>>>>>>>>" >> cmd.log
                for device in ${dev_list[@]} 
                do
                    echo "../sample/build/out/video_enc_multi_thread -d $device -i $SOURCE_STREAM -f $type -w $WIDTH_YUV -h $HEIGHT_YUV -c $codec_param -o tmp.data -s 0 -l 1 -T $threadnum_param" >> cmd.log
                    ../../sample/build/out/video_enc_multi_thread -d $device -i $SOURCE_STREAM -f $type -w $WIDTH_YUV -h $HEIGHT_YUV -c $codec_param \
                    -o tmp.data -s 0 -l 5 -T $threadnum_param &
                done
                echo "codec $codec_param <<<<<<<<<<<" >> cmd.log
            done
            wait
            echo "wait ================= " >> cmd.log
        done
        echo "yuv $type <<<<<<" >> cmd.log
    done
    echo "paralle $i <<<"
done
echo "done"

