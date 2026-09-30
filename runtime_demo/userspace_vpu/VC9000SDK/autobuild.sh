#!/usr/bin/env sh

RELEASE_BUILD="0"
BUILD_TARGET="0"
SANITIZER_MODE="0"
ENABLE_DYNAMIC_RES="0"
BUILD_ARM64="0"
ENABLE_UNIT_TEST="0"
USING_FFMPEG="1"
VIDEO_COMPAT="y"
REBUILD_FFMPEG_FROM_SRC="0"
OPTIMIZED_DEBUG="0"
STATIC_LINK="1"

if [ $# -eq 0 ];then
   echo "\e[1;36m-- Build both decoder & encoder in DEBUG mode!!! \e[0m"
   RELEASE_BUILD="0"
   BUILD_TARGET="0"
   ADDRESS_SANITIZER="0"
   ENABLE_DYNAMIC_RES="0"
   ENABLE_UNIT_TEST="0"
else
   echo "Parameters count: $#"
   while getopts "t:r:s:u:d:a:U:F:O:S:" opt; do
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
         d)
            ENABLE_DYNAMIC_RES=$OPTARG
            echo "\e[1;36m-- ENABLE_DYNAMIC_RES:$ENABLE_DYNAMIC_RES \e[0m"
            ;;
         a)
            BUILD_ARM64=$OPTARG
            echo "\e[1;36m-- BUILD_ARM64:$BUILD_ARM64 \e[0m"
            ;;
         U)
            ENABLE_UNIT_TEST=$OPTARG
            echo "\e[1;36m-- ENABLE_UNIT_TEST:$ENABLE_UNIT_TEST \e[0m"
            ;;
         F)
            REBUILD_FFMPEG_FROM_SRC=$OPTARG
            echo "\e[1;36m-- REBUILD_FFMPEG_FROM_SRC:$REBUILD_FFMPEG_FROM_SRC \e[0m"
            ;;
         O)
            OPTIMIZED_DEBUG=$OPTARG
            echo "\e[1;36m-- OPTIMIZED_DEBUG:$OPTIMIZED_DEBUG \e[0m"
            ;;
         S)
            STATIC_LINK=$OPTARG
            echo "\e[1;36m-- STATIC_LINK:$STATIC_LINK \e[0m"
            ;;
         ?)
            echo "\e[31m-- Usage: ./autobuild.sh -t 0 -r 1 -s 0 -a 0 -d 0 -S 0 \e[0m"
            echo "\e[31m--   t(target)             ==0 : build both encoder & decoder      [default]\e[0m"
            echo "\e[31m--                         ==1 : build only decoder \e[0m"
            echo "\e[31m--                         ==2 : build only encoder \e[0m"
            echo "\e[31m--   r(release)            ==0 : debug mode                        [default]\e[0m"
            echo "\e[31m--                         ==1 : release mode \e[0m"
            echo "\e[31m--   s(SANITIZER_MODE)     ==0 : disable Sanitizer(ASan/TSan)      [default]\e[0m"
            echo "\e[31m--                         ==1 : enable AddressSanitizer \e[0m"
            echo "\e[31m--                         ==2 : enable ThreadSanitizer \e[0m"
            echo "\e[31m--   d(dynamic resolution) ==0 : disable dynamic resolution        [default]\e[0m"
            echo "\e[31m--                         ==1 : enable dynamic resolution \e[0m"
            echo "\e[31m--   a(build arm64)        ==0 : build x86_64                      [default]\e[0m"
            echo "\e[31m--                         ==1 : build arm64                                \e[0m"
            echo "\e[31m--   U(run unit test)      ==0 : do not run ut                     [default]\e[0m"
            echo "\e[31m--                         ==1 : run ut(effective when target==0)           \e[0m"
            echo "\e[31m--   F(rebuild ffmpeg)     ==1 : clone and rebuild FFmpeg for sample from source code \e[0m"
            echo "\e[31m--                         ==0 : not rebuild ffmpeg for sample     [default]\e[0m"
            echo "\e[31m--   O(optimized debug)    ==0 : disable optimized debug           [default]\e[0m"
            echo "\e[31m--                         ==1 : enable optimized debug \e[0m"
            echo "\e[31m--   S(static link)        ==0 : samples link libvmpp-*.so         \e[0m"
            echo "\e[31m--                         ==1 : build libvmpp-*.a, samples link it [default]\e[0m"
            exit
            ;;
      esac
   done
