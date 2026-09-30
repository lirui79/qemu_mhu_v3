cmake_minimum_required(VERSION 3.10)

project(vmpp-dec)
message("-- Making vmpp-dec project")

set(INC_DIR ${CMAKE_SOURCE_DIR}/inc)
set(LIB_DIR ${PROJECT_SOURCE_DIR}/lib/bin)
set(LIB_INC_DIR ${PROJECT_SOURCE_DIR}/lib/software/source/inc)
set(DEC_INC_DIR ${PROJECT_SOURCE_DIR}/inc)
set(DEC_SRC_DIR ${PROJECT_SOURCE_DIR}/src)
set(COMMON_SRC_DIR ${CMAKE_SOURCE_DIR}/src)
include_directories(
	${LIB_INC_DIR}
	${DEC_INC_DIR}
    ${INC_DIR}
    ${COMMON_SRC_DIR}
    ${PROJECT_SOURCE_DIR}/lib/software/source/common
    ${PROJECT_SOURCE_DIR}/lib/software/source/config
    ${PROJECT_SOURCE_DIR}/lib/software/source/jpeg
    ${PROJECT_SOURCE_DIR}/lib/software/source/av1
    ${PROJECT_SOURCE_DIR}/lib/software/source/hevc
    ${PROJECT_SOURCE_DIR}/lib/software/source/h264high
    ${PROJECT_SOURCE_DIR}/lib/software/source/vp9
    ${PROJECT_SOURCE_DIR}/lib/software/source/avs2
	)

set(vdec_HEADERS
    ${DEC_INC_DIR}/jpeg_decoder.h
    ${DEC_INC_DIR}/video_decoder.h
    ${DEC_SRC_DIR}/sei_decoder.h
    ${INC_DIR}/vmpp_common.h
    ${INC_DIR}/vmpp_dec_defs.h
    ${INC_DIR}/vmpp_dec_api.h
    ${COMMON_SRC_DIR}/va_vdata_internal.h
    ${COMMON_SRC_DIR}/va_utils.h
    ${COMMON_SRC_DIR}/va_log.h
    ${COMMON_SRC_DIR}/va_version.h
	)

set(vdec_SOURCES
    ${DEC_SRC_DIR}/vmpp_dec_api_impl.c
    ${DEC_SRC_DIR}/sei_decoder.c
    ${DEC_SRC_DIR}/jpeg_decoder.c
    ${DEC_SRC_DIR}/video_decoder.c
    ${COMMON_SRC_DIR}/va_utils.c
    ${COMMON_SRC_DIR}/va_log.c
	)

set(LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/build/out)

if(RELEASE_BUILD)
    set(CMAKE_C_FLAGS_RELEASE "-DNDEBUG=1 -D_NDEBUG=1 ${CMAKE_C_FLAGS_RELEASE}")
    set(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG=1 -D_NDEBUG=1 ${CMAKE_CXX_FLAGS_RELEASE}")
    add_compile_options(-g -O2  -fsigned-char  -fvisibility=hidden)
else()
    set(CMAKE_C_FLAGS_DEBUG "-DDEBUG=1 -D_DEBUG=1 ${CMAKE_C_FLAGS_DEBUG}")
    set(CMAKE_CXX_FLAGS_DEBUG "-DDEBUG=1 -D_DEBUG=1 ${CMAKE_C_FLAGS_DEBUG}")
    add_definitions("-Wall -g")
    add_compile_options(-g -O0  -fsigned-char  -fvisibility=hidden)
endif()

if (ENABLE_UNIT_TEST)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")
endif()

# VSI decoder 静态库（由 autobuild.sh 先编译好放到 src/dec/lib/bin）
set(DEC_VSI_LIBS
    ${LIB_DIR}/libcommon.a
    ${LIB_DIR}/libdechevc.a
    ${LIB_DIR}/libdech264.a
    ${LIB_DIR}/libdecav1.a
    ${LIB_DIR}/libdecvp9.a
    ${LIB_DIR}/libdecavs2.a
    ${LIB_DIR}/libdecjpeg.a
    ${LIB_DIR}/libdwl.a
    )

add_library(${PROJECT_NAME} SHARED
	${vdec_HEADERS}
	${vdec_SOURCES}
	)

target_link_libraries(
    ${PROJECT_NAME}
    PUBLIC
    -Wl,--whole-archive
    -Wl,-Bstatic
    ${DEC_VSI_LIBS}
    -Wl,-Bdynamic
    -Wl,--no-whole-archive
    )

install(TARGETS ${PROJECT_NAME} DESTINATION lib)
#install(DIRECTORY ${INC_DIR}/ DESTINATION inc)

################################################################################
#	static library: libvmpp-dec.a
#	把本项目的目标文件与 VSI 的 *.a 合并成一个自包含静态库，
#	sample 只需要链这一个 .a 即可（不需要再带一堆依赖库）
################################################################################
if (BUILD_STATIC_LIBS)
    add_library(${PROJECT_NAME}-objs STATIC
        ${vdec_HEADERS}
        ${vdec_SOURCES}
        )
    set_target_properties(${PROJECT_NAME}-objs PROPERTIES
        OUTPUT_NAME ${PROJECT_NAME}-objs
        POSITION_INDEPENDENT_CODE ON)

    set(DEC_STATIC_LIB ${LIBRARY_OUTPUT_PATH}/lib${PROJECT_NAME}.a)
    set(DEC_MERGE_SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/merge_${PROJECT_NAME}.ar)

    set(_merge_content "CREATE ${DEC_STATIC_LIB}\n")
    string(APPEND _merge_content "ADDLIB $<TARGET_FILE:${PROJECT_NAME}-objs>\n")
    foreach (_vsi_lib ${DEC_VSI_LIBS})
        string(APPEND _merge_content "ADDLIB ${_vsi_lib}\n")
    endforeach ()
    string(APPEND _merge_content "SAVE\nEND\n")
    file(GENERATE OUTPUT ${DEC_MERGE_SCRIPT} CONTENT "${_merge_content}")

    add_custom_command(
        OUTPUT ${DEC_STATIC_LIB}
        COMMAND ${CMAKE_COMMAND} -E rm -f ${DEC_STATIC_LIB}
        COMMAND sh -c "${CMAKE_AR} -M < ${DEC_MERGE_SCRIPT}"
        DEPENDS ${PROJECT_NAME}-objs ${DEC_VSI_LIBS}
        COMMENT "Merging ${PROJECT_NAME} objects and VSI libs into lib${PROJECT_NAME}.a"
        VERBATIM
        )

    add_custom_target(${PROJECT_NAME}-static ALL DEPENDS ${DEC_STATIC_LIB})
    install(FILES ${DEC_STATIC_LIB} DESTINATION lib)
endif ()