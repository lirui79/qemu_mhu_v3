#!/bin/bash

# ----------------------------------------------------------------------------------------------------------
# echo color (\033、\e和\E)
RED='\E[1;31m'
GREEN='\E[1;32m'
WHITE='\E[1;37m'
YELOW='\E[1;33m'
CLR='\E[0m'

# global value
run_num=0
pass_num=0
fail_num=0
cur_dir=`pwd`
log_file="log.txt"
out_dir="/home/stone/workspace/OUTPUT"
check_md5=0


decDevice="/dev/hantrodec"
encDevice="/dev/hantroenc"
memDevice="/dev/memalloc"

# decoders list
#!/bin/bash

TARGET_DIR="/home/stone/workspace"
# 定义全局数组
h264_list=()
hevc_list=()
vp9_list=()
av1_list=()
jpeg_list=()
yuv1080_list=()
yuv352_list=()


# 定义函数：查找指定格式的视频文件并存入指定数组
# 参数1: 目标目录路径
# 参数2: 接收结果的数组变量名 (例如: h264_list)
# 参数3: 文件扩展名 (例如: h264, hevc, vp9, av1)
load_video_files_by_ext() {
    local target_dir="$1"
    local array_name="$2"
    local extension="$3"

    # 1. 参数校验
    if [ -z "$target_dir" ] || [ -z "$array_name" ] || [ -z "$extension" ]; then
        echo "Error: Missing arguments. Usage: load_video_files_by_ext <dir> <array_name> <ext>" >&2
        return 1
    fi

    if [ ! -d "$target_dir" ]; then
        echo "Error: Directory '$target_dir' does not exist." >&2
        return 1
    fi

    # 2. 建立对全局数组的引用 (Nameref)
    # 这允许函数直接操作调用者传入的数组变量
    local -n result_array="$array_name"

    # 清空目标数组，防止追加旧数据
    result_array=()

    # 3. 执行查找
    # 注意：find 中扩展名需要加点，如 *.h264
    while IFS= read -r -d '' file; do
        result_array+=("$file")
    done < <(find "$target_dir" -type f -name "*.${extension}" -print0)

    return 0
}
# -------------------------------
check_char_device() {
    local dev_path=$1
    local dev_name=$2
    if [ -c "$dev_path" ]; then
        echo "[OK] $dev_name ($dev_path)"
        return 0
    else
        echo "[FAIL] $dev_name ($dev_path)"
        return 1
    fi
    return 0
}

#---------------------------------------------------------------------------
# package echo
echo_with_color(){
    echo -e "$1$2$CLR"
    if [ "$3" ];then
        echo "$2" >> "$3"
    fi
}

decoder_video() {
    local dec_dev="$1"
    local mem_dev="$2"
    local input_file="$3"
    local output_file="$4"
    local codec="$5"
    ./build/out/video_dec -d ${dec_dev} -m ${mem_dev} -i ${input_file} -c ${codec} -o ${output_file} -l 1 -k 0 -s 1
}

