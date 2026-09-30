cmake_minimum_required(VERSION 3.18)
project(npu_codec_bridge VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ------------------------------------------------------------------
# Options
# ------------------------------------------------------------------
option(NPU_LIVE_MODE "Build with real VastAI NPU SDK (requires hardware)" OFF)

# ------------------------------------------------------------------
# Find dependencies
# ------------------------------------------------------------------

# Torch — provides its own bundled pybind11 (with type_caster<at::Tensor>)
find_package(Torch REQUIRED)
message(STATUS "Torch found: ${TORCH_INCLUDE_DIRS}")
# Torch includes its own pybind11 at torch/include/pybind11/

# DRM headers (for zero-copy mmap in LIVE mode)
find_path(LIBDRM_INCLUDE_DIR NAMES drm.h
    PATHS /usr/include/libdrm /usr/include/drm /usr/include
    PATH_SUFFIXES libdrm drm
)
if(LIBDRM_INCLUDE_DIR)
    message(STATUS "libdrm found: ${LIBDRM_INCLUDE_DIR}")
else()
    message(STATUS "libdrm NOT found — zero-copy mmap disabled")
endif()

# Python (for extension suffix)
find_package(Python COMPONENTS Interpreter Development.Module REQUIRED)
message(STATUS "Python include: ${Python_INCLUDE_DIRS}")
message(STATUS "Python lib:     ${Python_LIBRARIES}")

# ------------------------------------------------------------------
# Derive torch library directory (must be BEFORE target definitions)
# ------------------------------------------------------------------
foreach(_lib ${TORCH_LIBRARIES})
    get_filename_component(_torch_lib_dir "${_lib}" DIRECTORY)
    if(EXISTS "${_torch_lib_dir}/libtorch_python.so")
        set(TORCH_LIB_DIR "${_torch_lib_dir}")
        break()
    endif()
endforeach()
if(NOT TORCH_LIB_DIR)
    find_library(TORCH_PYTHON_LIB torch_python
        HINTS "${TORCH_INSTALL_PREFIX}/lib"
              "/usr/local/lib/python3/dist-packages/torch/lib"
    )
    if(TORCH_PYTHON_LIB)
        get_filename_component(TORCH_LIB_DIR "${TORCH_PYTHON_LIB}" DIRECTORY)
    endif()
endif()
message(STATUS "Torch lib dir: ${TORCH_LIB_DIR}")
link_directories("${TORCH_LIB_DIR}")

# ------------------------------------------------------------------
# NPU SDK (only needed in LIVE mode)
# ------------------------------------------------------------------
set(NPU_SDK_ROOT "${CMAKE_SOURCE_DIR}/.." CACHE PATH "Root of vastai_video_sdk")

if(NPU_LIVE_MODE)
    set(NPU_SDK_INC "${NPU_SDK_ROOT}/inc")
    set(NPU_SDK_LIB "${NPU_SDK_ROOT}/lib")

    if(EXISTS "${NPU_SDK_INC}/vmpp_dec_api.h")
        message(STATUS "NPU SDK headers found at ${NPU_SDK_INC}")
    else()
        message(FATAL_ERROR "NPU SDK headers not found at ${NPU_SDK_INC}")
    endif()

    if(EXISTS "${NPU_SDK_LIB}/libvmpp-dec.so")
        message(STATUS "NPU SDK library found at ${NPU_SDK_LIB}")
        link_directories("${NPU_SDK_LIB}")
    else()
        message(FATAL_ERROR "NPU SDK library not found at ${NPU_SDK_LIB}")
    endif()
endif()

# ------------------------------------------------------------------
# Library target: npu_codec_bridge._C
# ------------------------------------------------------------------
set(SOURCES
    src/pybind_module.cpp
)

if(NPU_LIVE_MODE)
    message(STATUS "=== BUILD MODE: LIVE (real NPU SDK) ===")
    list(APPEND SOURCES
        src/npu_decoder_wrapper.cpp
        src/npu_decoder_zero_copy.cpp
    )
    add_library(_C MODULE ${SOURCES})
    target_include_directories(_C PRIVATE
        include
        ${NPU_SDK_INC}
        ${TORCH_INCLUDE_DIRS}
        ${Python_INCLUDE_DIRS}
        ${LIBDRM_INCLUDE_DIR}              # DRM headers (auto-detected)
    )
    target_link_libraries(_C PRIVATE
        ${TORCH_LIBRARIES}
        torch_python
        vmpp-dec
        pthread
        dl
        ${Python_LIBRARIES}
    )
    target_compile_definitions(_C PRIVATE NPU_LIVE_MODE=1)
else()
    message(STATUS "=== BUILD MODE: STUB (mock decoder, no hardware needed) ===")
    add_library(_C MODULE ${SOURCES})
    target_include_directories(_C PRIVATE
        include
        ${TORCH_INCLUDE_DIRS}
        ${Python_INCLUDE_DIRS}
    )
    # Need torch_python for the pybind11 type_caster<at::Tensor> symbols
    target_link_libraries(_C PRIVATE
        ${TORCH_LIBRARIES}
        torch_python
        ${Python_LIBRARIES}
    )
    target_compile_definitions(_C PRIVATE NPU_STUB_MODE=1)
endif()
# Python extension naming: _C.cpython-312-x86_64-linux-gnu.so
set_target_properties(_C PROPERTIES
    PREFIX ""
    OUTPUT_NAME "_C"
    SUFFIX ".${Python_SOABI}${CMAKE_SHARED_MODULE_SUFFIX}"
)

# Install to the package directory
install(TARGETS _C
    LIBRARY DESTINATION npu_codec_bridge
)

message(STATUS "Build configuration complete.")
message(STATUS "  Live mode : ${NPU_LIVE_MODE}")
message(STATUS "  Python    : ${Python_EXECUTABLE}")
