cmake_minimum_required(VERSION 3.12)

project(vastai_video_sdk)

message("-- vastai_video_sdk")

# 除 libvmpp-*.so 外，额外产出自包含的静态库 libvmpp-dec.a / libvmpp-enc.a
# （内部已合并 VSI 的 *.a 成员，sample 可直接静态链接）
option(BUILD_STATIC_LIBS "Build self-contained static libvmpp-*.a" ON)
message(STATUS "BUILD_STATIC_LIBS: ${BUILD_STATIC_LIBS}")

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

include(${CMAKE_SOURCE_DIR}/version.cmake)

if(MSVC)
	#TODO
else()
	if(RELEASE_BUILD)
		#do not increase build number automatically
		#math(EXPR lib_version_BUILD "${LIB_VERSION_BUILD} + 1")
		#set(NEW_VERSION_STR "SET(LIB_DEC_VERSION_MAJOR ${LIB_DEC_VERSION_MAJOR})\nSET(LIB_DEC_VERSION_MINOR ${LIB_DEC_VERSION_MINOR})\n\nSET(LIB_ENC_VERSION_MAJOR ${LIB_ENC_VERSION_MAJOR})\nSET(LIB_ENC_VERSION_MINOR ${LIB_ENC_VERSION_MINOR})\n\nSET(LIB_VERSION_BUILD ${lib_version_BUILD})")
		#file (WRITE ${CMAKE_SOURCE_DIR}/version.cmake ${NEW_VERSION_STR})
		#set(lib_version_BUILD ${LIB_VERSION_BUILD})
		# safe compile
        if (OPTIMIZED_DEBUG)
            set(CMAKE_C_FLAGS_RELEASE "-D_FORTIFY_SOURCE=2 -g3 -O3 ${CMAKE_C_FLAGS_RELEASE}")
            set(CMAKE_CXX_FLAGS_RELEASE "-D_FORTIFY_SOURCE=2 -g3 -O3 ${CMAKE_CXX_FLAGS_RELEASE}")
            message(STATUS "OPTIMIZED DEBUG ENABLED: Added -g3 -O3 flag")
        else()
		    set(CMAKE_C_FLAGS_RELEASE "-DNDEBUG=1 -D_NDEBUG=1 -D_FORTIFY_SOURCE=2 -O2 ${CMAKE_C_FLAGS_RELEASE}")
		    set(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG=1 -D_NDEBUG=1 -D_FORTIFY_SOURCE=2 -O2 ${CMAKE_CXX_FLAGS_RELEASE}")
        endif()

		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -fpie -fstack-protector-all")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -fpie -fstack-protector-all")
		set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -pie -Wl,-z,noexecstack -Wl,-z,relro -Wl,-z,now")
        if (NOT OPTIMIZED_DEBUG)
            set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -s")
            message(STATUS "STRIPPING SYMBOLS: Binary will be stripped for release")
            endif()
	else()
		#set(lib_version_BUILD ${LIB_VERSION_BUILD})
		if (SANITIZER_MODE STREQUAL "1")
			set(SANITIZE_FLAGS "-fsanitize=address") # -fsanitize=leak -fsanitize=undefined")
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${SANITIZE_FLAGS}")
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SANITIZE_FLAGS}")
		elseif (SANITIZER_MODE STREQUAL "2")
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=thread -g -O1 -fno-omit-frame-pointer -fPIC")
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread -g -O1 -fno-omit-frame-pointer -fPIC")
			set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=thread -fPIC -pie")
		endif()
		set(CMAKE_C_FLAGS_DEBUG "-DDEBUG=1 -D_DEBUG=1 ${CMAKE_C_FLAGS_DEBUG}")
		set(CMAKE_CXX_FLAGS_DEBUG "-DDEBUG=1 -D_DEBUG=1 ${CMAKE_C_FLAGS_DEBUG}")
		add_definitions("-Wall -g")
	endif()

	if (ANDROID_32BIT STREQUAL "1")
		add_definitions(-DANDROID_32BIT)
	endif()

	if (VIDEO_COMPAT STREQUAL "1")
    	add_definitions(-DVIDEO_COMPAT)
    endif()
endif()

configure_file(${CMAKE_SOURCE_DIR}/src/va_version.h.in ${CMAKE_SOURCE_DIR}/src/va_version.h)

###########################################################################################
#	static check
###########################################################################################
set (CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" "${CMAKE_SOURCE_DIR}/ut/static_check")
include(static_check)

###########################################################################################
#	include ctest
###########################################################################################
include(CTest)

###########################################################################################
#	subdirectory
###########################################################################################
add_subdirectory(src)     # source code directory
if (ENABLE_UNIT_TEST)
	add_subdirectory(ut)     # unit test directory
endif ()
#add_subdirectory(sample)