run_decoding_video(){
    local dec_dev="$1"
    local mem_dev="$2"
    local index=0
#    md5_set=(c6e6352bd0aac26cede924539010e63e c77fc913caf5ffeb3eb134557d16c8c9)
    for decoder_file in ${h264_list[@]}
    do
        input_file="${decoder_file}"
        output_file="${out_dir}/h264_${index}.yuv"
        echo_with_color $WHITE "-Decoding" ${log_file}
        decoder_video $dec_dev $mem_dev $input_file $output_file "h264"
            
#    ./video_dec -i /home/stone/workspace/sample_1920x1080.hevc -o output.yuv -d /dev/hantrodec  -m /dev/memalloc -c hevc -l 1 -k 0
#    ./build/out/video_dec -d ${dec_dev} -m ${mem_dev} -i ${input_file} -c ${decoder} -o ${out_file} -l 1 -k 0 >> ${log_file} 2>&1 | grep -F --line-buffered ''
#        if [ "${check_md5}" == "1" ];then
#                md5_video=("${md5_set[0]}")
#            else
#                md5_video=("${md5_set[1]}")
#            fi
#
#            result_sdk=`md5sum $out_sdk | awk '{print $1}'`
#            if [ "$md5_video" == "$result_sdk" ];then
#                echo_with_color $GREEN "-Decoding ${input_file} on $dev PASS!!!" ${log_file}
#                pass_num=$(($pass_num+1))
#            else
#                echo_with_color $RED "-!!! Decoding ${input_file} on $dev FAIL." ${log_file}
#                fail_num=$(($fail_num+1))
#                #exit
#            fi
#        else
#            echo_with_color $GREEN "-Decoding ${input_file} on $dev PASS!!!" ${log_file}
#            ((run_num++))
#        fi
        ((index++))

        rm $output_file
    done

    for decoder_file in ${hevc_list[@]}
    do
        input_file="${decoder_file}"
        output_file="${out_dir}/hevc_${index}.yuv"
        echo_with_color $WHITE "-Decoding" ${log_file}
        decoder_video  $dec_dev $mem_dev $input_file $output_file  "hevc"
        ((index++))
        rm $output_file
    done


    for decoder_file in ${vp9_list[@]}
    do
        input_file="${decoder_file}"
        output_file="${out_dir}/vp9_${index}.yuv"
        echo_with_color $WHITE "-Decoding" ${log_file}
        decoder_video  $dec_dev $mem_dev $input_file $output_file  "vp9"
        ((index++))
        rm $output_file
    done

    for decoder_file in ${av1_list[@]}
    do
        input_file="${decoder_file}"
        output_file="${out_dir}/av1_${index}.yuv"
        echo_with_color $WHITE "-Decoding" ${log_file}
        decoder_video  $dec_dev $mem_dev $input_file $output_file  "av1"
        ((index++))
        rm $output_file
    done

}

run_decoding_jpg(){
    local dec_dev="$1"
    local mem_dev="$2"
    local index=0
#    md5_set=(43fbe7052e0272c48a14048484a7d909)
    for decoder_file in ${jpeg_list[@]}
    do
        input_file="${decoder_file}"
        output_file="${out_dir}/jpeg_${index}.yuv"
        echo_with_color $WHITE "-Decoding" ${log_file}
#./jpeg_dec -i /home/stone/workspace/VC9000D.jpg -o /tmp/out2.yuv -d /dev/hantrodec -m /dev/memalloc -l 1 -c 0
        ./build/out/jpeg_dec -d ${dec_dev} -m ${mem_dev} -i ${input_file} -o ${output_file}  -l 1 -c 0 >> ${log_file} 2>&1 | grep -F --line-buffered ''
        ((index++))
        rm $output_file
    done
}

encode_video(){
    local enc_dev="$1"
    local mem_dev="$2"
    local intput_file="$3"    # input file
    local w="$4"      # width
    local h="$5"      # height
    local f="$6"      # yuv format
    local codec="$7"  # codec type
    local output_file="$8"    # output file

    local codec_type=1 #0 for h264 and 1 for hevc

    #copy array
#    md5_set=(a87a02096165a91f1e1cc5ee299a0c11 a49c1ed1b27c5065e2206850af6137f6)
    if [ $codec == "h264" ];then
        # md5_set=("${enc_h264_md5[@]}")
        codec_type=0
    fi

    echo_with_color $WHITE "-Encoding '${intput_file}' into '${codec}'" ${log_file}
#./video_enc -i /home/stone/workspace/YUV/akiyo_352x288_300.yuv -o output.h265 -w 352 -h 288 -t 352 -e /dev/hantroenc -m /dev/memalloc  -f yuv420p -c 1 
    ./build/out/video_enc -e ${enc_dev} -m ${mem_dev} -i ${intput_file} -w ${w} -h ${h} -f ${f} -c ${codec_type} -o ${output_file} >> ${log_file} 2>&1

    rm $output_file
}

