#!/bin/bash

cur_dir=`pwd`
outdir=$cur_dir/video_enc_out
logdir=$cur_dir/log

#inputfile="/home/vastai/resource/dataset/yuv/1080P/xg1920x1088_x265_121_num.yuv -w 1920 -h 1088"
inputfile="/home/vastai/resource/dataset/yuv/1080P/ParkScene_1920x1080_30fps_loop_8M.yuv -w 1920 -h 1080"

#inputfile="/data/resource/yuv/1080P/xg1920x1088_x265_121_num.yuv -w 1920 -h 1088"

#parallel=24
#p_num_for_each_die=(3 3 3 3 3 3 3 3    0 0 0 0 0 0 0 0)

#parallel=48
#p_num_for_each_die=(6 6 6 6 6 6 6 6    0 0 0 0 0 0 0 0)

#parallel=64
#p_num_for_each_die=(8 8 8 8 8 8 8 8    0 0 0 0 0 0 0 0)

#parallel=10
#p_num_for_each_die=(5 5 0 0 0 0 0 0    0 0 0 0 0 0 0 0)

#parallel=64
p_num_for_each_die=(32 32 0 0 0 0 0 0    0 0 0 0 0 0 0 0)

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

echo start encoding

while true
do
declare -i i

	for i in $(seq 1 $parallel)
        do {
		#echo $i
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
        else
            die_num=7
        fi

        if (($i <= ${total_p_num_for_each_die[3]})) ;then
             codec_type=hevc
        else
             codec_type=h264
        fi
 
        #echo $i -- $die_num
        #../sample/build/out/video_enc -i $inputfile -o ${outdir}/output-${die_num}-${i}.h264 -d /dev/vastai_video${die_num} -c 0 -l 65535 -s 0 >> log/log${die_num}_enc_$i 2>&1
        ../../sample/build/out/video_enc -i $inputfile -o ${outdir}/output-${die_num}-${i}.h264 -d /dev/vastai_video${die_num} -c 0 -l 65535 -s 0 --bitRate 8000000 | tee log/log${die_num}_enc_$i
        #../../sample/build/out/transcode_mt -i /home/vastai/resource/dataset/h264/cdzj-hevc-0-6.hevc -o ${outdir}/output-${die_num}-${i}.h265 -s 0 -r /dev/vastai_video${die_num} -C $codec_type -l 65535 -p 1000  >> log/log${die_num}_enc_$i 2>&1
	}&
	done
	wait

	while [ 1 ]
	do
                result=`ps -ef |grep ../sample/build/out/video_enc |grep -v grep| wc -l`
                if [ $result -eq 0 ]; then
                        echo "all" $parallel "threads exited!"
                        md5sum $outdir/*
       			break
                else
                        echo "waiting decoding exit!!!"
                        sleep 1
                fi
	done

break
done
