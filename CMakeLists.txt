cmake_minimum_required(VERSION 3.24)
project(OwlSpectralRenderer LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES)
    set(CMAKE_CUDA_ARCHITECTURES 75 86)
endif()

# ===== 依赖路径：默认值 = 作者本机，用户可 -D 或环境变量覆盖 =====
set(OWL_SDK_DIR       "D:/OWL_SDK"                                              CACHE PATH "OWL SDK root")
set(OPTIX_SDK_DIR     "C:/ProgramData/NVIDIA Corporation/OptiX SDK 9.0.0"       CACHE PATH "OptiX SDK root")
set(CUDA_TOOLKIT_ROOT "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v13.3" CACHE PATH "CUDA Toolkit root")

if(DEFINED ENV{OWL_ROOT})
    set(OWL_SDK_DIR       "$ENV{OWL_ROOT}"   CACHE PATH "" FORCE)
endif()
if(DEFINED ENV{OPTIX_ROOT})
    set(OPTIX_SDK_DIR     "$ENV{OPTIX_ROOT}" CACHE PATH "" FORCE)
endif()
if(DEFINED ENV{CUDA_PATH})
    set(CUDA_TOOLKIT_ROOT "$ENV{CUDA_PATH}"  CACHE PATH "" FORCE)
endif()

foreach(_v OWL_SDK_DIR OPTIX_SDK_DIR CUDA_TOOLKIT_ROOT)
    if(NOT EXISTS "${${_v}}")
        message(FATAL_ERROR "${_v} = '${${_v}}' does not exist. Override with -D${_v}=<path>")
    endif()
endforeach()

if(WIN32)
    set(CUDA_LIB_DIR "${CUDA_TOOLKIT_ROOT}/lib/x64")
    set(NVCC_EXE     "${CUDA_TOOLKIT_ROOT}/bin/nvcc.exe")
else()
    set(CUDA_LIB_DIR "${CUDA_TOOLKIT_ROOT}/lib64")
    set(NVCC_EXE     "${CUDA_TOOLKIT_ROOT}/bin/nvcc")
endif()

# ===== 主 target =====
add_executable(OwlSpectralRenderer
    main.cpp
    shared.h
    mat4.h
    sampling.h
    spectral.h
    sphere.h
    camera.h
    toBMP.h
    meshGAS.h
    scene_nested_glass.h
    scene_cornell_box.h
    scene_refraction_rich.h
    scene_obj.h
)

target_include_directories(OwlSpectralRenderer PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${OWL_SDK_DIR}/include"
    "${OPTIX_SDK_DIR}/include"
    "${CUDA_TOOLKIT_ROOT}/include"
)

target_link_directories(OwlSpectralRenderer PRIVATE
    "${OWL_SDK_DIR}/lib"
    "${CUDA_LIB_DIR}"
)

# 跟你原版一致，去掉 .lib 后缀，Windows 自动补
target_link_libraries(OwlSpectralRenderer PRIVATE owl cudart cuda)

if(MSVC)
    target_compile_options(OwlSpectralRenderer PRIVATE
        $<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=/Zc:preprocessor>)
endif()

# ===== shaders.cu -> shaders.ptx =====
set(SHADER_PTX "${CMAKE_CURRENT_BINARY_DIR}/shaders.ptx")

if(MSVC)
    set(NVCC_XCOMPILER -Xcompiler /Zc:preprocessor)
else()
    set(NVCC_XCOMPILER)
endif()

add_custom_command(
    OUTPUT "${SHADER_PTX}"
    COMMAND "${NVCC_EXE}"
            -ptx -std=c++17
            ${NVCC_XCOMPILER}
            -I"${CMAKE_CURRENT_SOURCE_DIR}"
            -I"${OWL_SDK_DIR}/include"
            -I"${OPTIX_SDK_DIR}/include"
            -I"${CUDA_TOOLKIT_ROOT}/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders.cu"
            -o "${SHADER_PTX}"
    DEPENDS
        shaders.cu
        shared.h
        spectral.h
        sampling.h
    VERBATIM
)

add_custom_target(generate_ptx DEPENDS "${SHADER_PTX}")
add_dependencies(OwlSpectralRenderer generate_ptx)

add_custom_command(TARGET OwlSpectralRenderer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SHADER_PTX}"
        "$<TARGET_FILE_DIR:OwlSpectralRenderer>"
    VERBATIM
)

if(WIN32)
    add_custom_command(TARGET OwlSpectralRenderer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${OWL_SDK_DIR}/lib/owl.dll"
            "$<TARGET_FILE_DIR:OwlSpectralRenderer>"
        VERBATIM)
endif()