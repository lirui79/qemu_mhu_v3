#!/usr/bin/env sh

RELEASE_BUILD="0"
BUILD_TARGET="0"
SANITIZER_MODE="0"
USER_HOST_MEM="0"
ENABLE_DYNAMIC_RES="0"
ENABLE_ANDROID_ARM64="0"
BUILD_ARM64="0"
USING_FFMPEG="1"
REBUILD_FFMPEG_FROM_SRC="0"
BUILD_32="0"
STATIC_LINK="1"

if [ $# -eq 0 ];then
   echo "para num is 0."
   RELEASE_BUILD="0"
   BUILD_TARGET="0"
   SANITIZER_MODE="0"
   USER_HOST_MEM="0"
   ENABLE_DYNAMIC_RES="0"
else
   echo "para num is not 0."
   while getopts "t:r:s:u:d:m:a:f:b:F:S:" opt; do
      case $opt in
         t)
            BUILD_TARGET=$OPTARG
            echo "\e[1;36m-- BUILD_TARGET:$BUILD_TARGET \e[0m"
            ;;
         r)
            RELEASE_BUILD=$OPTARG
            echo "\e[1;36m-- RELEASE_BUILD:$RELEASE_BUILD \e[0m"
            ;;
         s)
            SANITIZER_MODE=$OPTARG
            echo "\e[1;36m-- SANITIZER_MODE:$SANITIZER_MODE \e[0m"
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
         F)
            REBUILD_FFMPEG_FROM_SRC=$OPTARG
            echo "\e[1;36m-- REBUILD_FFMPEG_FROM_SRC:$REBUILD_FFMPEG_FROM_SRC \e[0m"
            ;;
         b)
            BUILD_32=$OPTARG
            echo "\e[1;36m-- BUILD_32:$BUILD_32 \e[0m"
            ;;
         S)
            STATIC_LINK=$OPTARG
            echo "\e[1;36m-- STATIC_LINK:$STATIC_LINK \e[0m"
            ;;
         ?)
            echo "\e[31m-- Invalid parameter!!! \e[0m"
            echo "\e[31m-- Usage: ./autobuild.sh -t 0 -r 1 -s 0 \e[0m"
            echo "\e[31m--   t(target)             ==0 : build both encoder &decoder samples\e[0m"
            echo "\e[31m--                         ==1 : build only decoder samples \e[0m"
            echo "\e[31m--                         ==2 : build only encoder samples \e[0m"
            echo "\e[31m--   r(release)            ==1 : release mode \e[0m"
            echo "\e[31m--                         ==0 : debug mode \e[0m"
            echo "\e[31m--   s(SANITIZER_MODE)     ==0 : disable Sanitizer(ASan/TSan)      [default]\e[0m"
            echo "\e[31m--                         ==1 : enable AddressSanitizer \e[0m"
            echo "\e[31m--                         ==2 : enable ThreadSanitizer \e[0m"
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
            echo "\e[31m--   F(rebuild ffmpeg)     ==1 : clone and rebuild FFmpeg from source code \e[0m"
            echo "\e[31m--                         ==0 : not rebuild ffmpeg \e[0m"
            echo "\e[31m--   b(build 32bit or 64bit)    ==1 : build 32bit \e[0m"
            echo "\e[31m--                              ==0 : build 64bit \e[0m"
            echo "\e[31m--   S(static link)             ==1 : link libvmpp-*.a              [default]\e[0m"
            echo "\e[31m--                              ==0 : link libvmpp-*.so \e[0m"
            exit
            ;;
      esac
   done
fi

