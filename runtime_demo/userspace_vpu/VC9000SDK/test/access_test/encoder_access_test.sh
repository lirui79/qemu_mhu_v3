#!/bin/bash

#source export.sh

cur_dir=`pwd`
outdir=$cur_dir/out/enc_yuv
#indir=$cur_dir/../encode_input/
indir=/home/vastai/resource/dataset/encoder_access_test
stream_dir=/home/vastai/resource/dataset/decoder_access_test
# indir=/video-case/lowlevel_SDK/res/encoder_access_test
# stream_dir=/video-case/lowlevel_SDK/res/decoder_access_test
> error_log_enc.txt
# platform: sv100 or sg100, default: sv100
platform=$1
# render_mode default: /dev/vastai_video0
render_node=$2

#rm -rf $outdir
mkdir -p $outdir

#encode_yuv_md5_array=("215432a8dd56a65c2d2b12546e21a672" "4c1680ce81e5defdf9a3759f490727c5"
#		      "215432a8dd56a65c2d2b12546e21a672" "4c1680ce81e5defdf9a3759f490727c5"
#		      "da3fdf10f08e716059878f22d984fea0" "a2dce4c222a5790846386e8f057adcb7"
#		      "da3fdf10f08e716059878f22d984fea0" "a2dce4c222a5790846386e8f057adcb7"
#		      "5774de055c9f5d03e137e36c962c84ea" "76eba06ef7655fcb75c38164d5e88821"
#		      "5774de055c9f5d03e137e36c962c84ea" "76eba06ef7655fcb75c38164d5e88821"
#		      "29745cb846691a1500e7c3684de01b41" "a19f6f3268d13fbd6a72714de7a2ad87"
#		      "29745cb846691a1500e7c3684de01b41" "a19f6f3268d13fbd6a72714de7a2ad87"
#)

#encode_yuv_md5_array=("b0597fad9fe710a5151ffbbfd0b87e14" "4c1680ce81e5defdf9a3759f490727c5"
#		      "b0597fad9fe710a5151ffbbfd0b87e14" "4c1680ce81e5defdf9a3759f490727c5"
#		      "b0597fad9fe710a5151ffbbfd0b87e14" "4c1680ce81e5defdf9a3759f490727c5"
#		      "da3fdf10f08e716059878f22d984fea0" "a2dce4c222a5790846386e8f057adcb7"
#		      "da3fdf10f08e716059878f22d984fea0" "a2dce4c222a5790846386e8f057adcb7"
#		      "5774de055c9f5d03e137e36c962c84ea" "76eba06ef7655fcb75c38164d5e88821"
#		      "5774de055c9f5d03e137e36c962c84ea" "76eba06ef7655fcb75c38164d5e88821"
#		      "29745cb846691a1500e7c3684de01b41" "a19f6f3268d13fbd6a72714de7a2ad87"
#		      "29745cb846691a1500e7c3684de01b41" "a19f6f3268d13fbd6a72714de7a2ad87"
#)



# encode_yuv_md5_array=("debacc947ebb0e0c1b3a80815c75fb0e" "4c1680ce81e5defdf9a3759f490727c5"
#                       "debacc947ebb0e0c1b3a80815c75fb0e" "4c1680ce81e5defdf9a3759f490727c5"
#                       "de358f4705d1303637292a0d5b8bdccc" "94286532d6c5e0744ea9ea4173a46607"
#                       "de358f4705d1303637292a0d5b8bdccc" "94286532d6c5e0744ea9ea4173a46607"
#                       "1e1059ecfdc4f6ce648dfc38186b9b27" "76eba06ef7655fcb75c38164d5e88821"
#                       "1e1059ecfdc4f6ce648dfc38186b9b27" "76eba06ef7655fcb75c38164d5e88821"
#                       "13b1d0376aca35073597158b5fb758db" "52852d60cf839ec4aa8351c959eb57be"
#                       "13b1d0376aca35073597158b5fb758db" "52852d60cf839ec4aa8351c959eb57be"
# )



