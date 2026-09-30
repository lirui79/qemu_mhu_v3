cmake_minimum_required(VERSION 3.12)

project(vmpp_sample)

message("-- vastai_video_sdk sample")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(${CMAKE_C_COMPILER_ID} MATCHES "Clang" OR ${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
	set(CMAKE_COMPILER_IS_CLANG TRUE)
endif()

if(LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES "(i[3-6]86|x86|x64|x86_64|amd64|e2k)")
	if(NOT MSVC)
		set(ARCH_SIMD_FLAGS "-mmmx" "-msse" "-msse2")
	endif()
elseif(LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES "^(powerpc|ppc)64(le)?")
	set(ARCH_SIMD_DEFINES "-DNO_WARN_X86_INTRINSICS")
	set(ARCH_SIMD_FLAGS "-mvsx")
	add_compile_definitions(NO_WARN_X86_INTRINSICS)
else()
	if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DSIMDE_ENABLE_OPENMP")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DSIMDE_ENABLE_OPENMP")
	endif()
	set(ARCH_SIMD_FLAGS "")
	message(STATUS "No Native SSE2 SIMD Support - Using SIMDE")
endif()

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANG)
	set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wvla -Wno-unused-function -Wno-missing-field-initializers ${CMAKE_CXX_FLAGS} -fno-strict-aliasing")
	set(CMAKE_C_FLAGS "-Wall -Wextra -Wvla -Wno-unused-function -Werror-implicit-function-declaration -Wno-missing-braces -Wno-missing-field-initializers ${CMAKE_C_FLAGS} -std=gnu99 -fno-strict-aliasing")

	option(USE_LIBC++ "Use libc++ instead of libstdc++" ${APPLE})
	if(USE_LIBC++)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
	endif()
elseif(MSVC)
	#TODO
endif()

if(CMAKE_BUILD_TYPE AND (CMAKE_BUILD_TYPE STREQUAL "Release"))
	set(RELEASE_BUILD "1")
elseif(CMAKE_BUILD_TYPE AND (CMAKE_BUILD_TYPE STREQUAL "Debug"))
	set(RELEASE_BUILD "0")
else()
	set(RELEASE_BUILD "0")
	message("Did not specify build type, using default :Debug")
endif()

if(ENABLE_DYNAMIC_RES)
    add_definitions(-DENABLE_DYNAMIC_RES)
endif()

if (ENABLE_ANDROID_ARM64 STREQUAL "1")
    set(PTHREAD_LIB "")
else()
    set(PTHREAD_LIB "pthread")
endif()

if (BUILD_ARM64 STREQUAL "1")
   add_definitions(-DARM64)
endif()

if (USING_FFMPEG STREQUAL "1")
   add_definitions(-DUSING_FFMPEG)
endif()

if(MSVC)
	#TODO
else()
	if(RELEASE_BUILD)
		set(CMAKE_C_FLAGS_RELEASE "-DNDEBUG=1 -D_NDEBUG=1 ${CMAKE_C_FLAGS_RELEASE}")
		set(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG=1 -D_NDEBUG=1 ${CMAKE_CXX_FLAGS_RELEASE}")
	else()
		if (SANITIZER_MODE STREQUAL "1")
			set(SANITIZE_FLAGS "-fsanitize=address -fno-omit-frame-pointer") # -fsanitize=leak -fsanitize=undefined")
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${SANITIZE_FLAGS}")
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SANITIZE_FLAGS}")
			set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
		elseif (SANITIZER_MODE STREQUAL "2")
			set(SANITIZE_FLAGS "-fsanitize=thread -g -O1 -fno-omit-frame-pointer -fPIC")
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${SANITIZE_FLAGS}")
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SANITIZE_FLAGS}")
			set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=thread -fPIC -pie")
		endif()
		set(CMAKE_C_FLAGS_DEBUG "-DDEBUG=1 -D_DEBUG=1 ${CMAKE_C_FLAGS_DEBUG}")
		set(CMAKE_CXX_FLAGS_DEBUG "-DDEBUG=1 -D_DEBUG=1 ${CMAKE_C_FLAGS_DEBUG}")
		add_definitions("-Wall -g")
	endif()

