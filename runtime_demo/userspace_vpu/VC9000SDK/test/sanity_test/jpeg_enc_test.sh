#!/bin/bash
filenames[0]=/home/vastai/resource/dataset/yuv/480P/smile_640x480_0.yuv
filenames[1]=/home/vastai/resource/dataset/yuv/480P/smile_640x480_nv21.yuv 
filenames[2]=/home/vastai/resource/dataset/yuv/480P/smile_640x480_yuv420p.yuv
filenames[3]=/home/vastai/resource/dataset/yuv/special/smile_642x482_0.yuv
filenames[4]=/home/vastai/resource/dataset/yuv/special/smile_642x482_0.yuv
filenames[5]=/home/vastai/resource/dataset/yuv/480P/smile_640x480_0.yuv
filenames[6]=/home/vastai/resource/dataset/yuv/720P/smile_1280x720_0.yuv
filenames[7]=/home/vastai/resource/dataset/yuv/1080P/smile_1920x1080_0.yuv
filenames[8]=/home/vastai/resource/dataset/yuv/16K/126M_16384x16384.yuv

filenames[9]=/home/vastai/resource/dataset/yuv/special/smile_641x481.yuv
filenames[10]=/home/vastai/resource/dataset/yuv/special/smile_639x479_0_nv12.yuv
filenames[11]=/home/vastai/resource/dataset/yuv/special/smile_641x482_nv12.yuv
filenames[12]=/home/vastai/resource/dataset/yuv/special/smile_642x481_nv12.yuv
filenames[13]=/home/vastai/resource/dataset/yuv/special/smile_641x481_yuv420p.yuv
filenames[14]=/home/vastai/resource/dataset/yuv/special/smile_639x479_0_yuv420p.yuv
filenames[15]=/home/vastai/resource/dataset/yuv/special/smile_641x482_yuv420p.yuv
filenames[16]=/home/vastai/resource/dataset/yuv/special/smile_642x481_yuv420p.yuv

params[0]="-w 640 -h 480 -f nv12"
params[1]="-w 640 -h 480 -f nv21"
params[2]="-w 640 -h 480 -f yuv420p"
params[3]="-w 642 -h 482 -f nv12"
params[4]="-w 640 -h 482 -t 642"
params[5]="-w 638 -h 480 -t 640"
params[6]="-w 1280 -h 720 -f nv12"
params[7]="-w 1920 -h 1080 -f nv12"
params[8]="-w 16384 -h 16384 -f nv12"

params[9]="-w 641 -h 481 -f nv12"
params[10]="-w 639 -h 479 -f nv12"
params[11]="-w 641 -h 482 -f nv12"
params[12]="-w 642 -h 481 -f nv12"
params[13]="-w 641 -h 481 -f yuv420p"
params[14]="-w 639 -h 479 -f yuv420p"
params[15]="-w 641 -h 482 -f yuv420p"
params[16]="-w 642 -h 481 -f yuv420p"

md5sums[0]=ac5989879f60352be8ea0733544cbf1e
md5sums[1]=ac5989879f60352be8ea0733544cbf1e
md5sums[2]=ac5989879f60352be8ea0733544cbf1e
md5sums[3]=28e9cbd44dda0f8e76038c6d93d6e77b
md5sums[4]=ebca8578e750aa7a0e7f15d76263225e
md5sums[5]=394682613f947396479858e460364f33
md5sums[6]=314273c29b6b142f54a0a0e10e60426b
md5sums[7]=e315594642d9076c453a59e9e67e958f
md5sums[8]=7335e64bee6fcc5a0dc77db5cc58c61a

md5sums[9]=e0f9a89bb3e1e6998c32b79232ddf6c9
md5sums[10]=09f850a5476c0378cd4dfbf4643ebc73
md5sums[11]=9d1675258ce80bdc8609898ca9a0032a
md5sums[12]=83abba07e75f8551b6af136e59eb4cda
md5sums[13]=6ea14e125e73f7dce1e2d598c00c6b01
md5sums[14]=29d63185a38ec8e433f4b43b2dc15790
md5sums[15]=9d1675258ce80bdc8609898ca9a0032a
md5sums[16]=490cdd2ef0c9ea0e40d8a9ff2f0071d0

file_num=$((${#filenames[@]} - 1))
#file_num=1


output_path=./jpeg_enc_out
log_path=$output_path/encode_test.log

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

echo "encode test start ("${#filenames[@]}") files:" 
echo '----------------------------------------------'
for i in $(seq 0 $file_num)
do
    #echo "--"$i
    #echo ${filenames[i]}
    if (( $random > 0 )); then
        i=$(rand 0 $file_num)
    fi
    echo $i":encoding: "${filenames[i]}
    #continue
    input_file=${filenames[i]}
    input_base=$(basename $input_file)
    file_name=${input_base%.*}
    output_file=$output_path/${file_name}_$i.jpg
    #echo output: $output_file
    ../../sample/build/out/jpeg_enc -i $input_file -o $output_file ${params[i]} -m 1 -d /dev/vastai_video0 > ${log_path} 2>&1

    result=`md5sum $output_file |  awk '{print $1}'`
    if [ $result = ${md5sums[i]} ]; then
        echo md5 check PASS!!!
        #rm $output_file
    else
        echo FAIL!!! $result '!=' ${md5sums[i]}
        repeat=0 
        break
    fi
done

echo ----------------------------------------------
echo decode test end

if (( $repeat <= 0 )); then
    break
fi
}


done