encode_jpg(){
    local enc_dev="$1"
    local mem_dev="$2"
    local intput_file="$3"    # input file
    local w="$4"      # width
    local h="$5"      # height
    local f="$6"      # yuv format
    local codec="$7"  # codec type
    local output_file="$8"    # output file

    local codec_type=0 # jpg

    #copy array
#    md5_set=(c1809f1f76f2d38b61f8118420456d30)
   
   
    echo_with_color $WHITE "-Encoding '${intput_file}' into '${codec}'" ${log_file}
#./jpeg_enc -i /home/stone/workspace/YUV/akiyo_352x288_300.yuv -o output.jpeg -w 352 -h 288 -t 352 -e /dev/hantroenc -m /dev/memalloc -f yuv420p 
    ./build/out/jpeg_enc -e ${enc_dev} -m ${mem_dev} -i ${intput_file} -w ${w} -h ${h} -f ${f} -o ${output_file} >> ${log_file} 2>&1
    rm $output_file
}

run_encoding_video(){
    local enc_dev="$1"
    local mem_dev="$2"
    local index=0

    for yuv_file in ${yuv1080_list[@]}
    do
        input_file="${yuv_file}"
        w=1920
        h=1080
        f="yuv420p"

        codec="hevc"
        output_file="${out_dir}/yuv_${index}.${codec}"
        encode_video $enc_dev $mem_dev ${input_file} $w $h $f $codec $output_file

        codec="h264"
        output_file="${out_dir}/yuv_${index}.${codec}"
        encode_video $enc_dev $mem_dev ${input_file} $w $h $f $codec $output_file

        ((index++))
    done
}

run_encoding_jpg(){
    local enc_dev="$1"
    local mem_dev="$2"
    local index=0

    for yuv_file in ${yuv1080_list[@]}
    do
        input_file="${yuv_file}"
        w=640
        h=480
        f="nv12"

        codec="jpg"
        output_file="${out_dir}/yuv_${index}.${codec}"
        encode_jpg $enc_dev $mem_dev ${input_file} $w $h $f $codec $output_file

        ((index++))
    done
}

run_transcoding(){
    local dec_dev="$1"
    local enc_dev="$2"
    local mem_dev="$3"
    local index=0
#    local trans_md5_sdk=(a49c1ed1b27c5065e2206850af6137f6 2397c0bf3511ee01269877c200238a2f)
    for decoder_file in ${h264_list[@]}
    do
        input_file="${decoder_file}"
        codec="hevc"
        output_file="${out_dir}/h264_${index}.${codec}"
#./transcode_mt -i /home/stone/workspace/akiyo_352x288_300_IBBBP.h264 -o output_352x288_300f.hevc -e /dev/hantroenc -d /dev/hantrodec -m /dev/memalloc -s 1 -l 1 -C hevc
        echo_with_color $WHITE "-Transcoding '${input_file}' into ${output_file}" ${log_file}
        ./build/out/transcode_mt -e $enc_dev -d $dec_dev -m $mem_dev  -i ${input_file} -C ${codec} -s 1 -f 0 -l 1 -o ${output_file} >> ${log_file} 2>&1
        ((index++))
        rm $output_file
    done

    for decoder_file in ${hevc_list[@]}
    do
        input_file="${decoder_file}"
        codec="h264"
        output_file="${out_dir}/hevc_${index}.${codec}"
#./transcode_mt -i /home/stone/workspace/akiyo_352x288_300_IBBBP.h264 -o output_352x288_300f.hevc -e /dev/hantroenc -d /dev/hantrodec -m /dev/memalloc -s 1 -l 1 -C hevc
        echo_with_color $WHITE "-Transcoding '${input_file}' into ${output_file}" ${log_file}
        ./build/out/transcode_mt -e $enc_dev -d $dec_dev -m $mem_dev  -i ${input_file} -C ${codec} -s 1 -f 0 -l 1 -o ${output_file} >> ${log_file} 2>&1
        ((index++))
        rm $output_file
    done
}

