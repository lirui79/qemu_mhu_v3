#!/bin/bash

cur_dir=`pwd`
outdir=$cur_dir/jpeg_enc_out
logdir=$cur_dir/log

#parallel=6
p_num_for_each_die=(3 3 3 3 3 3 3 3    0 0 0 0 0 0 0 0)
inputfile="/home/vastai/resource/dataset/yuv/16K/126M_16384x16384.yuv -w 16384 -h 16384 -f nv12"

#parallel=24
#p_num_for_each_die=(6 6 6 6 6 6 6 6    0 0 0 0 0 0 0 0)
#inputfile="/home/vastai/resource/dataset/yuv/16K/126M_16384x16384.yuv -w 16384 -h 16384 -f nv12"

#parallel=12
#p_num_for_each_die=(6 6 0 0 0 0 0 0    0 0 0 0 0 0 0 0)
#inputfile=/home/vastai/resource/dataset/jpg/16K/126M.jpg


#inputfile=/home/vastai/resource/dataset/jpg/1080P/smile_1920x1080.jpg
#inputfile=../sample/resource/stream1.jpg

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
        #echo $i -- $die_num
        #../sample/build/out/jpeg_dec  -i $inputfile  -o ${outdir}/output-${die_num}-${i}.yuv  -d /dev/vastai_video${die_num} -l 0 -m 1 -s 0 >> log/log${die_num}_dec_$i 2>&1
        ../../sample/build/out/jpeg_enc -i $inputfile -o ${outdir}/output-${die_num}-${i}.jpg  -d /dev/vastai_video${die_num} -l 0 -m 1 -s 0 >> log/log${die_num}_enc_$i 2>&1
	}&
	done
	wait

	while [ 1 ]
	do
                result=`ps -ef |grep ../sample/build/out/jpeg_enc |grep -v grep| wc -l`
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
