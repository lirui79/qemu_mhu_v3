#!/usr/bin/env sh

RELEASE_BUILD="0"
BUILD_TARGET="0"
USER_HOST_MEM="0"
ENABLE_DYNAMIC_RES="0"
ENABLE_ANDROID_ARM64="0"
BUILD_ARM64="0"
USING_FFMPEG="1"

if [ $# -eq 0 ];then
   RELEASE_BUILD="0"
   BUILD_TARGET="0"
   USER_HOST_MEM="0"
   ENABLE_DYNAMIC_RES="0"
else
   while getopts "t:r:u:d:m:a:f:" opt; do
      case $opt in
         t)
            BUILD_TARGET=$OPTARG
            echo "\e[1;36m-- BUILD_TARGET:$BUILD_TARGET \e[0m"
            ;;
         r)
            RELEASE_BUILD=$OPTARG
            echo "\e[1;36m-- RELEASE_BUILD:$RELEASE_BUILD \e[0m"
            ;;
         u)
            USER_HOST_MEM=$OPTARG
            echo "\e[1;36m-- USER_HOST_MEM:$USER_HOST_MEM \e[0m"
            ;;
         d)
            ENABLE_DYNAMIC_RES=$OPTARG
            echo "\e[1;36m-- ENABLE_DYNAMIC_RES:$ENABLE_DYNAMIC_RES \e[0m"
            ;;
         m)
            ENABLE_ANDROID_ARM64=$OPTARG
            echo "\e[1;36m-- ENABLE_ANDROID_ARM64:$ENABLE_ANDROID_ARM64 \e[0m"
            ;;
         a)
            BUILD_ARM64=$OPTARG
            echo "\e[1;36m-- BUILD_ARM64:$BUILD_ARM64 \e[0m"
            ;;
         f)
            USING_FFMPEG=$OPTARG
            echo "\e[1;36m-- USING_FFMPEG:$USING_FFMPEG \e[0m"
            ;;
         ?)
            echo "\e[31m-- Invalid parameter!!! \e[0m"
            echo "\e[31m-- Usage: ./autobuild.sh -t 0 -r 1 \e[0m"
            echo "\e[31m--   t(target)             ==0 : build both encoder &decoder samples\e[0m"
            echo "\e[31m--                         ==1 : build only decoder samples \e[0m"
            echo "\e[31m--                         ==2 : build only encoder samples \e[0m"
            echo "\e[31m--   r(release)            ==1 : release mode \e[0m"
            echo "\e[31m--                         ==0 : debug mode \e[0m"
            echo "\e[31m--   u(user_mem)           ==1 : use user provided host memory  \e[0m"
            echo "\e[31m--                         ==0 : use SDK provided host memory \e[0m"
            echo "\e[31m--   d(dynamic resolution) ==1 : enable dynamic resolution \e[0m"
            echo "\e[31m--                         ==0 : disable dynamic resolution \e[0m"
            echo "\e[31m--   m(android arm64)      ==1 : for Android ARM64 compile  \e[0m"
            echo "\e[31m--                         ==0 : not Android ARM64 compile \e[0m"
            echo "\e[31m--   a(arm64)              ==1 : for ARM64 compile  \e[0m"
            echo "\e[31m--                         ==0 : not ARM64 compile \e[0m"
            echo "\e[31m--   f(using ffmpeg)       ==1 : using ffmpeg  \e[0m"
            echo "\e[31m--                         ==0 : not using ffmpeg \e[0m"
            exit
            ;;
      esac
   done
fi

echo "\e[1;36m-- BUILD_TARGET:$BUILD_TARGET, RELEASE_BUILD:$RELEASE_BUILD, ENABLE_DYNAMIC_RES:$ENABLE_DYNAMIC_RES \e[0m"

SAMPLE_WORKDIR=$(cd $(dirname $0); pwd)
build_dir="build"

if [ ! -d "$build_dir" ]; then
   mkdir $build_dir
   echo "\e[36m-- Make sample build/ directory \e[0m"
else
   rm -rf $build_dir/*
   echo "\e[36m-- Empty sample build/ directory \e[0m"
fi

if [ $BUILD_ARM64 = "1" ];then
   if [ $ENABLE_ANDROID_ARM64 = "1" ];then
        NDK=/opt/android-ndk-r21e
        TOOLCHAIN=$NDK/toolchains/llvm/prebuilt/linux-x86_64
        API=28
        TARGET=aarch64-linux-android
        export CC=$TOOLCHAIN/bin/$TARGET$API-clang
        export AR=$TOOLCHAIN/bin/llvm-ar

        cp ./3rdparty/ffmpeg/lib/android/*.a  ./3rdparty/ffmpeg/lib/
   else
         cp ./3rdparty/ffmpeg/lib/arm_64/*.a  ./3rdparty/ffmpeg/lib/
   fi
else
   cp ./3rdparty/ffmpeg/lib/x86_64/*.a  ./3rdparty/ffmpeg/lib/
fi

#compile libvmpp-dec & libvmpp-enc samples
cd $SAMPLE_WORKDIR/$build_dir
if [ $RELEASE_BUILD = "1" ];then
   echo "\e[36m-- Release Mode \e[0m"
   cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TARGET=$BUILD_TARGET -DMEM_ONLY_DEV_CHECK=$USER_HOST_MEM \
      -DENABLE_DYNAMIC_RES=$ENABLE_DYNAMIC_RES -DENABLE_ANDROID_ARM64=$ENABLE_ANDROID_ARM64 -DUSING_FFMPEG=$USING_FFMPEG
else
   echo "\e[36m-- Debug Mode \e[0m"
   cmake .. -DBUILD_TARGET=$BUILD_TARGET -DMEM_ONLY_DEV_CHECK=$USER_HOST_MEM -DENABLE_DYNAMIC_RES=$ENABLE_DYNAMIC_RES \
      -DENABLE_ANDROID_ARM64=$ENABLE_ANDROID_ARM64 -DBUILD_ARM64=$BUILD_ARM64 -DUSING_FFMPEG=$USING_FFMPEG
fi
echo "\e[36m-- Start making libvmpp-dec | libvmpp-enc samples\e[0m"
#cd $SAMPLE_WORKDIR/$build_dir
#make clean
make
if [ $? -ne "0" ]; then
   echo "\e[31m-- Build libvmpp-dec | libvmpp-enc samples failed!!! \e[0m"
   exit
else
   echo "\e[1;36m-- Build libvmpp-dec | libvmpp-enc samples finished!!! \e[0m"
fi
