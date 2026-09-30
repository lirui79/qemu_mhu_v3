#!/bin/bash
VIDEO_DATASET_PATH=/data/qa-data/
device_id=0
dev0=0
dev1=1
encodepath=enc_out
encode_log_path=log
vmpp_bin_path=../../sample/build/out/video_enc

mkdir -p $encodepath
mkdir -p $encode_log_path

function run_encode_Test() {
  echo "test"
  cmdfile=cmd_${device_id}.log
  #delete old command log
  if [[ -f ${cmdfile} ]]; then
    rm -f ${cmdfile}
  fi
  # 0/1 h264/hevc
  codec_type_list=(0  1)
  keyint_list_all=(0 1 48 60 100 120 260)
  keyint_list=(0 1)
  index=0
  while [ $index -lt 100 ];do
  echo "round ${index} start!!!!!!!!!!!!!!!!!!" >> ${cmdfile}
  for line in $(cat $1); do
    # get input test data
    # echo testdata $i:$line
    filename=$(echo $line | cut -d ',' -f2)
    folderpath=$(echo $line | cut -d ',' -f3)
    raw_yuv_path=${VIDEO_DATASET_PATH}/${folderpath}/${filename}
    inputfile=${raw_yuv_path}
    width=$(echo $line | cut -d ',' -f4)
    height=$(echo $line | cut -d ',' -f5)
    pix_fmt=$(echo $line | cut -d ',' -f6)
    echo $raw_yuv_path $width $height $pix_fmt

    #if [ "$pix_fmt" == "nv12" ];then
    #  pix_fmt=0
    #fi
    #if [ "$pix_fmt" == "yuv420p" ];then
    #  pix_fmt=1
    #fi
    
    if [[ -f $raw_yuv_path ]]; then
      channel=16
      # run vame_encode
       ##codec parameter
      quality="BRONZE"
      p2b=0
      gopsie=1
      lookaheadLength=0
      for keyint_each in ${keyint_list[@]}; do
        for codec_type in ${codec_type_list[@]}; do
          for i in $(seq 1 $channel); do
            #set outputfile
            if [ "$codec_type" == "0" ];then
              suffix="h264"
            fi
            if [ "$codec_type" == "1" ];then
              suffix="hevc"
            fi
            outputfile1=${encodepath}/${filename}_${dev0}-${pix_fmt}-${quality}-p2b-${p2b}-keyint-${keyint_each}-round-${index}-channel-${i}.${suffix}
            logfile1=${encode_log_path}/${filename}_${dev0}-${pix_fmt}-${quality}-p2b-${p2b}-keyint-${keyint_each}-round-${index}-channel-${i}.log

            # run test
            ${vmpp_bin_path} -i ${inputfile} -d /dev/vastai_video${dev0} -c ${codec_type} -l 1 --keyInt ${keyint_each} --gopSize ${gopsie} \
            --lookaheadDepth ${lookaheadLength}  -w $width -h $height -s 1  -f $pix_fmt  -o ${outputfile1} | tee ${logfile1} &

            #../../sample/build/out/video_enc -d /dev/vastai_video0 -i /data/qa-data/video/vidyo1_1280x720_60.yuv -c 1 -l 1 --keyInt 120 --gopSize 0 \
            #-w 1920 -h 1080 -s 1 -f yuv420p -o Kimono1_sdk.265

             #echo cmd
            echo " ${vmpp_bin_path} -i ${inputfile} -d /dev/vastai_video${dev0} -c ${codec_type} -l 1 --keyInt ${keyint_each} --gopSize ${gopsie} \
            --lookaheadDepth ${lookaheadLength}  -w $width -h $height -s 1  -f $pix_fmt  -o ${outputfile1} " >> ${cmdfile}

            # rm -f ${outputfile}
            # check_encode_result $outputfile $width $height
            ## if device is 2-die, run test on die 1.
            if [ $dev1 ]; then
              #set outputfile
              outputfile2=${encodepath}/${filename}_${dev1}-${pix_fmt}-${quality}-p2b-${p2b}-keyint-${keyint_each}-round-${index}-channel-${i}.${suffix}
              logfile2=${encode_log_path}/${filename}_${dev1}-${pix_fmt}-${quality}-p2b-${p2b}-keyint-${keyint_each}-round-${index}-channel-${i}.log
              # run test
              ${vmpp_bin_path} -i ${inputfile} -d /dev/vastai_video${dev0} -c ${codec_type} -l 1 --keyInt ${keyint_each} --gopSize ${gopsie} \
              --lookaheadDepth ${lookaheadLength}  -w $width -h $height -s 1  -f $pix_fmt  -o ${outputfile2}  | tee ${logfile2} &
              #echo cmd
              echo "${vmpp_bin_path} -i ${inputfile} -d /dev/vastai_video${dev0} -c ${codec_type} -l 1 --keyInt ${keyint_each} --gopSize ${gopsie} \
              --lookaheadDepth ${lookaheadLength}  -w $width -h $height -s 1  -f $pix_fmt  -o ${outputfile2}" >> ${cmdfile}
              # rm -f ${outputfile}
              # check_encode_result $outputfile $width $height
            fi
          done #end channel
        done #end keyint
        wait 
        echo "${channel} encode ${outputfile1} end" 
        echo "${channel} encode ${outputfile2} end" 
        echo "${channel} encode ${outputfile1} end" >> ${cmdfile}
        echo "${channel} encode ${outputfile2} end" >> ${cmdfile}
        rm -f ${encodepath}/*.h264
        rm -f ${encodepath}/*.hevc
      done
    fi
  done
  wait
  echo "round ${index} end!!!!!!!!!!!!!!!!!!" >> ${cmdfile}
  index=$(($index+1))
done
}

run_encode_Test test.csv.txt