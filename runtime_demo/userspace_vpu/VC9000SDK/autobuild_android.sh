#!/usr/bin/env sh

# set build environment
#NDK=/home/vastai/ndk/android-ndk-r21e
NDK=/opt/android-ndk-r21e

TOOLCHAIN=$NDK/toolchains/llvm/prebuilt/linux-x86_64

API=28

if [ $# -eq 0 ];then
   echo "\e[1;36m-- Build both decoder & encoder in DEBUG 64 compiler mode!!! \e[0m"
   BUILD_TARGET="64"
   TARGET=aarch64-linux-android
   RELEASE_BUILD="Debug"
   RELEASE_DIR="debug"
else
   echo "Parameters count: $#"
   while getopts "b:r:" opt; do
      case $opt in
         b)
            BUILD_TARGET=$OPTARG
            echo "\e[1;36m-- BUILD_TARGET:$BUILD_TARGET \e[0m"

            if [ $BUILD_TARGET = "64" ]; then
                TARGET=aarch64-linux-android
                echo "specified 64 compiler."
            elif [ $BUILD_TARGET = "32" ]; then
                TARGET=armv7a-linux-androideabi
                echo "specified 32 compiler."
            else
                echo "Error: please specify 32 or 64 compiler."
                exit 1;
            fi
            ;;
         r)
            if [ $OPTARG = "0" ]; then
                RELEASE_BUILD="Debug"
                RELEASE_DIR="debug"
                echo "specified debug compiler."
            elif [ $OPTARG = "1" ]; then
                RELEASE_BUILD="Release"
                RELEASE_DIR="release"
                echo "specified release compiler."
            else
                echo "Error: please specify debug or release compiler."
                exit 1;
            fi
            echo "\e[1;36m-- RELEASE_BUILD:$RELEASE_BUILD \e[0m"
            ;;
         ?)
            echo "\e[31m-- Usage: ./autobuild_android.sh -b 32 -r 1\e[0m"
            echo "\e[31m--   b(target_bit)         ==32 : 32bit                            [default]\e[0m"
            echo "\e[31m--                         ==64 : 64bit                                     \e[0m"
            echo "\e[31m--   r(release)            ==0 : debug mode                        [default]\e[0m"
            echo "\e[31m--                         ==1 : release mode                               \e[0m"
            exit
            ;;
      esac
   done
fi
echo "\e[1;36m-- BUILD_TARGET:$BUILD_TARGET, RELEASE_BUILD:$RELEASE_BUILD, RELEASE_DIR:$RELEASE_DIR \e[0m"


# export PATH=$TOOLCHAIN/bin:$PATH

export CC=$TOOLCHAIN/bin/$TARGET$API-clang

export AR=$TOOLCHAIN/bin/llvm-ar

CUR_WORKDIR=$(cd $(dirname $0); pwd)

VSI=$CUR_WORKDIR/src/dec/lib

# create build and release dir
BUILD_DIR="build"
RELEASE_LIB="lib"

if [ ! -d "$BUILD_DIR" ]; then
   mkdir $BUILD_DIR
   echo "\e[36m-- Make build directory \e[0m"
