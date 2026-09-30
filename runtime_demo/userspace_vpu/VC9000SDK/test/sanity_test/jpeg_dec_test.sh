#!/bin/bash
filenames[0]=/home/vastai/resource/dataset/jpg/16K/126M.jpg
filenames[1]=/home/vastai/resource/dataset/jpg/4K/smile_3840x2160.jpg
filenames[2]=/home/vastai/resource/dataset/jpg/480P/smile_640x480.jpg
filenames[3]=/home/vastai/resource/dataset/jpg/special/smile_642x482.jpg
filenames[4]=/home/vastai/resource/dataset/jpg/special/smile_644x484.jpg
filenames[5]=/home/vastai/resource/dataset/jpg/special/smile_646x486.jpg
filenames[6]=/home/vastai/resource/dataset/jpg/special/smile_648x488.jpg
filenames[7]=/home/vastai/resource/dataset/jpg/720P/smile_1280x720.jpg
filenames[8]=/home/vastai/resource/dataset/jpg/1080P/smile_1920x1080.jpg
filenames[9]=/home/vastai/resource/dataset/jpg/1080P/seefood_1920x1080.jpg
filenames[10]=/home/vastai/resource/dataset/jpg/16K/16384_part1_16382x16382.jpg
filenames[11]=/home/vastai/resource/dataset/jpg/16K/16384_part1_16380x16380.jpg
filenames[12]=/home/vastai/resource/dataset/jpg/16K/16384_part1_16378x16378.jpg
filenames[13]=/home/vastai/resource/dataset/jpg/16K/16384_part1_16376x16376.jpg
filenames[14]=/home/vastai/resource/dataset/jpg/32K/vastai_32768x32768.jpg
filenames[15]=/home/vastai/resource/dataset/jpg/32K/16384_part1_32768x32768.jpg
filenames[16]=/home/vastai/resource/dataset/jpg/special/smile_641x481.jpg
filenames[17]=/home/vastai/resource/dataset/jpg/special/smile_639x479.jpg
filenames[18]=/home/vastai/resource/dataset/jpg/special/smile_299x200.jpg
filenames[19]=/home/vastai/resource/dataset/jpg/special/smile_300x201.jpg
filenames[20]=/home/vastai/resource/dataset/jpg/special/femal_yuv422_640x480.jpg

md5sums[0]=04460124ba7d2cb07adad0b3383483d7
md5sums[1]=bc3e86072a66f93e0ad347eb3ff02352
md5sums[2]=4c7d3d9209b76a738a91a56e20f11839
md5sums[3]=5a77af5a2fcfb8fda0cd430eaafce0a6
md5sums[4]=183099692a6417b6e2455e022b391172
md5sums[5]=86305e8489201526f3ddb087a64b12d0
md5sums[6]=2b2394cb2d0be8b593afc69462deec30
md5sums[7]=5530152dce9208cadbcb20aec9ff3936
md5sums[8]=08a77c009656387091d0cb5dfc0b5951
md5sums[9]=4495183e039a51a5f673b7837f3fbe98
md5sums[10]=5813b825b90e8462470573507b7a04ee
md5sums[11]=e27a6bf63ff54622c413b7859c6f672f
md5sums[12]=464c41d30d7305b922bd394c2d888e4c
md5sums[13]=c62412ea55bba963c3c3737f49563711
md5sums[14]=5c62d94a219783e1f5b66bc01b28da9f
md5sums[15]=c16f0944e159c89fa558448b077fcc82
md5sums[16]=dbbe54eac58ce4d3221bd084141b4d4a
md5sums[17]=5c140f53a18d1b02bac58156f7bd9d84
md5sums[18]=aabeda430bad44ef58aa2eeeda1a7a52
md5sums[19]=494c08bda05e8133fba2751671bdf512
md5sums[20]=2f8dc3661825f074776adf1d53c07568


file_num=$((${#filenames[@]} - 1))
output_path=./jpeg_dec_out
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
    #ffmpeg -y -hwaccel_device:v /dev/dri/renderD${dieID} -hwaccel vaapi  -i $input_file  $output_file > ${log_path} 2>&1
    ../../sample/build/out/jpeg_dec -i $input_file -o $output_file -d /dev/vastai_video${dieID} > ${log_path} 2>&1
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
