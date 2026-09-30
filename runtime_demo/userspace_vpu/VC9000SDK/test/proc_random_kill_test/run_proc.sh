#!/bin/bash

# echo color (\033、\e和\E)
RED='\E[1;31m'
GREEN='\E[1;32m'
YELLOW='\E[1;33m'
WHITE='\E[1;37m'
CLR='\E[0m'

force_exit=0
dev_count=0
parallel=32
device_list=()

# package echo
echo_with_color(){
    echo -e "$1$2$CLR"
    if [ "$3" ];then
        echo "$2" >> "$3"
    fi
}

enum_dev(){
    local dir="$1"
    local match="$2"
    echo_with_color $WHITE ">>>>> Enumerate video devices in directory '$dir'"
    local index=0
    for file in ${dir}*
    do
        if [[ $file =~ $match ]];then
            echo_with_color $GREEN "found $file!"
            device_list[${index}]="$file"
            ((index++))
        fi
    done
    if [ $index -eq 0 ];then
        echo_with_color $YELLOW "-!!! WARN No '$match' device available."
    else
        echo_with_color $YELLOW "-Found $index video devices in current environment!!!"
    fi

    dev_count=${index}
}

kill_local(){
    force_exit=1
    echo_with_color $YELLOW " --->>> receive SIGINT, script will exit..."
    sleep 1;
    while [ 1 ]
    do
        if ps -ef | grep '[y]kill_proc' >/dev/null
        then
            echo "Process exists"
        else
            IDs=`ps -ef | grep "transcode_mt" | grep -v "grep" | awk '{print $2}'`
            for id in $IDs
            do {
                kill -9 $id
                echo_with_color $YELLOW "fast kill 'transcode_mt' process ----->>>>> [ PID $id ]"
            }
            done
            process_num=`ps -ef | grep "transcode_mt" | grep -v grep | wc -l`
            if [ $process_num -eq 0 ];then
                echo_with_color $GREEN " --->>> all 'transcode_mt' processes have been killed, or no 'transcode_mt' process is running!!!"
                break
            fi
        fi
    done
    echo_with_color $GREEN " --->>> Bye!!!"
    exit
}

trap kill_local SIGINT

echo_with_color $GREEN " --->>> Run Process Starting Script..."

enum_dev /dev/ va_video
if [ $dev_count -eq 0 ];then
    enum_dev /dev/ vastai_video
fi

if [ $dev_count -eq 0 ];then
    echo_with_color $RED "-!!! WARN No device available."
    exit
fi

inout_params="-i ../pvp/res/res_bs_1_1920_1088_3438c3cc0a27155fc0139b9323e35a84_.hevc -c hevc"
common_params="--logLevel 3 --logLevelSDK 3 -l 1 -L 100 -t 4"

while [ $force_exit -eq 0 ]
do
    process_num=`ps -ef | grep "transcode_mt" | grep -v grep | wc -l`
    if [ $process_num -lt $parallel ];then
        let devIdx=$((RANDOM%dev_count))
        ../../sample/build/out/transcode_mt -d ${device_list[${devIdx}]} ${inout_params} ${common_params} -s 0  &
        echo_with_color $GREEN "run transcode_mt with parameters: -d ${device_list[${devIdx}]} ${inout_params} ${common_params} -s 0 ----->>>>> [ PID $! TOTAL $(($process_num+1)) ]"
    else
        let rdmSleep=$((RANDOM%3+1))
        echo_with_color $WHITE "sleep ${rdmSleep} second(s)"
        sleep $rdmSleep
    fi
done