else
   rm -rf $BUILD_DIR/*
   echo "\e[36m-- Empty build directory \e[0m"
fi

if [ ! -d "$RELEASE_LIB" ]; then
   mkdir $RELEASE_LIB
   echo "\e[36m-- Make lib directory \e[0m"
else
   rm -rf $RELEASE_LIB/*
   echo "\e[36m-- Empty lib directory \e[0m"
fi

# compile L2cache
build_l2cache() {
    echo "\e[36m-- Compile L2cache.\e[0m"
    L2CACHE_DIR=$VSI/L2CACHE/cache/software/linux_reference
    cd $L2CACHE_DIR
    make clean pci CC=$CC AR=$AR ENABLE_ANDROID_ARM64=y
}

# compile vsi decoder
build_vsi_decoder() {
    echo "\e[36m-- Compile vsi decoder, VSI :$VSI \e[0m"
    cd $VSI
    PLATFORM=arm_linux
    CONFIG=release
    cd $VSI
    rm -rf bin/

        make clean g2dec_lib ENV=$PLATFORM CC=$TOOLCHAIN/bin/$TARGET$API-clang AR=$TOOLCHAIN/bin/llvm-ar RELEASE=y ENABLE_ANDROID_ARM64=y VIDEO_COMPAT=y

    if [ $? -ne "0" ]; then
      echo "\e[31m-- Build VSI decoder library failed!!! \e[0m"
      exit
    fi

    cd $VSI
    mkdir bin
    cp ./out/${PLATFORM}/release/*.a ./bin

    cp -r ./software/source/inc ./bin
    echo "\e[36m-- Compiled vsi decoder.\e[0m"
}

build_vsi_encoder(){
    #compile vsi encoder
    cd $CUR_WORKDIR/src/enc/lib/software/linux_reference

    echo "\e[36m-- Build VSI encoder library in Debug mode \e[0m"

    if [ $TARGET = "aarch64-linux-android" ]; then
        make clean pci ENV=$PLATFORM CC=$TOOLCHAIN/bin/$TARGET$API-clang AR=$TOOLCHAIN/bin/llvm-ar DEBUG=y ARM64=y ENABLE_ANDROID_ARM64=y VIDEO_COMPAT=y
    elif [ $TARGET = "armv7a-linux-androideabi" ]; then
        make clean pci ENV=$PLATFORM CC=$TOOLCHAIN/bin/$TARGET$API-clang AR=$TOOLCHAIN/bin/llvm-ar DEBUG=y ARM64=y ENABLE_ANDROID_ARM64=y ANDROID_32BIT=y VIDEO_COMPAT=y
    fi 

    if [ $? -ne "0" ]; then
        echo "\e[31m-- Build VSI encoder library failed!!! \e[0m"
        exit
    fi

    mkdir  ../../bin
    cp ./*.a  ../../bin
    cp -r ../inc ../../bin
    echo "\e[1;36m-- Build VSI encoder library finished!!! \e[0m"
}

# compile libvmpp_dec.so & libvmpp-enc
build_vmpp_lib() {
    echo "\e[36m-- Compile libvmpp-dec & libvmpp-enc . \e[0m"
    cd $CUR_WORKDIR/$BUILD_DIR
    if [ $TARGET = "aarch64-linux-android" ]; then
        cmake .. -DCMAKE_BUILD_TYPE=$RELEASE_BUILD -DCMAKE_INSTALL_PREFIX=$CUR_WORKDIR -DBUILD_TARGET=0 -DANDROID_32BIT=0 -DVIDEO_COMPAT=1
    elif [ $TARGET = "armv7a-linux-androideabi" ]; then
        cmake .. -DCMAKE_BUILD_TYPE=$RELEASE_BUILD -DCMAKE_INSTALL_PREFIX=$CUR_WORKDIR -DBUILD_TARGET=0 -DANDROID_32BIT=1 -DVIDEO_COMPAT=1
    fi

    make clean
    make && make install

    if [ $? -ne "0" ]; then
    echo "\e[31m-- Build libvmpp_dec library failed!!! \e[0m"
    exit
    else
    echo "\e[1;36m-- Build libvmpp_dec library finished!!! \e[0m"
    fi
}

do_release() {
    cd $CUR_WORKDIR
    echo "CUR_WORKDIR : $CUR_WORKDIR"

    RELEASE_DIR_INC="inc"
    RELEASE_DIR_LIB="lib"
    if [ ! -d "$RELEASE_DIR" ]; then
        mkdir $RELEASE_DIR
        echo "\e[36m-- Make $RELEASE_DIR/ directory \e[0m"
    else
        rm -rf $RELEASE_DIR/*
        echo "\e[36m-- Empty $RELEASE_DIR/ directory \e[0m"
    fi

    mkdir $RELEASE_DIR/$RELEASE_DIR_INC
    mkdir $RELEASE_DIR/$RELEASE_DIR_LIB

    cp ./README.md ./$RELEASE_DIR
    cp -r ./sample ./$RELEASE_DIR
    cp -r ./test ./$RELEASE_DIR
    cp -r ./doc ./$RELEASE_DIR

    cp ./test/android/*.so ./$RELEASE_DIR/$RELEASE_DIR_LIB/
    cp ./test/android/install_android.sh ./$RELEASE_DIR

    cp ./inc/* ./$RELEASE_DIR/$RELEASE_DIR_INC/
    cp ./lib/* ./$RELEASE_DIR/$RELEASE_DIR_LIB/
    #cp ./inc/vmpp_common.h ./$RELEASE_DIR/$RELEASE_DIR_INC/
    #cp ./inc/vmpp_dec_defs.h ./$RELEASE_DIR/$RELEASE_DIR_INC/
    #cp ./inc/vmpp_dec_api.h ./$RELEASE_DIR/$RELEASE_DIR_INC/
    #cp ./lib/libvmpp-dec.so ./$RELEASE_DIR/$RELEASE_DIR_LIB/
    #cp ./lib/libvmpp-enc.so ./$RELEASE_DIR/$RELEASE_DIR_LIB/

    #rm -r ./$RELEASE_DIR/sample/enc
    #rm -r ./$RELEASE_DIR/sample/transcode
    #rm -r ./$RELEASE_DIR/sample/transcode_mt
}

build_samples() {
    echo "\e[36m-- build samples \e[0m"
    cd $CUR_WORKDIR/sample
    if [ $TARGET = "aarch64-linux-android" ]; then
        ./autobuild-samples.sh -r 0 -t 0 -u 1 -m 1 -a 1 -b 0 -f 0
    elif [ $TARGET = "armv7a-linux-androideabi" ]; then
        ./autobuild-samples.sh -r 0 -t 0 -u 1 -m 1 -a 1 -b 1 -f 0
    else
        echo "build sample error: please specify 32 or 64 compiler."
        exit 1;
    fi

}

build_l2cache 

build_vsi_decoder
build_vsi_encoder

build_vmpp_lib

build_samples

do_release

