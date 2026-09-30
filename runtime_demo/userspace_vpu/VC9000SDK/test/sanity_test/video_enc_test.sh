#!/bin/bash

i=0
pass_num=0
fail_num=0

filenames[i]=/home/vastai/resource/dataset/yuv/1080P/xg1920x1088_x265_100.yuv
params[i]="-w 1920 -h 1088 -f nv12"
md5sums_h264[i]=59c5afcc8e093caa56488b0d5cc3b9b1
md5sums_h265[i]=dba0678f3f32ffd697046b7a48d87871
case_names[i]=BASIC_1080P_NV12_01
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/yuv/1080P/xg1920x1088_x265_100.yuv
params[i]="-w 1918 -h 1088 -t 1920 -f nv12"
md5sums_h264[i]=3b41dbc9e9ef2586d4dd78a44ffc8ac0
md5sums_h265[i]=b7c55b408e5dbf175b0dbd9caa0228c5
case_names[i]=BASIC_1080P_NV12_02
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/yuv/1080P/xg1920x1088_x265_100_yuv420p.yuv
params[i]="-w 1918 -h 1088 -t 1920 -f yuv420p"
md5sums_h264[i]=3b41dbc9e9ef2586d4dd78a44ffc8ac0
md5sums_h265[i]=b7c55b408e5dbf175b0dbd9caa0228c5
case_names[i]=BASIC_1080P_YUV420P_01
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/yuv/1080P/xg1920x1088_x265_100_yuv420p.yuv
params[i]="-w 1920 -h 1088 -t 1920 -f yuv420p"
md5sums_h264[i]=59c5afcc8e093caa56488b0d5cc3b9b1
md5sums_h265[i]=dba0678f3f32ffd697046b7a48d87871
case_names[i]=BASIC_1080P_YUV420P_02
i=$(($i+1))

#BASIC ABR 1PASS Kimono1_1920x1080_24.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/Kimono1_1920x1080_24.yuv
params[i]="-w 1920 -h 1080  -f yuv420p  --bitRate 8000000 --vbvBufSize 12000 --vbvMaxRate 16000 --gopSize 0"
md5sums_h264[i]=28186947886db7766a1586a042f53ea6
md5sums_h265[i]=34053b60c983441c1c7677b17c22d82e
case_names[i]=JCTVC_1080P_YUV420P_ABR_1PASS_01
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/Kimono1_1920x1080_24.yuv
params[i]="-w 1920 -h 1080  -f yuv420p  --bitRate 6000000 --vbvBufSize 9000 --vbvMaxRate 12000 --gopSize 0"
md5sums_h264[i]=1cdf1de78496e4d9a7499f367cbbd94b
md5sums_h265[i]=dcea77a2e17c279c4cc9bfd3012646d5
case_names[i]=JCTVC_1080P_YUV420P_ABR_1PASS_02
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/Kimono1_1920x1080_24.yuv
params[i]="-w 1920 -h 1080  -f yuv420p  --bitRate 4000000 --vbvBufSize 6000 --vbvMaxRate 8000 --gopSize 0"
md5sums_h264[i]=f1420b0b29d876ac156a49c98a49303a
md5sums_h265[i]=6ba4498d5bb0431341b5116b3ae0f1a1
case_names[i]=JCTVC_1080P_YUV420P_ABR_1PASS_03
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/Kimono1_1920x1080_24.yuv
params[i]="-w 1920 -h 1080  -f yuv420p  --bitRate 2000000 --vbvBufSize 3000 --vbvMaxRate 4000 --gopSize 0"
md5sums_h264[i]=8576b354842a188cf1a11a5e3e7e6d39
md5sums_h265[i]=986dd62fa54bfa0330cbf6bb02570c47
case_names[i]=JCTVC_1080P_YUV420P_ABR_1PASS_04
i=$(($i+1))

#BASIC ABR 1PASS BasketballDrill_832x480_50.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballDrill_832x480_50.yuv
params[i]="-w 832 -h 480  -f yuv420p  --bitRate 1500000 --vbvBufSize 1800 --vbvMaxRate 2000 --gopSize 0"
md5sums_h264[i]=fa58fe34dcc068aeda7d8c390e4df210
md5sums_h265[i]=4912972f7145e347b21191badbcf2c78
case_names[i]=JCTVC_480P_YUV420P_ABR_1PASS_01
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballDrill_832x480_50.yuv
params[i]="-w 832 -h 480  -f yuv420p --bitRate 1250000 --vbvBufSize 1500 --vbvMaxRate 1600 --gopSize 0"
md5sums_h264[i]=3103a8833cab23fce095217e91250627
md5sums_h265[i]=675deb1974f849e9e0d7512d69ef8ee1
case_names[i]=JCTVC_480P_YUV420P_ABR_1PASS_02
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballDrill_832x480_50.yuv
params[i]="-w 832 -h 480  -f yuv420p --bitRate 1000000 --vbvBufSize 1200 --vbvMaxRate 1400 --gopSize 0"
md5sums_h264[i]=109a622677f6dc3489dee842dfb85bd9
md5sums_h265[i]=bc454de6f374f771b15f2d1c2c04819e
case_names[i]=JCTVC_480P_YUV420P_ABR_1PASS_03
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballDrill_832x480_50.yuv
params[i]="-w 832 -h 480  -f yuv420p --bitRate 750000 --vbvBufSize 800 --vbvMaxRate 1200 --gopSize 0"
md5sums_h264[i]=70c0ea2bd8c56a37e2e7de30200e1813
md5sums_h265[i]=9532e0c4a35ad6d1cba073c5cd7cc381
case_names[i]=JCTVC_480P_YUV420P_ABR_1PASS_04
i=$(($i+1))

#BASIC ABR 1PASS BasketballPass_416x240_50.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballPass_416x240_50.yuv
params[i]="-w 416 -h 240  -f yuv420p --bitRate 600000 --vbvBufSize 720 --vbvMaxRate 800 --gopSize 0"
md5sums_h264[i]=5d98344581e16788e4eabb01fba664b1
md5sums_h265[i]=e13082d0860433d84886c091e519efcf
case_names[i]=JCTVC_240P_YUV420P_ABR_1PASS_01
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballPass_416x240_50.yuv
params[i]="-w 416 -h 240  -f yuv420p  --bitRate 400000 --vbvBufSize 480 --vbvMaxRate 560 --gopSize 0"
md5sums_h264[i]=a33c0d226e1b0a5a5b32f96b3b46dee2
md5sums_h265[i]=40716e20457bfdd053764344d0ca2e90
case_names[i]=JCTVC_240P_YUV420P_ABR_1PASS_02
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballPass_416x240_50.yuv
params[i]="-w 416 -h 240  -f yuv420p  --bitRate 200000 --vbvBufSize 240 --vbvMaxRate 280 --gopSize 0"
md5sums_h264[i]=75f1111a8f917daceaa99c74d5cebb9f
md5sums_h265[i]=69a8c73f7a6fb36d8077c93d9ae32080
case_names[i]=JCTVC_240P_YUV420P_ABR_1PASS_03
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballPass_416x240_50.yuv
params[i]="-w 416 -h 240  -f yuv420p  --bitRate 100000 --vbvBufSize 120 --vbvMaxRate 140 --gopSize 0"
md5sums_h264[i]=4754252845a18b4489f2730ec31b6af9
md5sums_h265[i]=2a8f90d1e91e33f23b94f124e6846f93
case_names[i]=JCTVC_240P_YUV420P_ABR_1PASS_04
i=$(($i+1))

#BASIC ABR 2PASS Kimono1_1920x1080_24.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/Kimono1_1920x1080_24.yuv
params[i]="-w 1920 -h 1080  -f yuv420p  --bitRate 8000000 --vbvBufSize 12000 --vbvMaxRate 16000 --lookaheadDepth 20"
md5sums_h264[i]=795b69167616f05712e745839f85a1a2
md5sums_h265[i]=4d610e3bb3aac3d9b5f355c46271ff64
case_names[i]=JCTVC_1080P_YUV420P_ABR_2PASS_01
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/Kimono1_1920x1080_24.yuv
params[i]="-w 1920 -h 1080  -f yuv420p  --bitRate 6000000 --vbvBufSize 9000 --vbvMaxRate 12000 --lookaheadDepth 20"
md5sums_h264[i]=7b419031cc0d51276e5ddb67e8dc51f8
md5sums_h265[i]=419d4cd2fdab275d55c3b44d7cd99183
case_names[i]=JCTVC_1080P_YUV420P_ABR_2PASS_02
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/Kimono1_1920x1080_24.yuv
params[i]="-w 1920 -h 1080  -f yuv420p  --bitRate 4000000 --vbvBufSize 6000 --vbvMaxRate 8000 --lookaheadDepth 20"
md5sums_h264[i]=1d832424dc659309bbc45f129f694dcf
md5sums_h265[i]=ff7e019fd5e560bb6cf5f81f07dae10f
case_names[i]=JCTVC_1080P_YUV420P_ABR_2PASS_03
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/Kimono1_1920x1080_24.yuv
params[i]="-w 1920 -h 1080  -f yuv420p  --bitRate 2000000 --vbvBufSize 3000 --vbvMaxRate 4000 --lookaheadDepth 20"
md5sums_h264[i]=ee4479a3919c416e927331b67c559750
md5sums_h265[i]=d6a1c5b100eafa2ecde7ea174a166871
case_names[i]=JCTVC_1080P_YUV420P_ABR_2PASS_04
i=$(($i+1))

