#!/bin/sh

CUR_DIR=`pwd`

DST_DIR=/vendor

DST_LIB_DIR=$DST_DIR/lib64/

LOG_DIR=/var/log/vastai/


#mkdir -p $DST_LIB_DIR/dri
mkdir -p $LOG_DIR/zlog

#install libva and libdrm lib
cp $CUR_DIR/lib/*.so           $DST_LIB_DIR/
#cp $CUR_DIR/lib/dri/*.so       $DST_LIB_DIR/

#install ffmpeg
#cp $CUR_DIR/ffmpeg/lib/*.so    $DST_LIB_DIR/
#cp $CUR_DIR/ffmpeg/bin/*       $DST_BIN_DIR/


#install dbgtool
#cp $CUR_DIR/tool/*             $DST_BIN_DIR/


#install zlog.conf
#cp $CUR_DIR/conf/*             $LOG_DIR/zlog

#create link of libzlog.so
ln -s -f $DST_LIB_DIR/libzlog.so $DST_LIB_DIR/libzlog.so.1
ln -s -f $DST_LIB_DIR/libzlog.so $DST_LIB_DIR/libzlog.so.1.2

# 
#ln -s -f $DST_LIB_DIR/libva.so         $DST_LIB_DIR/libva.so.2
#ln -s -f $DST_LIB_DIR/libva-drm.so     $DST_LIB_DIR/libva-drm.so.2
#ln -s -f $DST_LIB_DIR/libdrm.so        $DST_LIB_DIR/libdrm.so.2
#ln -s -f $DST_LIB_DIR/libdrm_hantro.so $DST_LIB_DIR/libdrm_hantro.so.1

