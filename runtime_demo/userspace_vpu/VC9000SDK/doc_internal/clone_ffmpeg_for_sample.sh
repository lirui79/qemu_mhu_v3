#!/bin/bash

clone_ffmpeg(){
    local dir="$1"
    echo "\e[36m-- Clone source code from: http://192.168.20.70/hbliu/FFmpeg4SDKSample.git into $dir\e[0m"
    git clone http://192.168.20.70/hbliu/FFmpeg4SDKSample.git
    mv FFmpeg4SDKSample/ $dir
}