#BASIC ABR 2PASS BasketballDrill_832x480_50.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballDrill_832x480_50.yuv
params[i]="-w 832 -h 480  -f yuv420p  --bitRate 1500000 --vbvBufSize 1800 --vbvMaxRate 2000 --lookaheadDepth 20"
md5sums_h264[i]=ea6a33672679d9d25fd76358e1ceb97d
md5sums_h265[i]=f5de9a796ab8805c530e56e39497599f
case_names[i]=JCTVC_480P_YUV420P_ABR_2PASS_01
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballDrill_832x480_50.yuv
params[i]="-w 832 -h 480  -f yuv420p  --bitRate 1250000 --vbvBufSize 1500 --vbvMaxRate 1600 --lookaheadDepth 20"
md5sums_h264[i]=0a29bddc05e8e7c9fcfc31f9e2f66b87
md5sums_h265[i]=fdc8c455333cdb356a3e053ab5df40f9
case_names[i]=JCTVC_480P_YUV420P_ABR_2PASS_02
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballDrill_832x480_50.yuv
params[i]="-w 832 -h 480  -f yuv420p  --bitRate 1000000 --vbvBufSize 1200 --vbvMaxRate 1400 --lookaheadDepth 20"
md5sums_h264[i]=844dc95e55f3f61b50da01a93df858b3
md5sums_h265[i]=c086cccf858f83cd5bb9a2b7dc43de7d
case_names[i]=JCTVC_480P_YUV420P_ABR_2PASS_03
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballDrill_832x480_50.yuv
params[i]="-w 832 -h 480  -f yuv420p  --bitRate 750000 --vbvBufSize 800 --vbvMaxRate 1200 --lookaheadDepth 20"
md5sums_h264[i]=2d0798e91e75bad93489d0b45299a113
md5sums_h265[i]=000be3a39c01dae5e209bed3eee1cedd
case_names[i]=JCTVC_480P_YUV420P_ABR_2PASS_04
i=$(($i+1))

#BASIC ABR 2PASS BasketballPass_416x240_50.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballPass_416x240_50.yuv
params[i]="-w 416 -h 240  -f yuv420p  --bitRate 600000 --vbvBufSize 720 --vbvMaxRate 800 --lookaheadDepth 20"
md5sums_h264[i]=c805e72d5898cf825fa9442f252aa30b
md5sums_h265[i]=03bb0b9dbdb056f9fcd5e37dbae2f434
case_names[i]=JCTVC_240P_YUV420P_ABR_2PASS_01
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballPass_416x240_50.yuv
params[i]="-w 416 -h 240  -f yuv420p  --bitRate 400000 --vbvBufSize 480 --vbvMaxRate 560 --lookaheadDepth 20"
md5sums_h264[i]=2fc4b52b4a9c60bb96593644e2bef777
md5sums_h265[i]=5db10f6381eb4e4532e77e65120afdc2
case_names[i]=JCTVC_240P_YUV420P_ABR_2PASS_02
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballPass_416x240_50.yuv
params[i]="-w 416 -h 240  -f yuv420p  --bitRate 200000 --vbvBufSize 240 --vbvMaxRate 280 --lookaheadDepth 20"
md5sums_h264[i]=f2e7dfca4c98778646921f8e9aa97c78
md5sums_h265[i]=bb82152aad9cb79c4a5fd26a8be5eb0c
case_names[i]=JCTVC_240P_YUV420P_ABR_2PASS_03
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballPass_416x240_50.yuv
params[i]="-w 416 -h 240  -f yuv420p  --bitRate 100000 --vbvBufSize 120 --vbvMaxRate 140 --lookaheadDepth 20"
md5sums_h264[i]=ba7f794141e68602c6d70c8107d3f138
md5sums_h265[i]=aec8312756f58848422adf0cce625705
case_names[i]=JCTVC_240P_YUV420P_ABR_2PASS_04
i=$(($i+1))

#BASIC CRF Kimono1_1920x1080_24.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/Kimono1_1920x1080_24.yuv
params[i]="-w 1920 -h 1080  -f yuv420p  --crf 22 --lookaheadDepth 20"
md5sums_h264[i]=2cb1eea420d6996c50aa996c4e9d4a79
md5sums_h265[i]=9be2b047819296548ccb4cf8743eff11
case_names[i]=JCTVC_1080P_YUV420P_CRF_01
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/Kimono1_1920x1080_24.yuv
params[i]="-w 1920 -h 1080  -f yuv420p  --crf 27 --lookaheadDepth 20"
md5sums_h264[i]=3b04758f9484b22ca55f023d78fe9640
md5sums_h265[i]=a30ab8fb4c9e1abef106e663e71762e8
case_names[i]=JCTVC_1080P_YUV420P_CRF_02
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/Kimono1_1920x1080_24.yuv
params[i]="-w 1920 -h 1080  -f yuv420p  --crf 32 --lookaheadDepth 20"
md5sums_h264[i]=9209e4688cc8580da5f01a638d92fb21
md5sums_h265[i]=960bb5d96a9063bdf14b91f6e6f4f460
case_names[i]=JCTVC_1080P_YUV420P_CRF_03
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/Kimono1_1920x1080_24.yuv
params[i]="-w 1920 -h 1080  -f yuv420p  --crf 37 --lookaheadDepth 20"
md5sums_h264[i]=7b75afa9b8a50b2160198714fbddf046
md5sums_h265[i]=83227b20c3fcfce77dcd50be14315001
case_names[i]=JCTVC_1080P_YUV420P_CRF_04
i=$(($i+1))

#BASIC CRF BasketballDrill_832x480_50.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballDrill_832x480_50.yuv
params[i]="-w 832 -h 480  -f yuv420p  --crf 22 --lookaheadDepth 20"
md5sums_h264[i]=f1fd132477254d65404c43107a4085a6
md5sums_h265[i]=a9006196b59cd5fefa13c0cfbcf1ddb1
case_names[i]=JCTVC_480P_YUV420P_CRF_01
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballDrill_832x480_50.yuv
params[i]="-w 832 -h 480  -f yuv420p  --crf 27 --lookaheadDepth 20"
md5sums_h264[i]=94ac26e341e3a28d57d15ffa20fe02f0
md5sums_h265[i]=44dac9c4f74834ddf2133a88a7420d57
case_names[i]=JCTVC_480P_YUV420P_CRF_02
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballDrill_832x480_50.yuv
params[i]="-w 832 -h 480  -f yuv420p  --crf 32 --lookaheadDepth 20"
md5sums_h264[i]=e30c164e28ffbe4957404fb3a5df6da8
md5sums_h265[i]=d11fdf78956ff79fee0a103c87b2ce55
case_names[i]=JCTVC_480P_YUV420P_CRF_03
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballDrill_832x480_50.yuv
params[i]="-w 832 -h 480  -f yuv420p  --crf 37 --lookaheadDepth 20"
md5sums_h264[i]=f9c9224556648e761b6e8f45b46a51ca
md5sums_h265[i]=0866edcb75b58254adb75be8610cf082
case_names[i]=JCTVC_480P_YUV420P_CRF_04
i=$(($i+1))

#BASIC CRF BasketballPass_416x240_50.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballPass_416x240_50.yuv
params[i]="-w 416 -h 240  -f yuv420p  --crf 22 --lookaheadDepth 20"
md5sums_h264[i]=3538dbcdbaa97553dd54f5b2a00c2dd1
md5sums_h265[i]=013703039a8412e567740310f95744bc
case_names[i]=JCTVC_240P_YUV420P_CRF_01
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballPass_416x240_50.yuv
params[i]="-w 416 -h 240  -f yuv420p  --crf 27 --lookaheadDepth 20"
md5sums_h264[i]=3a516555860c6bc4a0887281692ecf30
md5sums_h265[i]=ef6275c78f9cb061e78e8e89a8748e8c
case_names[i]=JCTVC_240P_YUV420P_CRF_02
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballPass_416x240_50.yuv
params[i]="-w 416 -h 240  -f yuv420p  --crf 32 --lookaheadDepth 20"
md5sums_h264[i]=a770acfdb7c1225500220f09029aecae
md5sums_h265[i]=8e058687c6c7d8689d130c21df495503
case_names[i]=JCTVC_240P_YUV420P_CRF_03
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballPass_416x240_50.yuv
params[i]="-w 416 -h 240  -f yuv420p  --crf 37 --lookaheadDepth 20"
md5sums_h264[i]=8c5dbda3593528ef8f659df0299e09bc
md5sums_h265[i]=39cc8154bbe572b4d415ae09fa2abe2f
case_names[i]=JCTVC_240P_YUV420P_CRF_04
i=$(($i+1))