echo "\e[1;36m-- BUILD_TARGET:$BUILD_TARGET, RELEASE_BUILD:$RELEASE_BUILD, SANITIZER_MODE:$SANITIZER_MODE, ENABLE_DYNAMIC_RES:$ENABLE_DYNAMIC_RES \e[0m"

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
        if [ $BUILD_32 = "0" ];then
            TARGET=aarch64-linux-android
        else
            TARGET=armv7a-linux-androideabi
        fi
        export CC=$TOOLCHAIN/bin/$TARGET$API-clang
        export AR=$TOOLCHAIN/bin/llvm-ar

        cp ./3rdparty/ffmpeg/lib/android/*.a  ./3rdparty/ffmpeg/lib/
   else
        cp ./3rdparty/ffmpeg/lib/arm_64/*.a  ./3rdparty/ffmpeg/lib/
   fi
else
   cp ./3rdparty/ffmpeg/lib/x86_64/*.a  ./3rdparty/ffmpeg/lib/
fi

# ----- re-build FFmpeg -----
cd $SAMPLE_WORKDIR
FFDIR="TEMP_FFDir"
FFSRC="Src"
FFOUT="Out"
FFCLONE_SCRIPT="../doc_internal/clone_ffmpeg_for_sample.sh"
if [ -d $FFDIR ]; then
   echo "\e[36m-- Remove the existing temporary directory for FFmpeg compiling: '$FFDIR' \e[0m"
   rm -rf $FFDIR
fi

# import the clone script
if [ -f $FFCLONE_SCRIPT ]; then
   . ../doc_internal/clone_ffmpeg_for_sample.sh
fi

rebuild_ffmpeg(){
   # Please replace the clone function with your own if it's necessary
   clone_ffmpeg $FFSRC

   if [ ! -d $FFSRC ]; then
      echo "\e[31m-- FFmpeg source code is unavailable! \e[0m"
      return 0
   fi

   # The pre-compiled FFmpeg is built from FFmpeg-n8.0 "Huffman", configuration may need to be adjusted for other version
   echo "\e[36m-- Do the Configuration for FFmpeg \e[0m"
   ./$FFSRC/configure --prefix=./$FFOUT --enable-gpl --enable-version3 --disable-autodetect --disable-x86asm               \
      --disable-shared --enable-static --disable-programs --disable-optimizations --disable-stripping                      \
      --disable-doc --disable-avdevice --disable-avfilter --disable-swresample --disable-hwaccels                          \
      --disable-encoders --disable-decoders --enable-decoder=h264 --enable-decoder=hevc --enable-decoder=av1               \
      --enable-decoder=avs --enable-decoder=flv --enable-decoder=mjpeg --enable-decoder=vp8 --enable-decoder=vp9           \
      --disable-muxers --enable-muxer=h264 --enable-muxer=hevc --enable-muxer=matroska --enable-muxer=mjpeg                \
      --enable-muxer=flv --enable-muxer=mp4 --enable-muxer=rawvideo --disable-demuxers --enable-demuxer=av1                \
      --enable-demuxer=avi --enable-demuxer=flv --enable-demuxer=avs --enable-demuxer=avs2 --enable-demuxer=avs3           \
      --enable-demuxer=h264 --enable-demuxer=hevc --enable-demuxer=hls --enable-demuxer=ivf --enable-demuxer=m4v           \
      --enable-demuxer=matroska --enable-demuxer=mjpeg --enable-demuxer=mov --enable-demuxer=obu --enable-demuxer=rawvideo \
      --enable-demuxer=webm_dash_manifest --enable-demuxer=mpegps --enable-demuxer=mpegts --enable-demuxer=mpegtsraw       \
      --enable-demuxer=mpegvideo --disable-protocols --enable-protocol=file --disable-iconv --disable-bzlib --disable-xlib \
      --disable-zlib --disable-sdl2 --disable-amf --disable-cuda-llvm --disable-cuvid --disable-ffnvcodec --disable-libdrm \
      --disable-nvenc --disable-vaapi --disable-vdpau --disable-vulkan --disable-v4l2-m2m

   echo "\e[36m-- Make && Install \e[0m"
   make -j && make install
   return 1
}

if [ $REBUILD_FFMPEG_FROM_SRC = "1" ];then
   echo "\e[36m-- Rebuild FFmpeg \e[0m"
   mkdir $FFDIR
   cd $FFDIR
   rebuild_ffmpeg
   if [ $? -eq 1 ]; then
      echo "\e[36m-- Copy compiled FFmpeg libs \e[0m"
      cp ./$FFOUT/lib/*.a  ../3rdparty/ffmpeg/lib/
      echo "\e[36m-- Copy new FFmpeg header files \e[0m"
      cp -r ./$FFOUT/include  ../3rdparty/ffmpeg/include
      if [ $USING_FFMPEG = "0" ];then
         echo "\e[36m-- Enable USING_FFMPEG \e[0m"
         USING_FFMPEG="1"
      fi
   else
      echo "\e[31m-- Rebuild FFmpeg from source code failed! \e[0m"
   fi
   cd -
fi
# ----- re-build FFmpeg END -----

#compile libvmpp-dec & libvmpp-enc samples
cd $SAMPLE_WORKDIR/$build_dir
if [ $RELEASE_BUILD = "1" ];then
   echo "\e[36m-- Release Mode \e[0m"
   cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TARGET=$BUILD_TARGET \
      -DENABLE_DYNAMIC_RES=$ENABLE_DYNAMIC_RES -DENABLE_ANDROID_ARM64=$ENABLE_ANDROID_ARM64 -DUSING_FFMPEG=$USING_FFMPEG \
      -DLINK_STATIC=$STATIC_LINK
else
   echo "\e[36m-- Debug Mode \e[0m"
   cmake .. -DBUILD_TARGET=$BUILD_TARGET -DSANITIZER_MODE=$SANITIZER_MODE -DENABLE_DYNAMIC_RES=$ENABLE_DYNAMIC_RES \
      -DENABLE_ANDROID_ARM64=$ENABLE_ANDROID_ARM64 -DBUILD_ARM64=$BUILD_ARM64 -DUSING_FFMPEG=$USING_FFMPEG \
      -DLINK_STATIC=$STATIC_LINK
fi
echo "\e[36m-- Start making libvmpp-dec | libvmpp-enc samples\e[0m"
#cd $SAMPLE_WORKDIR/$build_dir
#make clean
make -j
if [ $? -ne "0" ]; then
   echo "\e[31m-- Build libvmpp-dec | libvmpp-enc samples failed!!! \e[0m"
   exit
else
   echo "\e[1;36m-- Build libvmpp-dec | libvmpp-enc samples finished!!! \e[0m"
fi
