#!/bin/bash

# echo color (\033、\e和\E)
RED='\E[1;31m'
GREEN='\E[1;32m'
YELLOW='\E[1;33m'
WHITE='\E[1;37m'
CLR='\E[0m'

# package echo
echo_with_color(){
    echo -e "$1$2$CLR"
    if [ "$3" ];then
        echo "$2" >> "$3"
    fi
}

kill_local(){
    echo_with_color $YELLOW " --->>> receive SIGINT, script will exit..."
    echo_with_color $GREEN " --->>> Bye!!!"
    exit
}


trap kill_local SIGINT

echo_with_color $GREEN " --->>> Run Process Killing Script..."

let killLoop=0

while [ 1 ]
do
    process_num=`ps -ef | grep "transcode_mt" | grep -v grep | wc -l`
    if [ $process_num -eq 0 ];then
        echo_with_color $GREEN " --->>> all 'transcode_mt' processes have been killed, or no 'transcode_mt' process is running!!!"
    else
        let idx=0
        let rdmProcIdx=$((RANDOM%$process_num))
        IDs=`ps -ef | grep "transcode_mt" | grep -v "grep" | awk '{print $2}'`
        for id in $IDs
        do
            if [ $idx -eq $rdmProcIdx ];then
                kill -9 $id
                echo_with_color $GREEN "kill 'transcode_mt' process ----->>>>> [ Number ${rdmProcIdx} PID $id TOTAL $process_num ]"
                break
            fi
            ((idx++))
        done
    fi
    ((killLoop++))
    let rdmSleep=$((RANDOM%3+1))
    echo_with_color $WHITE "sleep ${rdmSleep} second(s)"
    sleep $rdmSleep;
    let rdmBase=$((RANDOM%10+3))
    let temp=$(($killLoop%$rdmBase))
    if [ $killLoop -gt 0 ] && [ $temp -eq 0 ];then
        echo_with_color $YELLOW "----->>>>> rdm $rdmBase ----->>>>> KILL ALL 'transcode_mt' PROCs [ TOTAL $process_num ]"
        killall transcode_mt
        sleep 1;
    fi
done