#BASIC QualityMode gold test Kimono1_1920x1080_24.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/Kimono1_1920x1080_24.yuv
params[i]="-w 1920 -h 1080  -f yuv420p  --bitRate 8000000 --vbvBufSize 12000 --vbvMaxRate 16000 --qualityMode 0"
md5sums_h264[i]=d0dfed808ade2231fbc17431eff1c9a0
md5sums_h265[i]=54bb4aa27b26dd434318cc92686987cd
case_names[i]=JCTVC_1080P_YUV420P_GOLD_01
i=$(($i+1))

#BASIC QualityMode gold test BasketballDrill_832x480_50.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballDrill_832x480_50.yuv
params[i]="-w 832 -h 480  -f yuv420p  --bitRate 1500000 --vbvBufSize 1800 --vbvMaxRate 2000 --qualityMode 0"
md5sums_h264[i]=8ea2d5a4c78e43438f02a4ac2dbb9d75
md5sums_h265[i]=299d46445294a2303999608ca0f4faaa
case_names[i]=JCTVC_480P_YUV420P_GOLD_01
i=$(($i+1))

#BASIC  QualityMode gold test BasketballPass_416x240_50.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballPass_416x240_50.yuv
params[i]="-w 416 -h 240  -f yuv420p  --bitRate 600000 --vbvBufSize 720 --vbvMaxRate 800 --qualityMode 0"
md5sums_h264[i]=0e93dba33f7be26cfa28caec2c93336d
md5sums_h265[i]=3de2503e72df24566715e4c3f6b75d12
case_names[i]=JCTVC_240P_YUV420P_GOLD_01
i=$(($i+1))


#BASIC QualityMode silver test Kimono1_1920x1080_24.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/Kimono1_1920x1080_24.yuv
params[i]="-w 1920 -h 1080  -f yuv420p  --bitRate 8000000 --vbvBufSize 12000 --vbvMaxRate 16000 --qualityMode 1"
md5sums_h264[i]=4b90696403a3cf06a315c25395dbcbd2
md5sums_h265[i]=333ee2904fd384669de3c655881c93d6
case_names[i]=JCTVC_1080P_YUV420P_SILVER_01
i=$(($i+1))

#BASIC QualityMode silver test BasketballDrill_832x480_50.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballDrill_832x480_50.yuv
params[i]="-w 832 -h 480  -f yuv420p  --bitRate 1500000 --vbvBufSize 1800 --vbvMaxRate 2000 --qualityMode 1"
md5sums_h264[i]=83a6182fab136b47a7f7844c27ad26b6
md5sums_h265[i]=257fce2003c9e092657bba70cb6dc327
case_names[i]=JCTVC_480P_YUV420P_SILVER_01
i=$(($i+1))

#BASIC  QualityMode silver test BasketballPass_416x240_50.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballPass_416x240_50.yuv
params[i]="-w 416 -h 240  -f yuv420p  --bitRate 600000 --vbvBufSize 720 --vbvMaxRate 800 --qualityMode 1"
md5sums_h264[i]=6f5f413151390e5df9cd466df4ac3e98
md5sums_h265[i]=a4e6e03a850286e3318bf1664f01b6df
case_names[i]=JCTVC_240P_YUV420P_SILVER_01
i=$(($i+1))


#BASIC QualityMode silver2 test Kimono1_1920x1080_24.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/Kimono1_1920x1080_24.yuv
params[i]="-w 1920 -h 1080  -f yuv420p  --bitRate 8000000 --vbvBufSize 12000 --vbvMaxRate 16000 --qualityMode 2"
md5sums_h264[i]=4b90696403a3cf06a315c25395dbcbd2
md5sums_h265[i]=df05746a7453b5c035462e4119edc663
case_names[i]=JCTVC_1080P_YUV420P_SILVER2_01
i=$(($i+1))

#BASIC QualityMode silver2 test BasketballDrill_832x480_50.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballDrill_832x480_50.yuv
params[i]="-w 832 -h 480  -f yuv420p  --bitRate 1500000 --vbvBufSize 1800 --vbvMaxRate 2000 --qualityMode 2"
md5sums_h264[i]=83a6182fab136b47a7f7844c27ad26b6
md5sums_h265[i]=3a845626e0fb247ad1a50b04ed7a3e96
case_names[i]=JCTVC_480P_YUV420P_SILVER2_01
i=$(($i+1))

#BASIC  QualityMode silver2 test BasketballPass_416x240_50.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballPass_416x240_50.yuv
params[i]="-w 416 -h 240  -f yuv420p  --bitRate 600000 --vbvBufSize 720 --vbvMaxRate 800 --qualityMode 2"
md5sums_h264[i]=6f5f413151390e5df9cd466df4ac3e98
md5sums_h265[i]=d9267b7edce716b4dbe02098dd09d5ad
case_names[i]=JCTVC_240P_YUV420P_SILVER2_01
i=$(($i+1))


#BASIC QualityMode bronze test Kimono1_1920x1080_24.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/Kimono1_1920x1080_24.yuv
params[i]="-w 1920 -h 1080  -f yuv420p  --bitRate 8000000 --vbvBufSize 12000 --vbvMaxRate 16000 --qualityMode 3"
md5sums_h264[i]=4b90696403a3cf06a315c25395dbcbd2
md5sums_h265[i]=7061a9a372bc2cba0455f05c9a2e651f
case_names[i]=JCTVC_1080P_YUV420P_BRONZE_01
i=$(($i+1))

#BASIC QualityMode bronze test BasketballDrill_832x480_50.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballDrill_832x480_50.yuv
params[i]="-w 832 -h 480  -f yuv420p  --bitRate 1500000 --vbvBufSize 1800 --vbvMaxRate 2000 --qualityMode 3"
md5sums_h264[i]=83a6182fab136b47a7f7844c27ad26b6
md5sums_h265[i]=f849b83348ad6e2b4b388abdda5e0c0c
case_names[i]=JCTVC_480P_YUV420P_BRONZE_01
i=$(($i+1))

#BASIC  QualityMode bronze test BasketballPass_416x240_50.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballPass_416x240_50.yuv
params[i]="-w 416 -h 240  -f yuv420p  --bitRate 600000 --vbvBufSize 720 --vbvMaxRate 800 --qualityMode 3"
md5sums_h264[i]=6f5f413151390e5df9cd466df4ac3e98
md5sums_h265[i]=26f1c7806b448dce4ed958dd686ea484
case_names[i]=JCTVC_240P_YUV420P_BRONZE_01
i=$(($i+1))


#BASIC LookaheadDepth test Kimono1_1920x1080_24.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/Kimono1_1920x1080_24.yuv
params[i]="-w 1920 -h 1080  -f yuv420p  --bitRate 8000000 --vbvBufSize 12000 --vbvMaxRate 16000 --lookaheadDepth 5"
md5sums_h264[i]=44cf90555c14c09bc67c25ba03c1727c
md5sums_h265[i]=cacc71324065341f55f71b5a2c83cb51
case_names[i]=JCTVC_1080P_YUV420P_LOOKAHEAD_01
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/Kimono1_1920x1080_24.yuv
params[i]="-w 1920 -h 1080  -f yuv420p  --bitRate 8000000 --vbvBufSize 12000 --vbvMaxRate 16000 --lookaheadDepth 10"
md5sums_h264[i]=ce9a4514917a70ff63c7534eac7b9eff
md5sums_h265[i]=b4f9befb7dd49719642826bbbf413f7a
case_names[i]=JCTVC_1080P_YUV420P_LOOKAHEAD_02
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/Kimono1_1920x1080_24.yuv
params[i]="-w 1920 -h 1080  -f yuv420p  --bitRate 8000000 --vbvBufSize 12000 --vbvMaxRate 16000 --lookaheadDepth 15"
md5sums_h264[i]=669a3ef377da2639977a469990a95ffb
md5sums_h265[i]=0c577f0411d903225482a543cd50e907
case_names[i]=JCTVC_1080P_YUV420P_LOOKAHEAD_03
i=$(($i+1))

