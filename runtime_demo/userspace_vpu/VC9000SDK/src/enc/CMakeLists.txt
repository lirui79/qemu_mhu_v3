cmake_minimum_required(VERSION 3.10)

project(vmpp-enc)
message("-- Making vmpp-enc project")

set(INC_DIR ${CMAKE_SOURCE_DIR}/inc)
set(LIB_DIR ${PROJECT_SOURCE_DIR}/lib/bin)
set(LIB_INC_DIR ${PROJECT_SOURCE_DIR}/lib/bin/inc)
set(ENC_INC_DIR ${PROJECT_SOURCE_DIR}/inc)
set(ENC_SRC_DIR ${PROJECT_SOURCE_DIR}/src)
set(COMMON_SRC_DIR ${CMAKE_SOURCE_DIR}/src)

include_directories(
	${LIB_INC_DIR}
	${ENC_INC_DIR}
    ${INC_DIR}
    ${COMMON_SRC_DIR}
	)

set(venc_HEADERS
    ${ENC_INC_DIR}/video_encoder.h
    ${ENC_INC_DIR}/jpeg_encoder.h
    ${INC_DIR}/vmpp_common.h
    ${INC_DIR}/vmpp_enc_defs.h
	${INC_DIR}/vmpp_enc_api.h
    ${COMMON_SRC_DIR}/va_vdata_internal.h
    ${COMMON_SRC_DIR}/va_utils.h
    ${COMMON_SRC_DIR}/va_log.h
    ${COMMON_SRC_DIR}/va_version.h
	)

set(venc_SOURCES
    ${ENC_SRC_DIR}/vmpp_enc_api_impl.c
    ${ENC_SRC_DIR}/video_encoder.c
    ${ENC_SRC_DIR}/jpeg_encoder.c
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

# VSI encoder 静态库（由 autobuild.sh 先编译好放到 src/enc/lib/bin）
set(ENC_VSI_LIBS
    ${LIB_DIR}/libh2enc.a
    )

add_library(${PROJECT_NAME} SHARED
	${venc_HEADERS}
	${venc_SOURCES}
	)

target_link_libraries(
    ${PROJECT_NAME}
    PUBLIC
    -Wl,--whole-archive
    -Wl,-Bstatic
    ${ENC_VSI_LIBS}
    -Wl,-Bdynamic
    -Wl,--no-whole-archive
    )

install(TARGETS ${PROJECT_NAME} DESTINATION lib)

################################################################################
#	static library: libvmpp-enc.a（自包含，已合并 VSI 静态库成员）
################################################################################
if (BUILD_STATIC_LIBS)
    add_library(${PROJECT_NAME}-objs STATIC
        ${venc_HEADERS}
        ${venc_SOURCES}
        )
    set_target_properties(${PROJECT_NAME}-objs PROPERTIES
        OUTPUT_NAME ${PROJECT_NAME}-objs
        POSITION_INDEPENDENT_CODE ON)

    set(ENC_STATIC_LIB ${LIBRARY_OUTPUT_PATH}/lib${PROJECT_NAME}.a)
    set(ENC_MERGE_SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/merge_${PROJECT_NAME}.ar)

    set(_merge_content "CREATE ${ENC_STATIC_LIB}\n")
    string(APPEND _merge_content "ADDLIB $<TARGET_FILE:${PROJECT_NAME}-objs>\n")
    foreach (_vsi_lib ${ENC_VSI_LIBS})
        string(APPEND _merge_content "ADDLIB ${_vsi_lib}\n")
    endforeach ()
    string(APPEND _merge_content "SAVE\nEND\n")
    file(GENERATE OUTPUT ${ENC_MERGE_SCRIPT} CONTENT "${_merge_content}")

    add_custom_command(
        OUTPUT ${ENC_STATIC_LIB}
        COMMAND ${CMAKE_COMMAND} -E rm -f ${ENC_STATIC_LIB}
        COMMAND sh -c "${CMAKE_AR} -M < ${ENC_MERGE_SCRIPT}"
        DEPENDS ${PROJECT_NAME}-objs ${ENC_VSI_LIBS}
        COMMENT "Merging ${PROJECT_NAME} objects and VSI libs into lib${PROJECT_NAME}.a"
        VERBATIM
        )

    add_custom_target(${PROJECT_NAME}-static ALL DEPENDS ${ENC_STATIC_LIB})
    install(FILES ${ENC_STATIC_LIB} DESTINATION lib)
endif ()