encode_yuv_md5_array=(
    "399497595b5bf4f1dd05fee8cc3d56ec" #0
    "399497595b5bf4f1dd05fee8cc3d56ec" #1
    "399497595b5bf4f1dd05fee8cc3d56ec" #2
    "32e13022aafe62ce8e837198a552438e" #3
    "b6eca8a775ac296b6172f2652ce2ac49" #4
    "ce11a9191634d80a5b00a7334c4faba5" #5
    "785133a13e3741d9762e91471fccdd11" #6
    "bcf3b5b7f3393cf0ab824ad4d5f4143d" #7
    "3be05470cb115d1e65e9ed2dd47aede8" #8
    "d990a73010a408d0692ee9a68828c5e7" #9
    "a1532fcd917d03aca58fe14040c2ea67" #10
    "8e2f429fe436d16212e905c809ae8de7" #11
    "38c2e2f936acbce9828d1088ea83b669" #12
    "0ccfe18a5542757eb894f6764bd1bf9d" #13
    "4a992c461f3458252774609a4c40693d" #14
    "608f9234f87a46da0621416c61310c8f" #15
    "314f8ee17c3cdbbc501303922c8ae03a" #16
    "b0efb9e9f7e51bc1a086330dc692ad3a" #17
    "c0f65d1376f50c747253e235cd2ee0ff" #18
    "9ca98eb6b757bf0309a2cfe3f5a3f136" #19
    "b92ad46f485fd8f03b2b4d26f6ad945b" #20
    "750a7ebbaea031f44d75944bed3edc74" #21
    "3cf54c0ad991d76779c7e98b4062241b" #22
    "63a5e308e19bb0d029fa48011798ea83" #23
    "a63b72db9acdd8cab3972bf7cad8eae9" #24
    "21af8e13d1f275b96157d340cf61dc8e" #25
    "5485dec76487d6c23104f2c4750d47b8" #26
    "b582267e7f2421b188906373ec6c9010" #27
    "b0efb9e9f7e51bc1a086330dc692ad3a" #28
    "c0f65d1376f50c747253e235cd2ee0ff" #29
    "9ca98eb6b757bf0309a2cfe3f5a3f136" #30
    "342df1182313fb9b75ae97ef1c579897" #31
    "7767ecc8eef4e5a11256fd94f4e39f2e" #32
    "eed3fd672aa7c125acb7e1193e06d877" #33
    "b0aa1f1c409ab4a561b9052513e23157" #34
    "7c557b8b777891e09027c53101f92836" #35
    "bf54e8ec685283cfcc489f485a65a1a2" #36
    "a9c59b580417c408af322dff4ad14126" #37
    "5bf8caf03e700199d42c70a1438dcea5" #38
    "1ff1ce7b7fc362dbede08bb4ff0cced7" #39
    "4a200e77c1303533a227de1ac4cb250c" #40
    "f3b968bbf427f104dc45c8a023dde2fd" #41
    "64182c98fcbf0e03a40615700866d060" #42
    )