#BASIC LookaheadDepth test BasketballDrill_832x480_50.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballDrill_832x480_50.yuv
params[i]="-w 832 -h 480  -f yuv420p  --bitRate 1500000 --vbvBufSize 1800 --vbvMaxRate 2000 --lookaheadDepth 5"
md5sums_h264[i]=064ccb9723d75fccb2bc27425f63815d
md5sums_h265[i]=98ae8990291f347d7efebaef78766d90
case_names[i]=JCTVC_480P_YUV420P_LOOKAHEAD_01
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballDrill_832x480_50.yuv
params[i]="-w 832 -h 480  -f yuv420p  --bitRate 1500000 --vbvBufSize 1800 --vbvMaxRate 2000 --lookaheadDepth 10"
md5sums_h264[i]=65d9c01f508d7430a3f237fbb5b7c947
md5sums_h265[i]=06210bfcbea7114f481bbcb7fc88066f
case_names[i]=JCTVC_480P_YUV420P_LOOKAHEAD_02
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballDrill_832x480_50.yuv
params[i]="-w 832 -h 480  -f yuv420p  --bitRate 1500000 --vbvBufSize 1800 --vbvMaxRate 2000 --lookaheadDepth 15"
md5sums_h264[i]=65d9c01f508d7430a3f237fbb5b7c947
md5sums_h265[i]=06210bfcbea7114f481bbcb7fc88066f
case_names[i]=JCTVC_480P_YUV420P_LOOKAHEAD_03
i=$(($i+1))

#BASIC LookaheadDepth test test BasketballPass_416x240_50.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballPass_416x240_50.yuv
params[i]="-w 416 -h 240  -f yuv420p  --bitRate 600000 --vbvBufSize 720 --vbvMaxRate 800 --lookaheadDepth 5"
md5sums_h264[i]=6b1d0b38b79c5c360a8221e2e6a7fbb3
md5sums_h265[i]=12374c381fac3259bfd529cb77b03804
case_names[i]=JCTVC_240P_YUV420P_LOOKAHEAD_01
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballPass_416x240_50.yuv
params[i]="-w 416 -h 240  -f yuv420p  --bitRate 600000 --vbvBufSize 720 --vbvMaxRate 800 --lookaheadDepth 10"
md5sums_h264[i]=9a9585ebb030f65ac1352ba74e5f846f
md5sums_h265[i]=23176b124516807b6ad3fad8deb8f1ef
case_names[i]=JCTVC_240P_YUV420P_LOOKAHEAD_02
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballPass_416x240_50.yuv
params[i]="-w 416 -h 240  -f yuv420p  --bitRate 600000 --vbvBufSize 720 --vbvMaxRate 800 --lookaheadDepth 15"
md5sums_h264[i]=4ee1918d6d84284f177805afa6fca5b4
md5sums_h265[i]=23176b124516807b6ad3fad8deb8f1ef
case_names[i]=JCTVC_240P_YUV420P_LOOKAHEAD_03
i=$(($i+1))


#BASIC gopSize test Kimono1_1920x1080_24.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/Kimono1_1920x1080_24.yuv
params[i]="-w 1920 -h 1080  -f yuv420p  --bitRate 8000000 --vbvBufSize 12000 --vbvMaxRate 16000 --gopSize 0"
md5sums_h264[i]=28186947886db7766a1586a042f53ea6
md5sums_h265[i]=34053b60c983441c1c7677b17c22d82e
case_names[i]=JCTVC_1080P_YUV420P_GOPSIZE_01
i=$(($i+1))
#IPPP
filenames[i]=/home/vastai/resource/dataset/jctvc/Kimono1_1920x1080_24.yuv
params[i]="-w 1920 -h 1080  -f yuv420p  --bitRate 8000000 --vbvBufSize 12000 --vbvMaxRate 16000 --gopSize 1"
md5sums_h264[i]=4b90696403a3cf06a315c25395dbcbd2
md5sums_h265[i]=7061a9a372bc2cba0455f05c9a2e651f
case_names[i]=JCTVC_1080P_YUV420P_GOPSIZE_02
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/Kimono1_1920x1080_24.yuv
params[i]="-w 1920 -h 1080  -f yuv420p  --bitRate 8000000 --vbvBufSize 12000 --vbvMaxRate 16000 --gopSize 4"
md5sums_h264[i]=1f9091925b68d9e44458be7b4e338a8d
md5sums_h265[i]=422e101f421f82688c48d61c62dc173e
case_names[i]=JCTVC_1080P_YUV420P_GOPSIZE_03
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/Kimono1_1920x1080_24.yuv
params[i]="-w 1920 -h 1080  -f yuv420p  --bitRate 8000000 --vbvBufSize 12000 --vbvMaxRate 16000 --gopSize 8"
md5sums_h264[i]=a8f68fee8f5784b9af9eb52beddbf1b2
md5sums_h265[i]=064a7e06070a26648a60ecd4ca2a8b6f
case_names[i]=JCTVC_1080P_YUV420P_GOPSIZE_04
i=$(($i+1))

#BASIC gopSize test BasketballDrill_832x480_50.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballDrill_832x480_50.yuv
params[i]="-w 832 -h 480  -f yuv420p  --bitRate 1500000 --vbvBufSize 1800 --vbvMaxRate 2000 --gopSize 0"
md5sums_h264[i]=fa58fe34dcc068aeda7d8c390e4df210
md5sums_h265[i]=4912972f7145e347b21191badbcf2c78
case_names[i]=JCTVC_480P_YUV420P_GOPSIZE_01
i=$(($i+1))
#IPPP
filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballDrill_832x480_50.yuv
params[i]="-w 832 -h 480  -f yuv420p  --bitRate 1500000 --vbvBufSize 1800 --vbvMaxRate 2000 --gopSize 1"
md5sums_h264[i]=83a6182fab136b47a7f7844c27ad26b6
md5sums_h265[i]=f849b83348ad6e2b4b388abdda5e0c0c
case_names[i]=JCTVC_480P_YUV420P_GOPSIZE_02
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballDrill_832x480_50.yuv
params[i]="-w 832 -h 480  -f yuv420p  --bitRate 1500000 --vbvBufSize 1800 --vbvMaxRate 2000 --gopSize 4"
md5sums_h264[i]=6b65beed79ee8e449cc557151f1fe6ce
md5sums_h265[i]=7dbebc049455f1d8eec41fb0af863cc0
case_names[i]=JCTVC_480P_YUV420P_GOPSIZE_03
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballDrill_832x480_50.yuv
params[i]="-w 832 -h 480  -f yuv420p  --bitRate 1500000 --vbvBufSize 1800 --vbvMaxRate 2000 --gopSize 8"
md5sums_h264[i]=ea9233ecc334f96a4ce36238acc51fd7
md5sums_h265[i]=4abdbc357ff10d047080a3dafe88a6ff
case_names[i]=JCTVC_480P_YUV420P_GOPSIZE_04
i=$(($i+1))

#BASIC gopSize test test BasketballPass_416x240_50.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballPass_416x240_50.yuv
params[i]="-w 416 -h 240  -f yuv420p  --bitRate 600000 --vbvBufSize 720 --vbvMaxRate 800 --gopSize 0"
md5sums_h264[i]=5d98344581e16788e4eabb01fba664b1
md5sums_h265[i]=e13082d0860433d84886c091e519efcf
case_names[i]=JCTVC_240P_YUV420P_GOPSIZE_01
i=$(($i+1))
#IPPP
filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballPass_416x240_50.yuv
params[i]="-w 416 -h 240  -f yuv420p  --bitRate 600000 --vbvBufSize 720 --vbvMaxRate 800 --gopSize 1"
md5sums_h264[i]=6f5f413151390e5df9cd466df4ac3e98
md5sums_h265[i]=26f1c7806b448dce4ed958dd686ea484
case_names[i]=JCTVC_240P_YUV420P_GOPSIZE_02
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballPass_416x240_50.yuv
params[i]="-w 416 -h 240  -f yuv420p  --bitRate 600000 --vbvBufSize 720 --vbvMaxRate 800 --gopSize 4"
md5sums_h264[i]=8c1a500722b72455518a503f1fe6901e
md5sums_h265[i]=8e6dc70b52526c6829182e4eb27b35df
case_names[i]=JCTVC_240P_YUV420P_GOPSIZE_03
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballPass_416x240_50.yuv
params[i]="-w 416 -h 240  -f yuv420p  --bitRate 600000 --vbvBufSize 720 --vbvMaxRate 800 --gopSize 8"
md5sums_h264[i]=eac9c1ce5f39abbbe3aba8bee885dfe7
md5sums_h265[i]=dbde4e630dc4b0546a61bb8c2b8a41b9
case_names[i]=JCTVC_240P_YUV420P_GOPSIZE_04
i=$(($i+1))