fi
echo "\e[1;36m-- BUILD_TARGET:$BUILD_TARGET, RELEASE_BUILD:$RELEASE_BUILD, SANITIZER_MODE:$SANITIZER_MODE \e[0m"

CUR_WORKDIR=$(cd $(dirname $0); pwd)
BUILD_DIR="build"
RELEASE_LIB="lib"

if [ ! -d "$BUILD_DIR" ]; then
   mkdir $BUILD_DIR
   echo "\e[36m-- Make build/ directory \e[0m"
else
   rm -rf $BUILD_DIR/*
   echo "\e[36m-- Empty build/ directory \e[0m"
fi

if [ ! -d "$RELEASE_LIB" ]; then
   mkdir $RELEASE_LIB
   echo "\e[36m-- Make lib/ directory \e[0m"
else
   rm -rf $RELEASE_LIB/*
   echo "\e[36m-- Empty lib/ directory \e[0m"
fi

if [ $BUILD_ARM64 = "1" ];then
   USING_FFMPEG="0"
   # VSI 底层库 + vmpp + sample 全部交叉编译，否则会出现
   # "Relocations in generic ELF (EM: 62) ... file in wrong format"（x86 .a 被 aarch64 ld 链接）
   : ${CROSS_COMPILE:=aarch64-linux-gnu-}
   export CC=${CROSS_COMPILE}gcc
   export CXX=${CROSS_COMPILE}g++
   # VSI 子 Makefile 里 CC/AR 是由 CROSS(_COMPILE) 推导的，必须走命令行覆盖
   DEC_MAKE_FLAGS="CROSS=${CROSS_COMPILE}"
   ENC_MAKE_FLAGS="CROSS_COMPILE=${CROSS_COMPILE}"
else
   DEC_MAKE_FLAGS=""
   ENC_MAKE_FLAGS=""
fi

if [ $RELEASE_BUILD = "1" ];then
   SANITIZER_MODE="0"
fi

build_vsi_decoder(){
   #compile vsi decoder
   VSI=$CUR_WORKDIR/src/dec/lib
   PLATFORM="x86_linux"
   CONFIG=debug

   if [ $BUILD_ARM64 = "1" ];then
      PLATFORM="arm_pclinux"
   fi

   cd $VSI
   rm -rf bin/
   if [ $RELEASE_BUILD = "1" ];then
      CONFIG=release
      echo "\e[36m-- Build VSI decoder library in Release mode \e[0m"
      make clean && make g2dec jpegdec ENV=${PLATFORM} TRACE=n USE_MODEL_SIMULATION=n  RELEASE=y USE_VCMD=y STATIC=y $DEC_MAKE_FLAGS
   else
      echo "\e[36m-- Build VSI decoder library in Debug mode \e[0m"
      make clean && make g2dec jpegdec ENV=${PLATFORM} TRACE=n USE_MODEL_SIMULATION=n  RELEASE=n USE_VCMD=y STATIC=y $DEC_MAKE_FLAGS
   fi

   if [ $? -ne "0" ]; then
      echo "\e[31m-- Build VSI decoder library failed!!! \e[0m"
      exit 1
   fi

   cd $VSI
   mkdir bin
   cp ./out/${PLATFORM}/${CONFIG}/*.a ./bin

   cp -r ./software/source/inc ./bin
   echo "\e[1;36m-- Build VSI decoder library finished!!! \e[0m"
}

build_vsi_encoder(){
   #compile vsi encoder
   cd $CUR_WORKDIR/src/enc/lib/software/
   PLATFORM="pci"

   if [ $BUILD_ARM64 = "1" ];then
      PLATFORM="arm64_linux"
   fi

   rm -rf ../bin

   if [ $RELEASE_BUILD = "1" ];then
      echo "\e[36m-- Build VSI encoder library in Release mode \e[0m"
      make clean && make h264 hevc jpeg ENV=${PLATFORM} TRACE=n DEBUG=n $ENC_MAKE_FLAGS
   else
      echo "\e[36m-- Build VSI encoder library in Debug mode \e[0m"
      make clean && make h264 hevc jpeg ENV=${PLATFORM} TRACE=n DEBUG=y $ENC_MAKE_FLAGS
   fi

   if [ $? -ne "0" ]; then
      echo "\e[31m-- Build VSI encoder library failed!!! \e[0m"
      exit 1
   fi
   mkdir  ../bin
   cp linux_reference/*.a ../bin
   # 交叉/静态配置下不一定产出 .so，拷贝失败不算错误
   cp linux_reference/*.so ../bin 2>/dev/null || true
   cp -r inc ../bin
   make clean
   echo "\e[1;36m-- Build VSI encoder library finished!!! \e[0m"
}

if [ $BUILD_TARGET = "0" ]; then
   build_vsi_decoder
   build_vsi_encoder
elif [ $BUILD_TARGET = "1" ]; then
   build_vsi_decoder
elif [ $BUILD_TARGET = "2" ]; then
   build_vsi_encoder
fi

#compile libvmpp-dec & libvmpp-enc
cd $CUR_WORKDIR/$BUILD_DIR
if [ $RELEASE_BUILD = "1" ];then
   echo "\e[36m-- Release Mode \e[0m"
   # 如需使用 ffmpeg 可追加 -DUSING_FFMPEG=$USING_FFMPEG
   cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$CUR_WORKDIR -DBUILD_TARGET=$BUILD_TARGET -DENABLE_DYNAMIC_RES=$ENABLE_DYNAMIC_RES \
       -DENABLE_UNIT_TEST=$ENABLE_UNIT_TEST -DVIDEO_COMPAT=1 -DOPTIMIZED_DEBUG=$OPTIMIZED_DEBUG -DBUILD_STATIC_LIBS=$STATIC_LINK
else
   echo "\e[36m-- Debug Mode \e[0m"
   # 如需使用 ffmpeg 可追加 -DUSING_FFMPEG=$USING_FFMPEG
   cmake .. -DCMAKE_INSTALL_PREFIX=$CUR_WORKDIR -DBUILD_TARGET=$BUILD_TARGET -DSANITIZER_MODE=$SANITIZER_MODE -DENABLE_DYNAMIC_RES=$ENABLE_DYNAMIC_RES \
       -DENABLE_UNIT_TEST=$ENABLE_UNIT_TEST -DVIDEO_COMPAT=1 -DBUILD_STATIC_LIBS=$STATIC_LINK
fi
echo "\e[36m-- Start making libvmpp-dec & libvmpp-enc library \e[0m"
make clean
make -j && make install
if [ $? -ne "0" ]; then
   echo "\e[31m-- Build libvmpp library failed!!! \e[0m"
   exit 1
else
   echo "\e[1;36m-- Build libvmpp library finished!!! \e[0m"
   # 静态库是自定义命令产物，退出码可能不可靠，这里确认产物确实存在
   if [ $STATIC_LINK = "1" ]; then
      if [ $BUILD_TARGET = "0" ] || [ $BUILD_TARGET = "1" ]; then
         if [ ! -f $CUR_WORKDIR/lib/libvmpp-dec.a ]; then
            echo "\e[31m-- libvmpp-dec.a was not generated!!! \e[0m"
            exit 1
         fi
      fi
      if [ $BUILD_TARGET = "0" ] || [ $BUILD_TARGET = "2" ]; then
         if [ ! -f $CUR_WORKDIR/lib/libvmpp-enc.a ]; then
            echo "\e[31m-- libvmpp-enc.a was not generated!!! \e[0m"
            exit 1
         fi
      fi
   fi
fi

do_release(){
   cd $CUR_WORKDIR
   echo "CUR_WORKDIR : $CUR_WORKDIR"
   RELEASE_DIR=$1 #"release"
   RELEASE_DIR_INC="inc"
   RELEASE_DIR_LIB="lib"
   if [ ! -d "$RELEASE_DIR" ]; then
      mkdir $RELEASE_DIR
      echo "\e[36m-- Make ${RELEASE_DIR}/ directory \e[0m"
   else
      rm -rf $RELEASE_DIR/*
      echo "\e[36m-- Empty ${RELEASE_DIR}/ directory \e[0m"
   fi
   mkdir $RELEASE_DIR/$RELEASE_DIR_INC
   mkdir $RELEASE_DIR/$RELEASE_DIR_LIB

   cp ./README.md ./$RELEASE_DIR
   cp -r ./sample ./$RELEASE_DIR
   cp -r ./test ./$RELEASE_DIR
   cp -r ./doc ./$RELEASE_DIR

   if [ $BUILD_TARGET = "0" ]; then
      cp ./inc/* ./$RELEASE_DIR/$RELEASE_DIR_INC/
      cp ./lib/* ./$RELEASE_DIR/$RELEASE_DIR_LIB/
      rm -r ./$RELEASE_DIR/sample/transcode
   elif [ $BUILD_TARGET = "1" ]; then
      cp ./inc/vmpp_common.h ./$RELEASE_DIR/$RELEASE_DIR_INC/
      cp ./inc/vmpp_dec_defs.h ./$RELEASE_DIR/$RELEASE_DIR_INC/
      cp ./inc/vmpp_dec_api.h ./$RELEASE_DIR/$RELEASE_DIR_INC/
      cp ./lib/libvmpp-dec.so ./$RELEASE_DIR/$RELEASE_DIR_LIB/
      [ -f ./lib/libvmpp-dec.a ] && cp ./lib/libvmpp-dec.a ./$RELEASE_DIR/$RELEASE_DIR_LIB/
      rm -r ./$RELEASE_DIR/sample/enc
      rm -r ./$RELEASE_DIR/sample/transcode
      rm -r ./$RELEASE_DIR/sample/transcode_mt
   elif [ $BUILD_TARGET = "2" ]; then
      cp ./inc/vmpp_common.h ./$RELEASE_DIR/$RELEASE_DIR_INC/
      cp ./inc/vmpp_enc_defs.h ./$RELEASE_DIR/$RELEASE_DIR_INC/
      cp ./inc/vmpp_enc_api.h ./$RELEASE_DIR/$RELEASE_DIR_INC/
      cp ./lib/libvmpp-enc.so ./$RELEASE_DIR/$RELEASE_DIR_LIB/
      [ -f ./lib/libvmpp-enc.a ] && cp ./lib/libvmpp-enc.a ./$RELEASE_DIR/$RELEASE_DIR_LIB/
      rm -r ./$RELEASE_DIR/sample/dec
      rm -r ./$RELEASE_DIR/sample/transcode
      rm -r ./$RELEASE_DIR/sample/transcode_mt
   fi
}


#compile libvmpp-dec & libvmpp-enc samples
echo "\e[36m-- build samples \e[0m"
cd $CUR_WORKDIR/sample
./autobuild-samples.sh -r 0 -t $BUILD_TARGET -s $SANITIZER_MODE -d $ENABLE_DYNAMIC_RES -a $BUILD_ARM64 -f $USING_FFMPEG -F $REBUILD_FFMPEG_FROM_SRC -S $STATIC_LINK


if [ $RELEASE_BUILD = "1" ];then
   echo "\e[1;36m-- Release librarys and samples to release directory!!! \e[0m"
   do_release "release"
else
   echo "\e[1;36m-- Release librarys and samples to debug directory!!! \e[0m"
   do_release "debug"
fi

# CICD: upload reports of static check & unitest code coverage to Web
# echo "\e[36m-- upload static check & code coverage report \e[0m"
# ctest -S ut/script/static_check.cmake -DSOURCE_TREE=${PWD} -DTYPE=CPPCHECK
# ctest -S ut/run_test.cmake
