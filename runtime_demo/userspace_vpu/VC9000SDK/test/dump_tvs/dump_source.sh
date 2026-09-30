#!/bin/bash

json=$1

output=`cat $json | jq -r '.output'`
mkdir -p $output

source_list=`cat $json | jq '.sources'`
len=`cat $json | jq '.sources|length'`
length=`expr $len - 1`
echo "sources length:$length "

function rand(){   
   min=$1
   max=$(($2-$min+1))
   num=$(cat /dev/urandom | head -n 10 | cksum | awk -F ' ' '{print $1}')
   echo $(($num%$max+$min))
}

function encode(){
  echo "=============================================="
  echo "    start to download source, length:$length "
  echo "=============================================="
  for index in $(seq 0 $length)
  do
    name=`echo $source_list | jq ".[$index].name" | sed 's/\"//g'`
    url=`echo $source_list | jq ".[$index].url" | sed 's/\"//g'`
    frames=$(rand 1000 20000)
    vname=${name}${index}
    echo "index:$index, name: $vname, u: $url"
    ffmpeg -i $url -c copy -vframes ${frames} ${vname}.mp4
    ffmpeg -i ${vname}.mp4 -an -vcodec copy -bsf:v h264_mp4toannexb ${vname}.h264
    rm ${vname}.mp4
  done
}

encode

echo "=============================================="
echo "    ended download source       "
echo "=============================================="
