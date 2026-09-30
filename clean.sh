#!/bin/bash

make -C runtime_demo/driver_vpu/memalloc clean

make -C runtime_demo/driver_vpu/vcodec clean

make -C runtime_demo/userspace_vpu/VC9000E/software clean

rm -rf runtime_demo/userspace_vpu/VC9000E/build/
rm -rf runtime_demo/userspace_vpu/VC9000D/.depend
rm -rf runtime_demo/userspace_vpu/VC9000D/build/
rm -rf runtime_demo/userspace_vpu/VC9000SDK/build/
rm -rf runtime_demo/userspace_vpu/VC9000SDK/debug/
rm -rf runtime_demo/userspace_vpu/VC9000SDK/lib/
rm -rf runtime_demo/userspace_vpu/VC9000SDK/sample/build/
rm -rf runtime_demo/userspace_vpu/VC9000SDK/src/dec/lib/.depend
rm -rf runtime_demo/userspace_vpu/VC9000SDK/src/dec/lib/bin/
rm -rf runtime_demo/userspace_vpu/VC9000SDK/src/dec/lib/out/
rm -rf runtime_demo/userspace_vpu/VC9000SDK/src/enc/lib/bin/

