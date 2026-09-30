# vastai_video_sdk

This document describes the low-level video codec SDK of VASTAI

Access GitLab
---
[http://gitlabdev.vastai.com/video/vastai_video_sdk](http://gitlabdev.vastai.com/video/vastai_video_sdk)


File Structure
---
```
vastai_video_sdk
|-doc
|-inc
|-sample
|-src
|-test
|-ut
|-CMakeLists.txt
|-README.md
|-autobuild.sh
|-build_andriod_decoder.sh
|-version.cmake
```

Dependency
---

| - | branch | commit |
| ------ | ------ | ------ |
| [PCIe](http://gitlabdev.vastai.com/linux/pcie) |  d2_0_v1_5_a1_2 | 5200b4e5 |
| VDMCU |  decoder_refactor | 8bb5d5e0 |
| VEMCU |  encoder_dev | 7e96c6ef |
| [Runtime](http://gitlabdev.vastai.com/ai-compiler-group/runtime) | rel_alpha_1_0 | f235efc2 |

Usage
---

- Compile

```
./autobuild.sh  [options]
```
**NOTES:** *Please use **./autobuild.sh -?*** to view the detailed commands

This script will build libraries and samples at the same time. Libraries will be generated in ***build/out/*** while the samples will be generated in ***sample/build/out/***

- Run sample (take 'transcode_mt' as an example) 
  
*- only run decoding*
```
./sample/build/out/transcode_mt -r /dev/vastai_video0 -i xxx.hevc -s 1 -o xxx.yuv 
```
*- run transcoding*
```
./sample/build/out/transcode_mt -r /dev/vastai_video0 -i xxx.hevc -C h264 -s 1 -o xxx.h264 
```
- Build samples only (when libraries already exist)
```
cd sample/
./autobuild-samples.sh
```
**NOTES:** ***Please refer to the sample code for the detailed commands of each sample.***
- Build and run UT (when libraries already exist)
```
mount 192.168.30.93:/volume7/pe-data/cases/video/ /video-case/ # the required stream is in /video-case/
./autobuild.sh -d1 -U1
./build/unitest/unitest
```