run_parallel(){
    # h264 decode
    # ./video_dec_mt -i /home/stone/workspace/akiyo_352x288_300_IBBBP.h264 -s 1 -o output -d /dev/hantrodec -m /dev/memalloc -l 1 -T 4 -c h264 
    ./build/out/video_dec_mt -i /home/stone/workspace/akiyo_352x288_300_IBBBP.h264 -s 1 -o ${out_dir} -d /dev/hantrodec -m /dev/memalloc -l 1 -c h264 -T $1 >> ${log_file} 2>&1
    # hevc decode
    # ./video_dec_mt -i /home/stone/workspace/sample_640x360.hevc -s 1 -o  output -d /dev/hantrodec -m /dev/memalloc -l 1 -T 4 -c hevc
    ./build/out/video_dec_mt -i /home/stone/workspace/sample_640x360.hevc -s 1 -o  ${out_dir} -d /dev/hantrodec -m /dev/memalloc -l 1 -c hevc -T $1 >> ${log_file} 2>&1
    # jpg decode
    # ./jpeg_dec_mt -i /home/stone/workspace/stream1.jpg -o output -d /dev/hantrodec -m /dev/memalloc -l 1 -c 0
    ./build/out/jpeg_dec_mt -i /home/stone/workspace/stream1.jpg -o ${out_dir} -d /dev/hantrodec -m /dev/memalloc -l 1 -c 0 >> ${log_file} 2>&1

    # h264 encode
    # ./video_enc_mt -i /home/stone/workspace/YUV/akiyo_352x288_300.yuv -o output -w 352 -h 288 -t 352 -e /dev/hantroenc -m /dev/memalloc  -f yuv420p -c 0 >> ${log_file} 2>&1
    ./build/out/video_enc_mt -T $1 -i /home/stone/workspace/YUV/akiyo_352x288_300.yuv -o ${out_dir} -w 352 -h 288 -t 352 -e /dev/hantroenc -m /dev/memalloc  -f yuv420p -c 0 >> ${log_file} 2>&1
    # hevc encode
    # ./video_enc_mt -i /home/stone/workspace/YUV/akiyo_352x288_300.yuv -o output -w 352 -h 288 -t 352 -e /dev/hantroenc -m /dev/memalloc  -f yuv420p -c 1 >> ${log_file} 2>&1
    ./build/out/video_enc_mt -T $1  -i /home/stone/workspace/YUV/akiyo_352x288_300.yuv -o ${out_dir} -w 352 -h 288 -t 352 -e /dev/hantroenc -m /dev/memalloc  -f yuv420p -c 1 >> ${log_file} 2>&1
    # jpg encode
    # ./jpeg_enc_mt -i /home/stone/workspace/YUV/sintel_trailer_1920x1080p_1253.yuv -e /dev/hantroenc -m /dev/memalloc -w 1920 -h 1080 -f yuv420p -o output -l 1 -s 0 -d 1 -T 32 >> ${log_file} 2>&1
    ./build/out/jpeg_enc_mt -T 4 -i /home/stone/workspace/YUV/sintel_trailer_1920x1080p_1253.yuv -e /dev/hantroenc -m /dev/memalloc -w 1920 -h 1080 -f yuv420p -o ${out_dir} -l 1 -s 0 -d 0 >> ${log_file} 2>&1

    # transcode_mt
    # ./build/out/transcode_mt -i /video-case/lowlevel_SDK/res_UT/dec/cdzj.h264 -o out/cdzj_trans.hevc -s 1 -r /dev/vastai_video0 -l 1 -f 0 -c h264 -C hevc >> ${log_file} 2>&1
    # ./build/out/transcode_mt -i /video-case/lowlevel_SDK/res_UT/dec/cdzj.hevc -o out/cdzj_trans.h264 -s 1 -r /dev/vastai_video0 -l 1 -f 0 -c hevc -C h264 >> ${log_file} 2>&1
}

