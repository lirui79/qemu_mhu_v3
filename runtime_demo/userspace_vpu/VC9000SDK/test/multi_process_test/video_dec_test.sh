#!/bin/bash

cur_dir=`pwd`
outdir=$cur_dir/video_dec_out
logdir=$cur_dir/log

#inputfile=/home/vastai/resource/dataset/hevc/cdzj_1080p-hevc.mp4
#inputfile=/home/vastai/resource/dataset/h264/tlst_1080p-h264-0-10.h264
#inputfile=/home/vastai/resource/dataset/h264/xg1920x1088_x264_10.h264
#inputfile=/home/vastai/resource/dataset/h264/kaoya_3840x2160_123.h264
#inputfile=/data/centos/access_test/decode_input/kaoya.h264
#inputfile=/home/vastai/resource/dataset/h264/kaoya.h264
#inputfile=/home/vastai/resource/dataset/h264/ParkScene_1920x1080_30fps_loop_8M.h264
inputfile=/home/vastai/resource/dataset/h264/BQMall_832x480_60.h264
#inputfile=/home/vastai/resource/dataset/h264/cdzj-hevc-0-6.hevc

#parallel=22
#p_num_for_each_die=(3 3 3 3 3 3 2 2    0 0 0 0 0 0 0 0)

#parallel=48
#p_num_for_each_die=(6 6 6 6 6 6 6 6    0 0 0 0 0 0 0 0)

#parallel=96
p_num_for_each_die=(6 6 6 6 6 6 6 6   6 6 6 6 6 6 6 6)

#parallel=120 1080P
#p_num_for_each_die=(60 60 0 0 0 0 0 0    0 0 0 0 0 0 0 0)

#parallel=360 480P
p_num_for_each_die=(180 180 0 0 0 0 0 0    0 0 0 0 0 0 0 0)

for i in $(seq 0 15)
do {
echo  -n "${p_num_for_each_die[$i]} "
}
done
echo
total_p_num_for_each_die[0]=${p_num_for_each_die[0]}
echo  -n "${total_p_num_for_each_die[0]} "
for i in $(seq 1 15)
do {
total_p_num_for_each_die[$i]=$((${total_p_num_for_each_die[$i-1]} +  ${p_num_for_each_die[$i]} ))
echo  -n "${total_p_num_for_each_die[$i]} "
}
done
parallel=${total_p_num_for_each_die[15]}
echo 
echo prallel = $parallel

#stty -echo
mkdir -p $outdir
rm $outdir/*
mkdir -p $logdir
rm $logdir/*
#stty echo

while true
do
declare -i i

    for i in $(seq 1 $parallel)
        do {
        #echo $i
        #echo ${total_p_num_for_each_die[$i]}

        if (($i <= ${total_p_num_for_each_die[0]})) ;then
            die_num=0
        elif (($i <= ${total_p_num_for_each_die[1]})) ;then
            die_num=1
        elif (($i <= ${total_p_num_for_each_die[2]})) ;then
            die_num=2
        elif (($i <= ${total_p_num_for_each_die[3]})) ;then
            die_num=3
        elif (($i <= ${total_p_num_for_each_die[4]})) ;then
            die_num=4
        elif (($i <= ${total_p_num_for_each_die[5]})) ;then
            die_num=5
        elif (($i <= ${total_p_num_for_each_die[6]})) ;then
            die_num=6
        elif (($i <= ${total_p_num_for_each_die[7]})) ;then
            die_num=7
        elif (($i <= ${total_p_num_for_each_die[8]})) ;then
            die_num=8
        elif (($i <= ${total_p_num_for_each_die[9]})) ;then
            die_num=9
        elif (($i <= ${total_p_num_for_each_die[10]})) ;then
            die_num=10
        elif (($i <= ${total_p_num_for_each_die[11]})) ;then
            die_num=11
        elif (($i <= ${total_p_num_for_each_die[12]})) ;then
            die_num=12
        elif (($i <= ${total_p_num_for_each_die[13]})) ;then
            die_num=13
        elif (($i <= ${total_p_num_for_each_die[14]})) ;then
            die_num=14
        else
            die_num=15
        fi
        #echo $i -- $die_num
        ../../sample/build/out/video_dec -i $inputfile -o ${outdir}/output-${die_num}-${i}.yuv -d /dev/vastai_video${die_num} -c h264 -s 0 -m 0 -l 0 | tee log/log${die_num}_dec_$i
        #../../sample/build/out/transcode_mt -i $inputfile -o ${outdir}/output-${die_num}-${i}.yuv -s 0 -m 0 -l -1 -r /dev/vastai_video${die_num} -f 0 -c h264 | tee log/log${die_num}_dec_$i
    }&
    done
    wait

    while [ 1 ]
    do
                #result=`ps -ef |grep ../sample/build/out/video_dec |grep -v grep| wc -l`
                result=`ps -ef |grep ../sample/build/out/transcode_mt |grep -v grep| wc -l`
                if [ $result -eq 0 ]; then
                        echo "all thread exited, start next!!!"
                        #md5sum $outdir/*
                        break
                else
                        echo "waiting decoding exit!!!"
                        sleep 1
                fi
    done

break
done