#BASIC gdrDuration test Kimono1_1920x1080_24.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/Kimono1_1920x1080_24.yuv
params[i]="-w 1920 -h 1080  -f yuv420p  --bitRate 8000000 --vbvBufSize 12000 --vbvMaxRate 16000 --gdrDuration 0"
md5sums_h264[i]=4b90696403a3cf06a315c25395dbcbd2
md5sums_h265[i]=7061a9a372bc2cba0455f05c9a2e651f
case_names[i]=JCTVC_1080P_YUV420P_GDRDURATION_01
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/Kimono1_1920x1080_24.yuv
params[i]="-w 1920 -h 1080  -f yuv420p  --bitRate 8000000 --vbvBufSize 12000 --vbvMaxRate 16000 --gdrDuration 2 --keyInt 8 --gopSize 1"
md5sums_h264[i]=3853eada08cb0fa8271f1f799d289c0a
md5sums_h265[i]=1fac7f62b0288f808f722f298a76db94
case_names[i]=JCTVC_1080P_YUV420P_GDRDURATION_02
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/Kimono1_1920x1080_24.yuv
params[i]="-w 1920 -h 1080  -f yuv420p  --bitRate 8000000 --vbvBufSize 12000 --vbvMaxRate 16000 --gdrDuration 4 --keyInt 8 --gopSize 1"
md5sums_h264[i]=3fbe15e7814209d20816a14b37ecebe1
md5sums_h265[i]=c40d1a4922ab3b876b9333e97a5db398
case_names[i]=JCTVC_1080P_YUV420P_GDRDURATION_03
i=$(($i+1))

#BASIC gdrDuration test BasketballDrill_832x480_50.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballDrill_832x480_50.yuv
params[i]="-w 832 -h 480  -f yuv420p  --bitRate 1500000 --vbvBufSize 1800 --vbvMaxRate 2000 --gdrDuration 0"
md5sums_h264[i]=83a6182fab136b47a7f7844c27ad26b6
md5sums_h265[i]=f849b83348ad6e2b4b388abdda5e0c0c
case_names[i]=JCTVC_480P_YUV420P_GDRDURATION_01
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballDrill_832x480_50.yuv
params[i]="-w 832 -h 480  -f yuv420p  --bitRate 1500000 --vbvBufSize 1800 --vbvMaxRate 2000 --gdrDuration 2 --keyInt 8 --gopSize 1"
md5sums_h264[i]=2c76f0fd27a65b1c0135268f6435970e
md5sums_h265[i]=efc8fd149852475cf73d5921f0c46ca2
case_names[i]=JCTVC_480P_YUV420P_GDRDURATION_02
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballDrill_832x480_50.yuv
params[i]="-w 832 -h 480  -f yuv420p  --bitRate 1500000 --vbvBufSize 1800 --vbvMaxRate 2000 --gdrDuration 4 --keyInt 8 --gopSize 1"
md5sums_h264[i]=10f51f0ff02f61ef151fe0b47ee9139f
md5sums_h265[i]=fe5b2c064cdf6ab02061fc183a89fd87
case_names[i]=JCTVC_480P_YUV420P_GDRDURATION_03
i=$(($i+1))

#BASIC gdrDuration test test BasketballPass_416x240_50.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballPass_416x240_50.yuv
params[i]="-w 416 -h 240  -f yuv420p  --bitRate 600000 --vbvBufSize 720 --vbvMaxRate 800 --gdrDuration 0"
md5sums_h264[i]=6f5f413151390e5df9cd466df4ac3e98
md5sums_h265[i]=26f1c7806b448dce4ed958dd686ea484
case_names[i]=JCTVC_240P_YUV420P_GDRDURATION_01
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballPass_416x240_50.yuv
params[i]="-w 416 -h 240  -f yuv420p  --bitRate 600000 --vbvBufSize 720 --vbvMaxRate 800 --gdrDuration 2 --keyInt 8 --gopSize 1"
md5sums_h264[i]=0196b9f8956aa486642140294235c4eb
md5sums_h265[i]=5e0ebb1ea87814046643bb592929eaff
case_names[i]=JCTVC_240P_YUV420P_GDRDURATION_02
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballPass_416x240_50.yuv
params[i]="-w 416 -h 240  -f yuv420p  --bitRate 600000 --vbvBufSize 720 --vbvMaxRate 800 --gdrDuration 4 --keyInt 8 --gopSize 1"
md5sums_h264[i]=9edd3d2a6993dde5a0d23e64e9a26e41
md5sums_h265[i]=c111ee647b32527c5230928687f4dfea
case_names[i]=JCTVC_240P_YUV420P_GDRDURATION_03
i=$(($i+1))

#BASIC keyInt test Kimono1_1920x1080_24.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/Kimono1_1920x1080_24.yuv
params[i]="-w 1920 -h 1080  -f yuv420p  --bitRate 8000000 --vbvBufSize 12000 --vbvMaxRate 16000 --keyInt 40"
md5sums_h264[i]=6b3baca78caa994dd766db357b149196
md5sums_h265[i]=a576624931ec30b7fab6e92e9efe034f
case_names[i]=JCTVC_1080P_YUV420P_KEYINT_01
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/Kimono1_1920x1080_24.yuv
params[i]="-w 1920 -h 1080  -f yuv420p  --bitRate 8000000 --vbvBufSize 12000 --vbvMaxRate 16000 --keyInt 120"
md5sums_h264[i]=78ee1eb4508a30d716237d8e070afbc9
md5sums_h265[i]=031f8965a3f540b0d6c2520102582fba
case_names[i]=JCTVC_1080P_YUV420P_KEYINT_02
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/Kimono1_1920x1080_24.yuv
params[i]="-w 1920 -h 1080  -f yuv420p  --bitRate 8000000 --vbvBufSize 12000 --vbvMaxRate 16000 --keyInt 200"
md5sums_h264[i]=86032b7e368ae650525fde4c94bda2c3
md5sums_h265[i]=46d19542c7e3899a7ba22bbaea8795f9
case_names[i]=JCTVC_1080P_YUV420P_KEYINT_03
i=$(($i+1))

#BASIC keyInt BasketballDrill_832x480_50.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballDrill_832x480_50.yuv
params[i]="-w 832 -h 480  -f yuv420p  --bitRate 1500000 --vbvBufSize 1800 --vbvMaxRate 2000 --keyInt 40"
md5sums_h264[i]=b0752d3cab2313807b1539546b417f4e
md5sums_h265[i]=487e2d40d5c434144d58c044a1bfa3d8
case_names[i]=JCTVC_480P_YUV420P_KEYINT_01
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballDrill_832x480_50.yuv
params[i]="-w 832 -h 480  -f yuv420p  --bitRate 1500000 --vbvBufSize 1800 --vbvMaxRate 2000 --keyInt 120"
md5sums_h264[i]=2185aaed25bf0143b84b0c9a5a9d3b08
md5sums_h265[i]=08dd05da991d90782edda61272451726
case_names[i]=JCTVC_480P_YUV420P_KEYINT_02
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballDrill_832x480_50.yuv
params[i]="-w 832 -h 480  -f yuv420p  --bitRate 1500000 --vbvBufSize 1800 --vbvMaxRate 2000 --keyInt 200"
md5sums_h264[i]=3e95e46385aed63d656438f9ba792397
md5sums_h265[i]=ab341c388423e528c580ef8e9cd70fa5
case_names[i]=JCTVC_480P_YUV420P_KEYINT_03
i=$(($i+1))

#BASIC keyInt test BasketballPass_416x240_50.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballPass_416x240_50.yuv
params[i]="-w 416 -h 240  -f yuv420p  --bitRate 600000 --vbvBufSize 720 --vbvMaxRate 800 --keyInt 40"
md5sums_h264[i]=ffc3ec376174f0d620ccd3007b97d08b
md5sums_h265[i]=a50424981196111fed65ddcaa7e85582
case_names[i]=JCTVC_240P_YUV420P_KEYINT_01
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballPass_416x240_50.yuv
params[i]="-w 416 -h 240  -f yuv420p  --bitRate 600000 --vbvBufSize 720 --vbvMaxRate 800 --keyInt 120"
md5sums_h264[i]=fce1a7e177ce4e6d67dbd6c67ff620ab
md5sums_h265[i]=c5d8a3179100f1106db1b1274a855aa5
case_names[i]=JCTVC_240P_YUV420P_KEYINT_02
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballPass_416x240_50.yuv
params[i]="-w 416 -h 240  -f yuv420p  --bitRate 600000 --vbvBufSize 720 --vbvMaxRate 800 --keyInt 200"
md5sums_h264[i]=5e70b7a980a3be5963b2f6202b25d880
md5sums_h265[i]=af1c6f684f4e1c2243e50592ff94f521
case_names[i]=JCTVC_240P_YUV420P_KEYINT_03
i=$(($i+1))

#BASIC P2B test Kimono1_1920x1080_24.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/Kimono1_1920x1080_24.yuv
params[i]="-w 1920 -h 1080  -f yuv420p  --bitRate 8000000 --vbvBufSize 12000 --vbvMaxRate 16000 --P2B 0"
md5sums_h264[i]=4b90696403a3cf06a315c25395dbcbd2
md5sums_h265[i]=1c065848bc74cd93c089f65030a7dc83
case_names[i]=JCTVC_1080P_YUV420P_P2B_01
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/Kimono1_1920x1080_24.yuv
params[i]="-w 1920 -h 1080  -f yuv420p  --bitRate 8000000 --vbvBufSize 12000 --vbvMaxRate 16000 --P2B 1"
md5sums_h264[i]=d6e71d830e591a9ff499f4e638cc7857
md5sums_h265[i]=7061a9a372bc2cba0455f05c9a2e651f
case_names[i]=JCTVC_1080P_YUV420P_P2B_02
i=$(($i+1))