function calculate_psnr_ssim(){
    llvideo_log=$1
    ffmpeg_log=$2
    md5_index=$3
    
    if [[ $md5_index -eq 25 || $md5_index -eq 26 ]]; then
        echo "psnr calculates skipping encode $md5_index with stride"
    else
        llvideo_psnr=`grep -o 'Average PSNR\[[0-9]*\]: Y [0-9]*\.[0-9]*, U [0-9]*\.[0-9]*, V [0-9]*\.[0-9]*' ${llvideo_log} | awk '{print}'`
        llvideo_ssim=`grep -o 'Average SSIM\[[0-9]*\]: Y [0-9]*\.[0-9]*, U [0-9]*\.[0-9]*, V [0-9]*\.[0-9]*' ${llvideo_log} | awk '{print}'`
        echo -e "LLVideo: $llvideo_psnr\t$llvideo_ssim"

        y_psnr=$(cat ${llvideo_log} |grep "PSNR" |cut -d " " -f 12 |cut -d "," -f 1)
        u_psnr=$(cat ${llvideo_log} |grep "PSNR" |cut -d " " -f 14 |cut -d "," -f 1)
        v_psnr=$(cat ${llvideo_log} |grep "PSNR" |cut -d " " -f 16 |cut -d "," -f 1)
        # echo $y_psnr, $u_psnr, $v_psnr

        y_ssim=$(cat ${llvideo_log} |grep "SSIM" |cut -d " " -f 12 |cut -d "," -f 1)
        u_ssim=$(cat ${llvideo_log} |grep "SSIM" |cut -d " " -f 14 |cut -d "," -f 1)
        v_ssim=$(cat ${llvideo_log} |grep "SSIM" |cut -d " " -f 16 |cut -d "," -f 1)
        # echo $y_ssim, $u_ssim, $v_ssim

        ffmpeg_psnr=`grep -o 'PSNR y:[0-9]*\.[0-9]* u:[0-9]*\.[0-9]* v:[0-9]*\.[0-9]*' ${ffmpeg_log} | awk '{print}'`
        ffmpeg_ssim=`grep -o 'SSIM Y:[0-9]*\.[0-9]* ([0-9]*\.[0-9]*) U:[0-9]*\.[0-9]* ([0-9]*\.[0-9]*) V:[0-9]*\.[0-9]* ([0-9]*\.[0-9]*)' ${ffmpeg_log} | awk '{print}'`
        echo -e "FFmpeg: $ffmpeg_psnr\t$ffmpeg_ssim"

        ffmpeg_y_psnr=$(cat ${ffmpeg_log} |grep "PSNR" |cut -d " " -f 5 |cut -d ":" -f 2)
        ffmpeg_u_psnr=$(cat ${ffmpeg_log} |grep "PSNR" |cut -d " " -f 6 |cut -d ":" -f 2)
        ffmpeg_v_psnr=$(cat ${ffmpeg_log} |grep "PSNR" |cut -d " " -f 7 |cut -d ":" -f 2)
        # echo $ffmpeg_y_psnr, $ffmpeg_u_psnr, $ffmpeg_v_psnr

        ffmpeg_y_ssim=$(cat ${ffmpeg_log} |grep "SSIM" |cut -d " " -f 5 |cut -d ":" -f 2)
        ffmpeg_u_ssim=$(cat ${ffmpeg_log} |grep "SSIM" |cut -d " " -f 7 |cut -d ":" -f 2)
        ffmpeg_v_ssim=$(cat ${ffmpeg_log} |grep "SSIM" |cut -d " " -f 9 |cut -d ":" -f 2)
        # echo $ffmpeg_y_ssim, $ffmpeg_u_ssim, $ffmpeg_v_ssim

        diff_y_psnr=`awk 'BEGIN{printf "%f\n",('$y_psnr' - '$ffmpeg_y_psnr')}'`
        diff_u_psnr=`awk 'BEGIN{printf "%f\n",('$u_psnr' - '$ffmpeg_u_psnr')}'`
        diff_v_psnr=`awk 'BEGIN{printf "%f\n",('$v_psnr' - '$ffmpeg_v_psnr')}'`

        diff_y_ssim=`awk 'BEGIN{printf "%f\n",('$y_ssim' - '$ffmpeg_y_ssim')}'`
        diff_u_ssim=`awk 'BEGIN{printf "%f\n",('$u_ssim' - '$ffmpeg_u_ssim')}'`
        diff_v_ssim=`awk 'BEGIN{printf "%f\n",('$v_ssim' - '$ffmpeg_v_ssim')}'`

        diff_y_psnr=${diff_y_psnr#-}
        diff_u_psnr=${diff_u_psnr#-}
        diff_v_psnr=${diff_v_psnr#-}
        diff_y_ssim=${diff_y_ssim#-}
        diff_u_ssim=${diff_u_ssim#-}
        diff_v_ssim=${diff_v_ssim#-}
        echo -e "diff psnr: $diff_y_psnr, $diff_u_psnr, $diff_v_psnr\tdiff ssim: $diff_y_ssim, $diff_u_ssim, $diff_v_ssim"

        psnr_limit=0.8
        ssim_limit=0.01

        if [ $(echo "${diff_y_psnr} < ${psnr_limit}" | bc) = 1 ] \
            && [ $(echo "${diff_u_psnr} < ${psnr_limit}" | bc) = 1 ] \
            && [ $(echo "${diff_v_psnr} < ${psnr_limit}" | bc) = 1 ] \
            && [ $(echo "${diff_y_ssim} < ${ssim_limit}" | bc) = 1 ] \
            && [ $(echo "${diff_u_ssim} < ${ssim_limit}" | bc) = 1 ] \
            && [ $(echo "${diff_v_ssim} < ${ssim_limit}" | bc) = 1 ];
        then
            echo ">>>>> psnr & ssim pass"
        else
            if [ $(echo "${y_psnr} > 32" | bc) = 1 ] \
                && [ $(echo "${u_psnr} > 32" | bc) = 1 ] \
                && [ $(echo "${v_psnr} > 32" | bc) = 1 ] \
                && [ $(echo "${ffmpeg_y_psnr} > 32" | bc) = 1 ] \
                && [ $(echo "${ffmpeg_u_psnr} > 32" | bc) = 1 ] \
                && [ $(echo "${ffmpeg_v_psnr} > 32" | bc) = 1 ]; then
                echo "psnr >= 32, pass"
            else
                echo ">>>>> psnr & ssim fail"
                sleep 1
                # Logging error messages to variables or files
                echo "err: md5index:$md5_index, $inputfile, diff psnr: $diff_y_psnr, $diff_u_psnr, $diff_v_psnr\tdiff ssim: $diff_y_ssim, $diff_u_ssim, $diff_v_ssim" >> error_log_enc.txt
                return 1
            fi
        fi
    fi
}

function encode_yuv(){
    inputfile=$1
    filename=${inputfile##*/}
    width=$2
    height=$3
    stride=$4
    pix_format=$5
    enc_type=$6
    md5_index=$7
    lookaheadDepth=$8
    gopSize=$9

    if  [ "$lookaheadDepth"x = ""x ] ; then
        lookaheadDepth=0
    fi
    if  [ "$gopSize"x = ""x ] ; then
        gopSize=1
    fi

    #echo lookaheadDepth=$lookaheadDepth
    #echo gopSize=$gopSize
    
    today=$(date +%d)
    #echo today: $today
    day=$((10#$today%5))
    #echo day: $day
    name=$outdir/out$md5_index-$filename$day".$enc_type"
    if  [ "$enc_type"x = "av1"x ] ; then
	    name=$outdir/out$md5_index-$filename$day".ivf"
    fi
    #echo name: $name

    #ffmpeg -y   -init_hw_device vaapi=vastai:$render_node -hwaccel vaapi -hwaccel_output_format vaapi -hwaccel_device vastai -filter_hw_device vastai -s $res_size  -pix_fmt $pix_format -r 30 -i $inputfile -vf format=$pix_format\|vaapi,hwupload  -c:v $enc_type"_vaapi"  -b:v 2000000 -vast-params "tune=1:vbvMaxRate=4000:keyint=120:miniGopSize=0:lookaheadLength=0:intraQpOffset=-2:preset=gold_quality" -y $name
    if [ "$enc_type"x = "jpeg"x ] ; then
        echo "<<< $md5_index:jpeg encode: $inputfile >>>"
        echo
        ../../sample/build/out/jpeg_enc -i $inputfile -o $name -w $width -h $height -t $stride -f $pix_format -d $render_node
    elif [ "$enc_type"x = "h264"x ] ; then
        echo "<<< $md5_index:h264 encode: $inputfile >>>"
        echo

        if [ $enable_mse == 1 ];then
            ../../sample/build/out/video_enc -i $inputfile -o $name -w $width -h $height -t $stride -f $pix_format -s 1 -c 0 -d $render_node --lookaheadDepth $lookaheadDepth --gopSize $gopSize --psnr 1 --ssim 1 > ${llvideo_log_path} 2>&1

            $vebanch_ffmpeg -hide_banner -s ${width}x${height} -pix_fmt $pix_format -r 30 -i $inputfile -r 30 -i $name -filter_complex '[1:v][0:v]psnr=stats_file=-:shortest=1;[1:v][0:v]ssim=stats_file=-:shortest=1' -f null - 1>/dev/null 2>$ffmpeg_log_path

            perl -pe 's/\e[\[\(][0-9;]*[mGKFB]//g' -i $outdir/*.log

            calculate_psnr_ssim ${llvideo_log_path} ${ffmpeg_log_path} ${md5_index}
        else
            ../../sample/build/out/video_enc -i $inputfile -o $name -w $width -h $height -t $stride -f $pix_format -s 1 -c 0 -d $render_node --lookaheadDepth $lookaheadDepth --gopSize $gopSize
        fi

    elif [ "$enc_type"x = "hevc"x ] ; then
        echo "<<< $md5_index:hevc encode: $inputfile >>>"
        echo

        if [ $enable_mse == 1 ];then
            ../../sample/build/out/video_enc -i $inputfile -o $name -w $width -h $height -t $stride -f $pix_format -s 1 -c 1 -d $render_node --lookaheadDepth $lookaheadDepth --gopSize $gopSize --psnr 1 --ssim 1 > ${llvideo_log_path} 2>&1

            $vebanch_ffmpeg -hide_banner -s ${width}x${height} -pix_fmt $pix_format -r 30 -i $inputfile -r 30 -i $name -filter_complex '[1:v][0:v]psnr=stats_file=-:shortest=1;[1:v][0:v]ssim=stats_file=-:shortest=1' -f null - 1>/dev/null 2>$ffmpeg_log_path

            perl -pe 's/\e[\[\(][0-9;]*[mGKFB]//g' -i $outdir/*.log

            calculate_psnr_ssim ${llvideo_log_path} ${ffmpeg_log_path} ${md5_index}
        else
            ../../sample/build/out/video_enc -i $inputfile -o $name -w $width -h $height -t $stride -f $pix_format -s 1 -c 1 -d $render_node --lookaheadDepth $lookaheadDepth --gopSize $gopSize
        fi

    elif [ "$enc_type"x = "av1"x ] ; then
        echo "<<< $md5_index:av1 encode: $inputfile >>>"
        echo
        ../../sample/build/out/video_enc -i $inputfile -o $name -w $width -h $height -t $stride -f $pix_format -s 1 -c 2 -d $render_node --lookaheadDepth $lookaheadDepth --gopSize $gopSize
    else
        echo unsupported codec type: ${enc_type}
    fi

    hw_md5=`md5sum $name`
    hw_md5=${hw_md5:0:32}

    sw_md5=${encode_yuv_md5_array[$md5_index]}

    echo " "
    echo "hw_md5=$hw_md5, sw_md5=$sw_md5"

    echo " "
    if [ "$hw_md5" != "$sw_md5" ] ; then
        echo "$inputfile encode result mismatch, failed!!!"
        sleep 1
        # Logging error messages to variables or files
        echo "err: md5index:$md5_index, $inputfile encode result mismatch: hw_md5=$hw_md5, sw_md5=$sw_md5" >> error_log_enc.txt
        return 1
    else
        echo "encode result match， success!!!"
        # rm $name
    fi

    echo " "
}

function transcode(){
    inputfile=$1
    filename=${inputfile##*/}
    dec_type=$2
    enc_type=$3
    md5_index=$4

    today=$(date +%d)
    #echo today: $today
    day=$((10#$today%5))
    #echo day: $day
    name=$outdir/out$md5_index-$filename$day".$enc_type"
    if  [ "$enc_type"x = "av1"x ] ; then
        name=$outdir/out$md5_index-$filename$day".ivf"
    fi
    #echo name: $name


    echo "<<< $md5_index:transcode_mt: $inputfile >>>"
    echo
    ../../sample/build/out/transcode_mt -i $inputfile -o $name -s 1 -C $enc_type -r $render_node -f 0 -c  $dec_type

    hw_md5=`md5sum $name`
    hw_md5=${hw_md5:0:32}

    sw_md5=${encode_yuv_md5_array[$md5_index]}

    echo " "
    echo "hw_md5=$hw_md5, sw_md5=$sw_md5"

    echo " "
    if [ "$hw_md5" != "$sw_md5" ] ; then
        echo "$inputfile transcode_mt result mismatch, failed!!!"
        sleep 1
        # Logging error messages to variables or files
        echo "err: md5index:$md5_index, $inputfile transcode result mismatch, hw_md5=$hw_md5, sw_md5=$sw_md5" >> error_log_enc.txt
    else
        echo "transcode_mt result match， success!!!"
        # rm $name
    fi

    echo " "
}

function run_multi_process_test(){

    inputfile=$1
    filename=${inputfile##*/}
    width=$2
    height=$3
    stride=$4
    pix_format=$5
    enc_type=$6
    md5_index=$7

    today=$(date +%d)
    #echo today: $today
    day=$((10#$today%5))
    #echo day: $day
    name=$outdir/out-$filename$day".$enc_type"
    if  [ "$enc_type"x = "av1"x ] ; then
        name=$outdir/out-$filename$day".ivf"
    fi
    #echo name: $name

    for i in $(seq 1 20)
    do
        if [ "$enc_type"x = "jpeg"x ] ; then
            ../../sample/build/out/jpeg_enc -i $inputfile -o $name -w $width -h $height -t $stride -f $pix_format -s 0 -d $render_node -l 1000  &
        elif [ "$enc_type"x = "h264"x ] ; then
            ../../sample/build/out/video_enc -i $inputfile -o $name -w $width -h $height -t $stride -f $pix_format -s 0 -c 0 -d $render_node &
        elif [ "$enc_type"x = "hevc"x ] ; then
            ../../sample/build/out/video_enc -i $inputfile -o $name -w $width -h $height -t $stride -f $pix_format -s 0 -c 1 -d $render_node &
        elif [ "$enc_type"x = "av1"x ] ; then
            ../../sample/build/out/video_enc -i $inputfile -o $name -w $width -h $height -t $stride -f $pix_format -s 0 -c 2 -d $render_node &
        else
            echo unsupported codec type: ${enc_type}
        fi
    done

    count=0
    while [ 1 ]
    do

        if [ "$enc_type"x = "jpeg"x ] ; then
            result=`ps -ef |grep  "jpeg_enc " |grep -v grep`
        else
            result=`ps -ef |grep  "video_enc " |grep -v grep`
        fi

        if [ $count -gt 120 ] ; then
            echo " "
            echo "run 20 channel encode test failed!!!"
            echo " "
            # Logging error messages to variables or files
            echo "err: md5index:$md5_index, $inputfile run 20 channel encode test failed!!!" >> error_log_enc.txt
        fi

            if [ -z "$result" ]; then
                echo " "
                echo "run 20 channel $enc_type encode test success!!!"
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

    inputfile=$1
    filename=${inputfile##*/}
    width=$2
    height=$3
    stride=$4
    pix_format=$5
    enc_type=$6
    md5_index=$7

    today=$(date +%d)
    #echo today: $today
    day=$((10#$today%5))
    #echo day: $day
    name=$outdir/out-$filename$day".$enc_type"
    if  [ "$enc_type"x = "av1"x ] ; then
        name=$outdir/out-$filename$day".ivf"
    fi
    #echo name: $name

    if [ "$platform"x = "sv100"x ] ; then
        thread_num=100
    else
        thread_num=28
    fi

    if [ "$enc_type"x = "jpeg"x ] ; then
        ../../sample/build/out/jpeg_enc_mt -T $thread_num -i $inputfile -w $width -h $height -t $stride -f $pix_format -s 0 -d $render_node
    elif [ "$enc_type"x = "h264"x ] ; then
        ../../sample/build/out/video_enc_multi_thread -T $thread_num -i $inputfile -o $outdir -w $width -h $height -t $stride -f $pix_format -s 1 -c 0 -d $render_node
    elif [ "$enc_type"x = "hevc"x ] ; then
        ../../sample/build/out/video_enc_multi_thread -T $thread_num -i $inputfile -o $outdir -w $width -h $height -t $stride -f $pix_format -s 1 -c 1 -d $render_node
    elif [ "$enc_type"x = "av1"x ] ; then
        ../../sample/build/out/video_enc_multi_thread -T $thread_num -i $inputfile -o $outdir -w $width -h $height -t $stride -f $pix_format -s 1 -c 2 -d $render_node
    else
        echo unsupported codec type: ${enc_type}
    fi

    echo "run $thread_num thread $enc_type encode test success!!!"

}

function run_yuv_encode(){
    
    echo "run_yuv_encode start:"

    ## jpeg encoder test

    #1. 480P/pix_fmt:
    encode_yuv $indir/smile_640x480_0.yuv 640 480 640 nv12 jpeg 0
    encode_yuv $indir/smile_640x480_nv21.yuv 640 480 640 nv21 jpeg 1
    encode_yuv $indir/smile_640x480_yuv420p.yuv 640 480 640 yuv420p jpeg 2

    #2. alignment:
    encode_yuv $indir/smile_642x482_0.yuv 642 482 642 nv12 jpeg 3
    encode_yuv $indir/smile_642x482_0.yuv 640 482 642 nv12 jpeg 4
    encode_yuv $indir/smile_640x480_0.yuv 638 480 640 nv12 jpeg 5

    #3. 720P
    encode_yuv $indir/smile_1280x720_0.yuv 1280 720 1280 nv12 jpeg 6

    #4. 1080P
    encode_yuv $indir/smile_1920x1080_0.yuv 1920 1080 1920 nv12 jpeg 7

    #5. 16K
    encode_yuv $indir/126M_16384x16384.yuv 16384 16384 16384 nv12 jpeg 8

    #6. Odd number resolution
    encode_yuv $indir/smile_641x481.yuv 641 481 641 nv12 jpeg 9
    encode_yuv $indir/smile_641x481_yuv420p.yuv 641 481 641 yuv420p jpeg 10


    ## video encoder test

    #7 480P h264/hevc/av1
    encode_yuv $indir/BQMall_832x480_60.yuv 832 480 832 yuv420p h264 11
    encode_yuv $indir/BQMall_832x480_60.yuv 832 480 832 yuv420p hevc 12
    encode_yuv $indir/BQMall_832x480_60.yuv 832 480 832 yuv420p av1 13

    #8 720P h264/hevc/av1
    encode_yuv $indir/SlideEditing_1280x720_30.yuv 1280 720 1280 yuv420p h264 14
    encode_yuv $indir/SlideEditing_1280x720_30.yuv 1280 720 1280 yuv420p hevc 15
    encode_yuv $indir/SlideEditing_1280x720_30.yuv 1280 720 1280 yuv420p av1 16

    #9 1080P h264/hevc/av1
    encode_yuv $indir/Kimono1_1920x1080_24.yuv 1920 1080 1920 yuv420p h264 17
    encode_yuv $indir/Kimono1_1920x1080_24.yuv 1920 1080 1920 yuv420p hevc 18
    encode_yuv $indir/Kimono1_1920x1080_24.yuv 1920 1080 1920 yuv420p av1 19

    #10 4K h264/hevc/av1
    encode_yuv $indir/Kimono1_4096x4096_24_10.yuv 4096 4096 4096 yuv420p h264 20
    encode_yuv $indir/Kimono1_4096x4096_24_10.yuv 4096 4096 4096 yuv420p hevc 21
    encode_yuv $indir/Netflix_TunnelFlag_4096x2160_60fps_8bit_420.yuv 4096 2160 4096 yuv420p av1 22

    #11 aligment h264/hevc (2 byte aligment)
    encode_yuv $indir/BasketballDrill_834x480_50.yuv 834 480 834 yuv420p h264 23
    encode_yuv $indir/BasketballDrill_834x480_50.yuv 834 480 834 yuv420p hevc 24

    #12 stride h264/hevc
    encode_yuv $indir/Kimono1_1920x1080_24.yuv 1918 1080 1920 yuv420p h264 25
    encode_yuv $indir/Kimono1_1920x1080_24.yuv 1918 1080 1920 yuv420p hevc 26
    encode_yuv $indir/Kimono1_1920x1080_24.yuv 1912 1080 1920 yuv420p av1 27

    #13 NV12 h264/hevc
    encode_yuv $indir/Kimono1_1920x1080_24_NV12.yuv 1920 1080 1920 nv12 h264 28
    encode_yuv $indir/Kimono1_1920x1080_24_NV12.yuv 1920 1080 1920 nv12 hevc 29
    encode_yuv $indir/Kimono1_1920x1080_24_NV12.yuv 1920 1080 1920 nv12 av1 30

    #14 transcode_mt h264 <--> hevc <--> av1 <--> h264
    transcode_mt $stream_dir/tlst_1080p-h264-0-10.h264 h264 h264 31
    transcode_mt $stream_dir/tlst_1080p-h264-0-10.h264 h264 hevc 32
    transcode_mt $stream_dir/tlst_1080p-h264-0-10.h264 h264 av1 33
    transcode_mt $stream_dir/cdzj-hevc-0-6.hevc hevc h264 34
    transcode_mt $stream_dir/cdzj-hevc-0-6.hevc hevc hevc 35
    transcode_mt $stream_dir/cdzj-hevc-0-6.hevc hevc av1 36

    if [ "$platform"x = "sv100"x ] ; then
        transcode_mt $stream_dir/ParkScene_1920x1080_24_8M.ivf av1 av1 37
        transcode_mt $stream_dir/ParkScene_1920x1080_24_8M.ivf av1 h264 38
        transcode_mt $stream_dir/ParkScene_1920x1080_24_8M.ivf av1 hevc 39
    fi

    #15 2pass&gopSize=0 h264/hevc/av1
    encode_yuv $indir/Kimono1_1920x1080_24.yuv 1920 1080 1920 yuv420p h264 40 20 0
    encode_yuv $indir/Kimono1_1920x1080_24.yuv 1920 1080 1920 yuv420p hevc 41 20 0
    encode_yuv $indir/Kimono1_1920x1080_24.yuv 1920 1080 1920 yuv420p av1 42 20 0

    #16 multi-process test (20 processes)
    run_multi_process_test $indir/Kimono1_1920x1080_24.yuv 1920 1080 1920 yuv420p h264 100
    sleep 2
    run_multi_process_test $indir/Kimono1_1920x1080_24.yuv 1920 1080 1920 yuv420p hevc 101
    sleep 2
    run_multi_process_test $indir/Kimono1_1920x1080_24.yuv 1920 1080 1920 yuv420p av1 102
    sleep 2
    run_multi_process_test $indir/smile_1920x1080_0.yuv 1920 1080 1920 nv12 jpeg 103
    sleep 2

    #17 multi-thread test (100 threads)
    run_multi_thread_test $indir/Kimono1_1920x1080_24.yuv 1920 1080 1920 yuv420p h264 104
    sleep 2
    run_multi_thread_test $indir/Kimono1_1920x1080_24.yuv 1920 1080 1920 yuv420p hevc 105
    sleep 2
    run_multi_thread_test $indir/Kimono1_1920x1080_24.yuv 1920 1080 1920 yuv420p av1 106
    sleep 2
    run_multi_thread_test $indir/smile_1920x1080_0.yuv 1920 1080 1920 nv12 jpeg 107
    
    echo "run_yuv_encode end!"
}

function main(){
    if [ -z $render_node ];then
        render_node="/dev/vastai_video0"
    fi
    if [ -z $platform ];then
        platform="sv100"
    fi

    enable_mse=0
    if [ "$1"x == "1"x ];then
        llvideo_log_path=$outdir/llvideo_psnr_ssim.log
        ffmpeg_log_path=$outdir/ffmpeg_psnr_ssim.log

        if [ `uname -m` == "x86_64" ];then
            vebanch_ffmpeg=/video-case/video4.3/ffmpeg_org/ffmpeg
        else
            vebanch_ffmpeg=/video-case/video4.3/ffmpeg_org_arm/ffmpeg
        fi

        enable_mse=1
    fi

    run_yuv_encode

    if [ -s error_log_enc.txt ]; then
        echo " "
        echo "error_log_enc.txt is generated and not empty, read it!"
        cat error_log_enc.txt
    elif [ -f error_log_enc.txt ]; then
        echo " "
        echo "Pass all test cases!"
    fi
    exit 0
}

main $3