run_test(){

    # clear old log file
    if [ -f ${log_file} ];then
        rm ${log_file}
    fi

    # clear old output directory
    if [ -d ${out_dir} ];then
        rm -rf ${out_dir}
    fi

    mkdir -p ${out_dir}

    echo_with_color $WHITE ">>>>> Run Test Cases" ${log_file}

    echo_with_color $WHITE "========== Run Test Cases On '$decDevice' '$encDevice' '$memDevice' ==========" ${log_file}

    # run decode
    echo_with_color $WHITE "---------- Run Decoding ----------" ${log_file}
    run_decoding_video $decDevice  $memDevice
    run_decoding_jpg $decDevice  $memDevice

    echo_with_color $WHITE "---------- Decoding FINISHED ----------" ${log_file}

    # run encode
    echo_with_color $WHITE "---------- Run Encoding ----------" ${log_file}
    run_encoding_video $encDevice  $memDevice
    run_encoding_jpg $encDevice  $memDevice
    echo_with_color $WHITE "---------- Encoding FINISHED ----------" ${log_file}

    # run transcode
    echo_with_color $WHITE "---------- Run Transcoding ----------" ${log_file}
    run_transcoding $decDevice $encDevice $memDevice
    echo_with_color $WHITE "---------- Transcoding FINISHED ----------" ${log_file}

    echo_with_color $GREEN "========== ALL Test Cases On '$decDevice' '$encDevice' '$memDevice' FINISHED ==========" ${log_file}
    # break

    if [ "$1" != "0" ];then
        echo_with_color $WHITE "---------- Run Decoding in Parallel ["$1"] ----------" ${log_file}
        run_parallel $1
        echo_with_color $WHITE "---------- Decoding in Parallel FINISHED ----------" ${log_file}

    fi

    rm -rf ${out_dir}
}

run(){
    check_md5=1
    parallel=8

    # 1. 调用函数
    check_char_device "$decDevice" "Hantro Decoder"

    # 2. 立即获取返回值并保存（防止被后续命令覆盖）
    status=$?

    # 3. 根据返回值判断
    if [ $status -eq 0 ]; then
        echo "check $decDevice [OK] pass"
    else
        echo "check $decDevice [FAIL] (Code:$status)"
        # 可以选择在此处退出脚本
        exit $status
    fi

    check_char_device "$encDevice" "Hantro Encoder"
    status=$?

    # 3. 根据返回值判断
    if [ $status -eq 0 ]; then
        echo "check $encDevice [OK] pass"
    else
        echo "check $encDevice [FAIL] (Code:$status)"
        # 可以选择在此处退出脚本
        exit $status
    fi

    check_char_device "$memDevice" "MemAlloc"
    # 2. 立即获取返回值并保存（防止被后续命令覆盖）
    status=$?

    # 3. 根据返回值判断
    if [ $status -eq 0 ]; then
        echo "check $memDevice [OK] pass"
    else
        echo "check $memDevice [FAIL] (Code:$status)"
        # 可以选择在此处退出脚本
        exit $status
    fi

    # 检查目录是否存在
    if [ ! -d "$TARGET_DIR" ]; then
        echo "Error: Target directory $TARGET_DIR not found."
        exit 1
    fi

    # 分别调用函数加载不同格式的文件
    # 注意：这里传入的是数组名的字符串，而不是数组本身
    load_video_files_by_ext "$TARGET_DIR/H264" h264_list "h264"
    load_video_files_by_ext "$TARGET_DIR/HEVC" hevc_list "hevc"
    load_video_files_by_ext "$TARGET_DIR" vp9_list "vp9"
    load_video_files_by_ext "$TARGET_DIR" av1_list "av1"
    load_video_files_by_ext "$TARGET_DIR/JPEG" jpeg_list "jpg"
    load_video_files_by_ext "$TARGET_DIR/YUV/1080p" yuv1080_list "yuv"

    # 输出统计结果
    echo "----------------------------------------"
    echo "扫描完成统计 ($TARGET_DIR):"
    echo "H.264 文件数量: ${#h264_list[@]}"
    echo "HEVC 文件数量:  ${#hevc_list[@]}"
    echo "VP9 文件数量:   ${#vp9_list[@]}"
    echo "AV1 文件数量:   ${#av1_list[@]}"
    echo "JPEG 文件数量:   ${#jpeg_list[@]}"
    echo "YUV 文件数量:   ${#yuv1080_list[@]}"
    echo "----------------------------------------"

    run_test ${parallel}
}

run $1