#BASIC P2B BasketballDrill_832x480_50.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballDrill_832x480_50.yuv
params[i]="-w 832 -h 480  -f yuv420p  --bitRate 1500000 --vbvBufSize 1800 --vbvMaxRate 2000 --P2B 0"
md5sums_h264[i]=83a6182fab136b47a7f7844c27ad26b6
md5sums_h265[i]=491916c85e53cb2c56dd63313443361a
case_names[i]=JCTVC_480P_YUV420P_P2B_01
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballDrill_832x480_50.yuv
params[i]="-w 832 -h 480  -f yuv420p  --bitRate 1500000 --vbvBufSize 1800 --vbvMaxRate 2000 --P2B 1"
md5sums_h264[i]=b502e4f2cbe51247e98edaf6567b131f
md5sums_h265[i]=f849b83348ad6e2b4b388abdda5e0c0c
case_names[i]=JCTVC_480P_YUV420P_P2B_02
i=$(($i+1))

#BASIC P2B test BasketballPass_416x240_50.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballPass_416x240_50.yuv
params[i]="-w 416 -h 240  -f yuv420p  --bitRate 600000 --vbvBufSize 720 --vbvMaxRate 800 --P2B 0"
md5sums_h264[i]=6f5f413151390e5df9cd466df4ac3e98
md5sums_h265[i]=d1648da39234a63d16f4eb44280b1347
case_names[i]=JCTVC_240P_YUV420P_P2B_01
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballPass_416x240_50.yuv
params[i]="-w 416 -h 240  -f yuv420p  --bitRate 600000 --vbvBufSize 720 --vbvMaxRate 800 --P2B 1"
md5sums_h264[i]=95413cbb6995d61a464be2f2ab51959f
md5sums_h265[i]=26f1c7806b448dce4ed958dd686ea484
case_names[i]=JCTVC_240P_YUV420P_P2B_02
i=$(($i+1))

#BASIC bBPyramid test Kimono1_1920x1080_24.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/Kimono1_1920x1080_24.yuv
params[i]="-w 1920 -h 1080  -f yuv420p  --bitRate 8000000 --vbvBufSize 12000 --vbvMaxRate 16000 --gopSize 0 --bBPyramid 0"
md5sums_h264[i]=9a29759e75ab457e72e9e14d22fda112
md5sums_h265[i]=246cf3ec979ed7646c148473f35a5fcf
case_names[i]=JCTVC_1080P_YUV420P_bBPyramid_01
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/Kimono1_1920x1080_24.yuv
params[i]="-w 1920 -h 1080  -f yuv420p  --bitRate 8000000 --vbvBufSize 12000 --vbvMaxRate 16000 --gopSize 0 --bBPyramid 1"
md5sums_h264[i]=28186947886db7766a1586a042f53ea6
md5sums_h265[i]=34053b60c983441c1c7677b17c22d82e
case_names[i]=JCTVC_1080P_YUV420P_bBPyramid_02
i=$(($i+1))

#BASIC bBPyramid BasketballDrill_832x480_50.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballDrill_832x480_50.yuv
params[i]="-w 832 -h 480  -f yuv420p  --bitRate 1500000 --vbvBufSize 1800 --vbvMaxRate 2000 --gopSize 0 --bBPyramid 0"
md5sums_h264[i]=1a832e7831765d63c6f44d6709bc0e05
md5sums_h265[i]=87423dd0364ab535ca8ed6eafa1793f0
case_names[i]=JCTVC_480P_YUV420P_bBPyramid_01
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballDrill_832x480_50.yuv
params[i]="-w 832 -h 480  -f yuv420p  --bitRate 1500000 --vbvBufSize 1800 --vbvMaxRate 2000 --gopSize 0 --bBPyramid 1"
md5sums_h264[i]=fa58fe34dcc068aeda7d8c390e4df210
md5sums_h265[i]=4912972f7145e347b21191badbcf2c78
case_names[i]=JCTVC_480P_YUV420P_bBPyramid_02
i=$(($i+1))

#BASIC bBPyramid test BasketballPass_416x240_50.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballPass_416x240_50.yuv
params[i]="-w 416 -h 240  -f yuv420p  --bitRate 600000 --vbvBufSize 720 --vbvMaxRate 800 --gopSize 0 --bBPyramid 0"
md5sums_h264[i]=0d2ea9a9a819c97c919be3a6f518003c
md5sums_h265[i]=69bb093a8a7e224f0985f8a795bc32c0
case_names[i]=JCTVC_240P_YUV420P_bBPyramid_01
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballPass_416x240_50.yuv
params[i]="-w 416 -h 240  -f yuv420p  --bitRate 600000 --vbvBufSize 720 --vbvMaxRate 800 --gopSize 0 --bBPyramid 1"
md5sums_h264[i]=5d98344581e16788e4eabb01fba664b1
md5sums_h265[i]=e13082d0860433d84886c091e519efcf
case_names[i]=JCTVC_240P_YUV420P_bBPyramid_02
i=$(($i+1))

#BASIC llRc test Kimono1_1920x1080_24.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/Kimono1_1920x1080_24.yuv
params[i]="-w 1920 -h 1080  -f yuv420p  --bitRate 8000000 --vbvBufSize 12000 --vbvMaxRate 16000 --llRc 0"
md5sums_h264[i]=4b90696403a3cf06a315c25395dbcbd2
md5sums_h265[i]=7061a9a372bc2cba0455f05c9a2e651f
case_names[i]=JCTVC_1080P_YUV420P_llRc_01
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/Kimono1_1920x1080_24.yuv
params[i]="-w 1920 -h 1080  -f yuv420p  --bitRate 8000000 --vbvBufSize 12000 --vbvMaxRate 16000 --llRc 2"
md5sums_h264[i]=7a946a55a89c435b694c3d6677ed57dd
md5sums_h265[i]=c86a8507ec3f8e9b02801503887c8c5e
case_names[i]=JCTVC_1080P_YUV420P_llRc_02
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/Kimono1_1920x1080_24.yuv
params[i]="-w 1920 -h 1080  -f yuv420p  --bitRate 8000000 --vbvBufSize 12000 --vbvMaxRate 16000 --llRc 4"
md5sums_h264[i]=8821e63ea110b74951c597f299cb3b1d
md5sums_h265[i]=2640e328e3aa8af1befb6993de3d07f3
case_names[i]=JCTVC_1080P_YUV420P_llRc_03
i=$(($i+1))

#BASIC llRc BasketballDrill_832x480_50.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballDrill_832x480_50.yuv
params[i]="-w 832 -h 480  -f yuv420p  --bitRate 1500000 --vbvBufSize 1800 --vbvMaxRate 2000 --llRc 0"
md5sums_h264[i]=83a6182fab136b47a7f7844c27ad26b6
md5sums_h265[i]=f849b83348ad6e2b4b388abdda5e0c0c
case_names[i]=JCTVC_480P_YUV420P_llRc_01
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballDrill_832x480_50.yuv
params[i]="-w 832 -h 480  -f yuv420p  --bitRate 1500000 --vbvBufSize 1800 --vbvMaxRate 2000 --llRc 2"
md5sums_h264[i]=606ca90d7d7c7aebc8f433041054dace
md5sums_h265[i]=85e27c721ce93802bfa798d2069509f8
case_names[i]=JCTVC_480P_YUV420P_llRc_02
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballDrill_832x480_50.yuv
params[i]="-w 832 -h 480  -f yuv420p  --bitRate 1500000 --vbvBufSize 1800 --vbvMaxRate 2000 --llRc 4"
md5sums_h264[i]=812e140292c7555c619fda377a269bb4
md5sums_h265[i]=b22b996bbe22add9cec32aca9502859e
case_names[i]=JCTVC_480P_YUV420P_llRc_03
i=$(($i+1))

#BASIC llRc test BasketballPass_416x240_50.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballPass_416x240_50.yuv
params[i]="-w 416 -h 240  -f yuv420p  --bitRate 600000 --vbvBufSize 720 --vbvMaxRate 800 --llRc 0"
md5sums_h264[i]=6f5f413151390e5df9cd466df4ac3e98
md5sums_h265[i]=26f1c7806b448dce4ed958dd686ea484
case_names[i]=JCTVC_240P_YUV420P_llRc_01
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballPass_416x240_50.yuv
params[i]="-w 416 -h 240  -f yuv420p  --bitRate 600000 --vbvBufSize 720 --vbvMaxRate 800 --llRc 2"
md5sums_h264[i]=62c4044b0b0b911a476afea8a368b42c
md5sums_h265[i]=5935af3d3fd58dff037f96edf4907566
case_names[i]=JCTVC_240P_YUV420P_llRc_02
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballPass_416x240_50.yuv
params[i]="-w 416 -h 240  -f yuv420p  --bitRate 600000 --vbvBufSize 720 --vbvMaxRate 800 --llRc 4"
md5sums_h264[i]=f0628d3d5b1bacd68bd602a713ab0e2d
md5sums_h265[i]=b76945f552695ec3efc03092d3cdf56c
case_names[i]=JCTVC_240P_YUV420P_llRc_03
i=$(($i+1))