endif()

set(VMPP_SDK_ROOT ${CMAKE_SOURCE_DIR}/..)
set(VMPP_LIB_DIR ${VMPP_SDK_ROOT}/lib)
set(VMPP_INC_DIR ${VMPP_SDK_ROOT}/inc)

# sample 默认静态链接自包含的 libvmpp-*.a；置 0 则用原来的 libvmpp-*.so
option(LINK_STATIC "Link samples against static libvmpp-*.a" ON)
message(STATUS "LINK_STATIC: ${LINK_STATIC}")

if (LINK_STATIC)
    set(VMPP_DEC_LIB ${VMPP_LIB_DIR}/libvmpp-dec.a)
    set(VMPP_ENC_LIB ${VMPP_LIB_DIR}/libvmpp-enc.a)
    # 静态链接时不能加 --whole-archive：
    #  1) 自包含 .a 里已经并入了全部 VSI 成员，按需抽取即可，否则可执行文件会白白膨胀；
    #  2) libvmpp-dec.a / libvmpp-enc.a 各自都编译了同一份 va_log.c、va_utils.c，
    #     VSI 侧 dec/enc 还各有一份 fifo.c，整体拉入会 multiple definition
    #     （动态库时代两个 .so 符号表互相独立，所以没有这个问题）。
    set(VMPP_WHOLE_ARCHIVE "")
    set(VMPP_NO_WHOLE_ARCHIVE "")
else ()
    set(VMPP_DEC_LIB ${VMPP_LIB_DIR}/libvmpp-dec.so)
    set(VMPP_ENC_LIB ${VMPP_LIB_DIR}/libvmpp-enc.so)
    set(VMPP_WHOLE_ARCHIVE -Wl,--whole-archive)
    set(VMPP_NO_WHOLE_ARCHIVE -Wl,--no-whole-archive)
endif ()

# guest rootfs 是纯静态 busybox：没有 /lib，也没有 ld-linux-aarch64.so.1 / libc.so.6。
# 只把 libvmpp-*.a 静态化还不够，可执行文件本身仍需解释器，execve 会直接 ENOENT，
# 表现为 shell 报 "xxx: not found"（文件明明在 /bin 里）。所以样本要 -static 全静态链接。
option(EXE_STATIC "Link sample executables fully static (-static)" ${LINK_STATIC})
if (EXE_STATIC AND NOT MSVC
        AND NOT (SANITIZER_MODE STREQUAL "1" OR SANITIZER_MODE STREQUAL "2"))
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
    # 各样本的链接参数里原本带 -Wl,-Bdynamic（只为让 libc/libstdc++ 保持动态），
    # 全静态时必须去掉，否则它会把链接器切回动态，-static 形同虚设。
    set(VMPP_BDYNAMIC "")
else ()
    set(VMPP_BDYNAMIC -Wl,-Bdynamic)
endif ()
message(STATUS "EXE_STATIC: ${EXE_STATIC}")

if (BUILD_TARGET STREQUAL "0")
	if (EXISTS ${CMAKE_SOURCE_DIR}/dec)
		add_subdirectory(dec)
	endif()
	if (EXISTS ${CMAKE_SOURCE_DIR}/enc)
		add_subdirectory(enc)
	endif()
	# NOTE: the transcode samples depend on the platform runtime interfaces
	# (vmppRuntimeInstance/open_runtime/vaccrt_*) which are not provided by the SDK any more,
	# so they are removed from the build until the runtime interfaces are available again.
	if (EXISTS ${CMAKE_SOURCE_DIR}/transcode)
		add_subdirectory(transcode)
	endif()
	if (EXISTS ${CMAKE_SOURCE_DIR}/transcode_mt)
		add_subdirectory(transcode_mt)
	endif()
elseif (BUILD_TARGET STREQUAL "1")
	if (EXISTS ${CMAKE_SOURCE_DIR}/dec)
		add_subdirectory(dec)
	endif()
elseif (BUILD_TARGET STREQUAL "2")
	if (EXISTS ${CMAKE_SOURCE_DIR}/enc)
    	add_subdirectory(enc)
	endif()
endif()
