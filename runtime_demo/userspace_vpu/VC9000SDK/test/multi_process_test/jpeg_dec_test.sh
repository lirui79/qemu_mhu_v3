#!/bin/bash

cur_dir=`pwd`
outdir=$cur_dir/jpeg_dec_out
logdir=$cur_dir/log

#inputfile=../sample/resource/stream1.jpg

#parallel=22
#p_num_for_each_die=(3 3 3 3 3 3 2 2     0 0 0 0 0 0 0 0)
#inputfile=/home/vastai/resource/dataset/jpg/4K/smile_3840x2160.jpg

#parallel=48
#p_num_for_each_die=(6 6 6 6 6 6 6 6      0 0 0 0 0 0 0 0)
#inputfile=/home/vastai/resource/dataset/jpg/4K/smile_3840x2160.jpg

#parallel=12
p_num_for_each_die=(6 6 0 0 0 0 0 0     0 0 0 0 0 0 0 0)
#inputfile=/home/vastai/resource/dataset/jpg/16K/126M.jpg

#parallel=30
#p_num_for_each_die=(30 0 0 0 0 0 0 0    0 0 0 0 0 0 0 0)
#inputfile=/home/vastai/resource/dataset/jpg/1080P/smile_1920x1080.jpg

#parallel=48
#p_num_for_each_die=(24 24 0 0 0 0 0 0     0 0 0 0 0 0 0 0)
#inputfile=/home/vastai/resource/dataset/jpg/1080P/smile_1920x1080.jpg

inputfile=/home/vastai/resource/dataset/jpg/1080P/input.jpg

#parallel=24
p_num_for_each_die=(1 0 0 0 0 0 0 0    0 0 0 0 0 0 0 0)

#parallel=192
#p_num_for_each_die=(12 12 12 12 12 12 12 12     12 12 12 12 12 12 12 12)

#parallel=240
#p_num_for_each_die=(15 15 15 15 15 15 15 15     15 15 15 15 15 15 15 15)

#parallel=320
#p_num_for_each_die=(20 20 20 20 20 20 20 20     20 20 20 20 20 20 20 20)

#parallel=352
#p_num_for_each_die=(22 22 22 22 22 22 22 22      22 22 22 22 22 22 22 22)


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
        ../../sample/build/out/jpeg_dec  -i $inputfile  -o ${outdir}/output-${die_num}-${i}.yuv  -d /dev/vastai_video${die_num} -l 0 -m 0 -s 0 | tee log/log${die_num}_dec_$i
	}&
	done
	wait

	while [ 1 ]
	do
                result=`ps -ef |grep ../../sample/build/out/jpeg_dec |grep -v grep| wc -l`
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