#BASIC roi test Kimono1_1920x1080_24.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/Kimono1_1920x1080_24.yuv
params[i]="-w 1920 -h 1080  -f yuv420p  --bitRate 8000000 --vbvBufSize 12000 --vbvMaxRate 16000 --roiInt 0"
md5sums_h264[i]=4b90696403a3cf06a315c25395dbcbd2
md5sums_h265[i]=7061a9a372bc2cba0455f05c9a2e651f
case_names[i]=JCTVC_1080P_YUV420P_ROI_01
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/Kimono1_1920x1080_24.yuv
params[i]="-w 1920 -h 1080  -f yuv420p  --bitRate 8000000 --vbvBufSize 12000 --vbvMaxRate 16000 --roiType 1 --roiInt 1 --roiParam top=0,left=0,bottom=512,right=512,qpType=0,qpValue=18"
md5sums_h264[i]=7ac6070fd5a5c9606938cf181e9fcd1c
md5sums_h265[i]=2d071d79b28aa76fa6b49c216992712e
case_names[i]=JCTVC_1080P_YUV420P_ROI_02
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/Kimono1_1920x1080_24.yuv
params[i]="-w 1920 -h 1080  -f yuv420p  --bitRate 8000000 --vbvBufSize 12000 --vbvMaxRate 16000 --roiType 1 --roiInt 8 --roiParam top=0,left=0,bottom=512,right=512,qpType=0,qpValue=18"
md5sums_h264[i]=34bcde409e07c6835b900cbae85e0925
md5sums_h265[i]=12a93852aba4f7a856fd26149883851e
case_names[i]=JCTVC_1080P_YUV420P_ROI_03
i=$(($i+1))

#BASIC roi BasketballDrill_832x480_50.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballDrill_832x480_50.yuv
params[i]="-w 832 -h 480  -f yuv420p  --bitRate 1500000 --vbvBufSize 1800 --vbvMaxRate 2000 --roiInt 0"
md5sums_h264[i]=83a6182fab136b47a7f7844c27ad26b6
md5sums_h265[i]=f849b83348ad6e2b4b388abdda5e0c0c
case_names[i]=JCTVC_480P_YUV420P_ROI_01
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballDrill_832x480_50.yuv
params[i]="-w 832 -h 480  -f yuv420p  --bitRate 1500000 --vbvBufSize 1800 --vbvMaxRate 2000 --roiType 1 --roiInt 1 --roiParam top=0,left=0,bottom=256,right=256,qpType=0,qpValue=23"
md5sums_h264[i]=857ac0e2f3755a4899bf14399c56e58b
md5sums_h265[i]=5c3a79e8c24f6eb92d48228e5726cd92
case_names[i]=JCTVC_480P_YUV420P_ROI_02
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballDrill_832x480_50.yuv
params[i]="-w 832 -h 480  -f yuv420p  --bitRate 1500000 --vbvBufSize 1800 --vbvMaxRate 2000 --roiType 1 --roiInt 8 --roiParam top=0,left=0,bottom=256,right=256,qpType=0,qpValue=23"
md5sums_h264[i]=034371a6c0c97fe9f17f2f4e7bd18f99
md5sums_h265[i]=2432fa9cd819bb0abae4b68d02a4fc43
case_names[i]=JCTVC_480P_YUV420P_ROI_03
i=$(($i+1))

#BASIC roi test BasketballPass_416x240_50.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballPass_416x240_50.yuv
params[i]="-w 416 -h 240  -f yuv420p  --bitRate 600000 --vbvBufSize 720 --vbvMaxRate 800 --roiInt 0"
md5sums_h264[i]=6f5f413151390e5df9cd466df4ac3e98
md5sums_h265[i]=26f1c7806b448dce4ed958dd686ea484
case_names[i]=JCTVC_240P_YUV420P_ROI_01
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballPass_416x240_50.yuv
params[i]="-w 416 -h 240  -f yuv420p  --bitRate 600000 --vbvBufSize 720 --vbvMaxRate 800 --roiType 1 --roiInt 1 --roiParam top=0,left=0,bottom=128,right=128,qpType=0,qpValue=28"
md5sums_h264[i]=4f42f49688c10ef3ba5bccb9bd2ad258
md5sums_h265[i]=06d80ace52dae8e3259526bb42935ef6
case_names[i]=JCTVC_240P_YUV420P_ROI_02
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballPass_416x240_50.yuv
params[i]="-w 416 -h 240  -f yuv420p  --bitRate 600000 --vbvBufSize 720 --vbvMaxRate 800 --roiType 1 --roiInt 8 --roiParam top=0,left=0,bottom=128,right=128,qpType=0,qpValue=28"
md5sums_h264[i]=2bbfdf2d4d7a195a134f3b47c0d7f3ae
md5sums_h265[i]=6a216a90de046d4e768f7e273546cf2a
case_names[i]=JCTVC_240P_YUV420P_ROI_03
i=$(($i+1))

#BASIC SEI test Kimono1_1920x1080_24.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/Kimono1_1920x1080_24.yuv
params[i]="-w 1920 -h 1080  -f yuv420p  --bitRate 8000000 --vbvBufSize 12000 --vbvMaxRate 16000 --extSEIInt 0"
md5sums_h264[i]=4b90696403a3cf06a315c25395dbcbd2
md5sums_h265[i]=7061a9a372bc2cba0455f05c9a2e651f
case_names[i]=JCTVC_1080P_YUV420P_SEI_01
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/Kimono1_1920x1080_24.yuv
params[i]="-w 1920 -h 1080  -f yuv420p  --bitRate 8000000 --vbvBufSize 12000 --vbvMaxRate 16000 --extSEIInt 1"
md5sums_h264[i]=804510f8f0f03b15e8de57b9a81bcd43
md5sums_h265[i]=3b6cf43c77ee6da16237bd6f580f890d
case_names[i]=JCTVC_1080P_YUV420P_SEI_02
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/Kimono1_1920x1080_24.yuv
params[i]="-w 1920 -h 1080  -f yuv420p  --bitRate 8000000 --vbvBufSize 12000 --vbvMaxRate 16000 --extSEIInt 8"
md5sums_h264[i]=cd6f2cc90af81a995e3f9c5b2d9ca224
md5sums_h265[i]=029ca6e4d100ccdd7e85906a1fc6c6fd
case_names[i]=JCTVC_1080P_YUV420P_SEI_03
i=$(($i+1))

#BASIC SEI BasketballDrill_832x480_50.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballDrill_832x480_50.yuv
params[i]="-w 832 -h 480  -f yuv420p  --bitRate 1500000 --vbvBufSize 1800 --vbvMaxRate 2000 --extSEIInt 0"
md5sums_h264[i]=83a6182fab136b47a7f7844c27ad26b6
md5sums_h265[i]=f849b83348ad6e2b4b388abdda5e0c0c
case_names[i]=JCTVC_480P_YUV420P_SEI_01
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballDrill_832x480_50.yuv
params[i]="-w 832 -h 480  -f yuv420p  --bitRate 1500000 --vbvBufSize 1800 --vbvMaxRate 2000 --extSEIInt 1"
md5sums_h264[i]=f07b89ee0d692ebd33d238da09447946
md5sums_h265[i]=7ee8159cfee367c5c6d70e724262c072
case_names[i]=JCTVC_480P_YUV420P_SEI_02
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballDrill_832x480_50.yuv
params[i]="-w 832 -h 480  -f yuv420p  --bitRate 1500000 --vbvBufSize 1800 --vbvMaxRate 2000 --extSEIInt 8"
md5sums_h264[i]=25b5ccd701793943d8fe50d9b94dbb4b
md5sums_h265[i]=84e4ca6c03ec97964dc42e5b48e064cd
case_names[i]=JCTVC_480P_YUV420P_SEI_03
i=$(($i+1))

#BASIC SEI test BasketballPass_416x240_50.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballPass_416x240_50.yuv
params[i]="-w 416 -h 240  -f yuv420p  --bitRate 600000 --vbvBufSize 720 --vbvMaxRate 800 --extSEIInt 0"
md5sums_h264[i]=6f5f413151390e5df9cd466df4ac3e98
md5sums_h265[i]=26f1c7806b448dce4ed958dd686ea484
case_names[i]=JCTVC_240P_YUV420P_SEI_01
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballPass_416x240_50.yuv
params[i]="-w 416 -h 240  -f yuv420p  --bitRate 600000 --vbvBufSize 720 --vbvMaxRate 800 --extSEIInt 1"
md5sums_h264[i]=a916b695273ed302e03f96f02d293774
md5sums_h265[i]=1c452f52904622578a44e83885344350
case_names[i]=JCTVC_240P_YUV420P_SEI_02
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballPass_416x240_50.yuv
params[i]="-w 416 -h 240  -f yuv420p  --bitRate 600000 --vbvBufSize 720 --vbvMaxRate 800 --extSEIInt 8"
md5sums_h264[i]=78f4c131c9cb58f172dd5149022d2d5a
md5sums_h265[i]=5239ca050ed1f27cf43c564ebd2aba33
case_names[i]=JCTVC_240P_YUV420P_SEI_03
i=$(($i+1))

