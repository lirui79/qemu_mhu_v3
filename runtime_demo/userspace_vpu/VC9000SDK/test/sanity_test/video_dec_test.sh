#!/bin/bash
# aligned h264/h265
filenames[0]=/home/vastai/resource/dataset/h264/xg1920x1088_x264_10.h264
filenames[1]=/home/vastai/resource/dataset/hevc/xg1920x1088_x265_100.h265

# un-aligned h264/h265
filenames[2]=/home/vastai/resource/dataset/h264/xg1922x1088_x264_100.h264
filenames[3]=/home/vastai/resource/dataset/hevc/xg1922x1088_x265_100.h265

# 720P h264/hevc
filenames[4]=/home/vastai/resource/dataset/h264/wurenqu-720p-h264-0-9.h264
filenames[5]=/home/vastai/resource/dataset/hevc/stream-720p-200f.hevc

# 1080P h264/hevc
filenames[6]=/home/vastai/resource/dataset/h264/tlst_1080p-h264-0-10.h264
filenames[7]=/home/vastai/resource/dataset/hevc/cdzj-hevc-0-6.hevc

# 4K h264/hevc
filenames[8]=/home/vastai/resource/dataset/h264/Kimono1_4096x4096_24.h264
filenames[9]=/home/vastai/resource/dataset/hevc/Kimono1_4096x4096_24.h265


codec[0]=h264
codec[1]=hevc
codec[2]=h264
codec[3]=hevc
codec[4]=h264
codec[5]=hevc
codec[6]=h264
codec[7]=hevc
codec[8]=h264
codec[9]=hevc

md5sums[0]=39ce5d637ed83cad6bd20f98e57fd239
md5sums[1]=b24fe8a79e98f3a876611eab74f5d4fb
md5sums[2]=0173f8f739d582ce4d41a5917c085b0e
md5sums[3]=44aedcb5f37f1aa7ca47795788d21121
md5sums[4]=78ae12671e49f22a47e26b1c71f93144
md5sums[5]=0b808986c001439350f3f5fb9c5ee3e2
md5sums[6]=1699687e20b959323e950ca62975a470
md5sums[7]=87b00a6b3ae108570221e2ad4feb55eb
md5sums[8]=275ed29ce9724aaf1b93d48b189104e3
md5sums[9]=0246a3264fe34f2d71364965959a9e6b

file_num=$((${#filenames[@]} - 1))
output_path=./video_dec_out
log_path=$output_path/decode_test.log

dieID=0

mkdir -p $output_path

if [ -z "$1" ]
then
  echo "Default:"
  repeat=0
  random=0
else
  repeat=$1
  random=$1
fi

function rand(){
  min=$1
  max=$(($2-$min+1))
  num=$(($RANDOM+1000000000)) 
  echo $(($num%$max+$min))
}

echo "repeat =" $repeat
echo "random =" $random


while true
do {

echo "decode test start ("${#filenames[@]}") files:" 
echo '----------------------------------------------'
for i in $(seq 0 $file_num)
do
    #echo "--"$i
    #echo ${filenames[i]}
    if (( $random > 0 )); then
        i=$(rand 0 $file_num)
    fi
    echo $i":decoding: "${filenames[i]}
    #continue
    input_file=${filenames[i]}
    input_base=$(basename $input_file)
    file_name=${input_base%.*}
    output_file=$output_path/${file_name}.yuv
    #echo output: $output_file
    #../../sample/build/out/transcode_mt -i $input_file -o $output_file -s 1 -m 1 -r /dev/vastai_video0 > ${log_path} 2>&1
    ../../sample/build/out/video_dec -i $input_file -c ${codec[i]} -o $output_file -s 1 -m 0 -d /dev/vastai_video0 > ${log_path} 2>&1
    
    result=`md5sum $output_file |  awk '{print $1}'`
    if [ $result = ${md5sums[i]} ]; then
        echo md5 check PASS!!!
        #rm $output_file
    else
        echo FAIL!!! $result '!=' ${md5sums[i]}
        repeat=0 
        #break
    fi
done

echo ----------------------------------------------
echo decode test end

if (( $repeat <= 0 )); then
    break
fi
}


done
