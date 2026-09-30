


make -C linux_a76 KERNEL_SRC="${PWD}/linux-6.8" kernel-config

make -C linux_a76 KERNEL_SRC="${PWD}/linux-6.8" KERNEL_BUILD="${PWD}/linux_a76/build/kernel" all

rm linux_a76/build/initramfs.cpio linux_a76/build/linux_payload.bin linux_a76/build/cortex-r52-a76-vp.dtb  /dev/shm/* -rf

./linux_a76/scripts/run_linux_vpudemo.sh

insmod lib/modules/memalloc.ko alloc_size=128 alloc_base=0x90100000;insmod lib/modules/vcodec.ko

g2dec --dec-dev=/dev/hantrodec --mem-dev=/dev/memalloc --logoutlevel=3 --no-write --logtracemap=CFG --input-format=h264 /home/akiyo_352x288_300_IBBBP.h264 &

g2dec --dec-dev=/dev/hantrodec --mem-dev=/dev/memalloc --logoutlevel=3 --no-write --logtracemap=CFG --input-format=h265 -Ob1.yuv /home/sample_640x360.hevc &

 h264_testenc --encDevice=/dev/hantroenc --memDevice=/dev/memalloc -a0 -b9 --refRingBufEnable=0 -i/home/akiyo_352x288_10.yuv --inputFormat=0 --gopSize=1 --compressor=3 --rdoLevel=1 --enableRdoQuant=0 --enableTS=0 --lumWidthSrc=352 --lumHeightSrc=288 --width=352 --height=288 --codecFormat=h264 --inputAlignmentExp=0 --aqInfoAlignmentExp=6 --bitDepthLuma=8 --bitDepthChroma=8 --refAlignmentExp=0 --refChromaAlignmentExp=6 -o h264_case_id_246_cmodel_cmds.h264 &
;
hevc_testenc --encDevice=/dev/hantroenc --memDevice=/dev/memalloc -a0 -b9 --rdoLevel=1 --refRingBufEnable=0 -i/home/akiyo_352x288_10.yuv --inputFormat=0 --gopSize=1 --enableRdoQuant=0 --lumWidthSrc=352 --lumHeightSrc=288 --width=352 --height=288 --codecFormat=hevc --inputAlignmentExp=0 --aqInfoAlignmentExp=6 --bitDepthLuma=8 --bitDepthChroma=8 --refAlignmentExp=0 --refChromaAlignmentExp=6 -o hevc_case_id_118_cmodel_cmds.hevc &