#BASIC forceIDR test Kimono1_1920x1080_24.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/Kimono1_1920x1080_24.yuv
params[i]="-w 1920 -h 1080  -f yuv420p  --bitRate 8000000 --vbvBufSize 12000 --vbvMaxRate 16000 --gopSize 1 --forceIDRInt 0"
md5sums_h264[i]=4b90696403a3cf06a315c25395dbcbd2
md5sums_h265[i]=7061a9a372bc2cba0455f05c9a2e651f
case_names[i]=JCTVC_1080P_YUV420P_forceIDR_01
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/Kimono1_1920x1080_24.yuv
params[i]="-w 1920 -h 1080  -f yuv420p  --bitRate 8000000 --vbvBufSize 12000 --vbvMaxRate 16000 --gopSize 1 --forceIDRInt 125"
md5sums_h264[i]=66c8001c40b3a6733b562d9c1382b4fb
md5sums_h265[i]=324a2ff6038b7d5ae9fb7a69e46aa0b3
case_names[i]=JCTVC_1080P_YUV420P_forceIDR_02
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/Kimono1_1920x1080_24.yuv
params[i]="-w 1920 -h 1080  -f yuv420p  --bitRate 8000000 --vbvBufSize 12000 --vbvMaxRate 16000 --gopSize 1 --forceIDRInt 250"
md5sums_h264[i]=4b90696403a3cf06a315c25395dbcbd2
md5sums_h265[i]=7061a9a372bc2cba0455f05c9a2e651f
case_names[i]=JCTVC_1080P_YUV420P_forceIDR_03
i=$(($i+1))

#BASIC forceIDR BasketballDrill_832x480_50.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballDrill_832x480_50.yuv
params[i]="-w 832 -h 480  -f yuv420p  --bitRate 1500000 --vbvBufSize 1800 --vbvMaxRate 2000 --gopSize 1 --forceIDRInt 0"
md5sums_h264[i]=83a6182fab136b47a7f7844c27ad26b6
md5sums_h265[i]=f849b83348ad6e2b4b388abdda5e0c0c
case_names[i]=JCTVC_480P_YUV420P_forceIDR_01
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballDrill_832x480_50.yuv
params[i]="-w 832 -h 480  -f yuv420p  --bitRate 1500000 --vbvBufSize 1800 --vbvMaxRate 2000 --gopSize 1 --forceIDRInt 125"
md5sums_h264[i]=15b2c947ed45b5d0f2ebef06e047dd72
md5sums_h265[i]=3b78c7440c5865ec0e48169605f8110c
case_names[i]=JCTVC_480P_YUV420P_forceIDR_02
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballDrill_832x480_50.yuv
params[i]="-w 832 -h 480  -f yuv420p  --bitRate 1500000 --vbvBufSize 1800 --vbvMaxRate 2000 --gopSize 1 --forceIDRInt 250"
md5sums_h264[i]=6d6e1d6360a9ead6fcd6e4862c328fc4
md5sums_h265[i]=0b528a80e36ef767b81ac1b5a6e77fdd
case_names[i]=JCTVC_480P_YUV420P_forceIDR_03
i=$(($i+1))

#BASIC forceIDR test BasketballPass_416x240_50.yuv
filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballPass_416x240_50.yuv
params[i]="-w 416 -h 240  -f yuv420p  --bitRate 600000 --vbvBufSize 720 --vbvMaxRate 800 --gopSize 1 --forceIDRInt 0"
md5sums_h264[i]=6f5f413151390e5df9cd466df4ac3e98
md5sums_h265[i]=26f1c7806b448dce4ed958dd686ea484
case_names[i]=JCTVC_240P_YUV420P_forceIDR_01
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballPass_416x240_50.yuv
params[i]="-w 416 -h 240  -f yuv420p  --bitRate 600000 --vbvBufSize 720 --vbvMaxRate 800 --gopSize 1 --forceIDRInt 125"
md5sums_h264[i]=19ace6821389f95c6343e81d318b66f3
md5sums_h265[i]=1c7bbda47fb119f1e802b274d3dab774
case_names[i]=JCTVC_240P_YUV420P_forceIDR_02
i=$(($i+1))

filenames[i]=/home/vastai/resource/dataset/jctvc/BasketballPass_416x240_50.yuv
params[i]="-w 416 -h 240  -f yuv420p  --bitRate 600000 --vbvBufSize 720 --vbvMaxRate 800 --gopSize 1 --forceIDRInt 250"
md5sums_h264[i]=8129fdb1d1dd399da382cb3de8963028
md5sums_h265[i]=053dae4f5daa93fc2f4fac73510ace26
case_names[i]=JCTVC_240P_YUV420P_forceIDR_03
i=$(($i+1))

file_num=$((${#filenames[@]} - 1))
#file_num=1

output_path=./video_enc_out
log_path=$output_path/encode_test.log
dieID=0
mkdir -p $output_path

if [ -z "$1" ]
then
  echo "Default:"
  repeat=0
  random=0
else
  repeat=$1
  random=$1
fi

function rand(){
  min=$1
  max=$(($2-$min+1))
  num=$(($RANDOM+1000000000)) 
  echo $(($num%$max+$min))
}

echo "repeat =" $repeat
echo "random =" $random


while true
do {

echo "video encode test start ("${#filenames[@]}") files:" 
echo '----------------------------------------------'
for i in $(seq 0 $file_num)
do
    #echo "--"$i
    #echo ${filenames[i]}
    if (( $random > 0 )); then
        i=$(rand 0 $file_num)
    fi

    echo "Case "$i": "${case_names[i]}
    echo $i":encoding: "${filenames[i]}

    #continue
    input_file=${filenames[i]}
    input_base=$(basename $input_file)
    file_name=${input_base%.*}
    output_file_264=$output_path/${file_name}_$i.264
    output_file_265=$output_path/${file_name}_$i.265
    #echo output: $output_file
#h264
    ../../sample/build/out/video_enc -i $input_file -o $output_file_264 ${params[i]} --logLevel 2 -c 0 -d /dev/vastai_video0 > ${log_path} 2>&1
    h264_kbps=`grep -o 'kbps: [0-9]*\.[0-9]*' ${log_path} | tail -1 |  awk '{print}'`
    h264_psnr=`grep -o 'Average PSNR: Y [0-9]*\.[0-9]*, U [0-9]*\.[0-9]*, V [0-9]*\.[0-9]*' ${log_path}  |  awk '{print}'`
    echo H264 $h264_kbps $h264_psnr

    result_264=`md5sum $output_file_264 |  awk '{print $1}'`
    if [ $result_264 = ${md5sums_h264[i]} ]; then
        echo h264 md5 check PASS!!!
        pass_num=$(($pass_num+1))
        #rm $output_file
    else
        echo h264 FAIL!!! $result_264 '!=' ${md5sums_h264[i]}
        echo params: h264 video_enc -i $input_file -o $output_file_264 ${params[i]}  -c 0 -d /dev/vastai_video0
        repeat=0 
        fail_num=$(($fail_num+1))
        #break
    fi
#h265
    ../../sample/build/out/video_enc -i $input_file -o $output_file_265 ${params[i]} --logLevel 2 -c 1 -d /dev/vastai_video0 > ${log_path} 2>&1
    h265_kbps=`grep -o 'kbps: [0-9]*\.[0-9]*' ${log_path} | tail -1 |  awk '{print}'`
    h265_psnr=`grep -o 'Average PSNR: Y [0-9]*\.[0-9]*, U [0-9]*\.[0-9]*, V [0-9]*\.[0-9]*' ${log_path}  |  awk '{print}'`
    echo H265 $h265_kbps $h265_psnr
    
    result_265=`md5sum $output_file_265 |  awk '{print $1}'`
    if [ $result_265 = ${md5sums_h265[i]} ]; then
        echo h265 md5 check PASS!!!
        pass_num=$(($pass_num+1))
        #rm $output_file
    else
        echo h265 FAIL!!! $result_265 '!=' ${md5sums_h265[i]}
        echo params: h265 video_enc -i $input_file -o $output_file_265 ${params[i]}  -c 1 -d /dev/vastai_video0
        repeat=0 
        fail_num=$(($fail_num+1))
        #break
    fi
    echo
done

echo ----------------------------------------------
echo video encode test end, pass: $pass_num, fail: $fail_num

if (( $repeat <= 0 )); then
    break
fi
}


done
