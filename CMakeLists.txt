cmake_minimum_required(VERSION 3.24)
project(OwlSpectralRenderer LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

add_compile_options($<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=/Zc:preprocessor>)

set(OWL_SDK_DIR "D:/OWL_SDK")

add_executable(OwlSpectralRenderer 
    main.cpp 
    shared.h
    mat4.h
    sampling.h
    scene_ball.h
    camera.h
    toBMP.h
)

target_include_directories(OwlSpectralRenderer PRIVATE 
    "${OWL_SDK_DIR}/include" 
    "C:/ProgramData/NVIDIA Corporation/OptiX SDK 9.0.0/include"
    "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v13.3/include"
)

target_link_directories(OwlSpectralRenderer PRIVATE 
    "${OWL_SDK_DIR}/lib"
    "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v13.3/lib/x64"
)
target_link_libraries(OwlSpectralRenderer PRIVATE owl.lib cuda.lib cudart.lib)

# 将 shaders.cu 编译为 shaders.ptx
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/shaders.ptx
    COMMAND "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v13.3/bin/nvcc.exe" 
            -ptx -std=c++17 
            -Xcompiler /Zc:preprocessor
            -I"${CMAKE_CURRENT_SOURCE_DIR}" 
            -I"${OWL_SDK_DIR}/include" 
            -I"C:/ProgramData/NVIDIA Corporation/OptiX SDK 9.0.0/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders.cu" 
            -o "${CMAKE_CURRENT_BINARY_DIR}/shaders.ptx"
    DEPENDS shaders.cu
)

add_custom_target(generate_ptx ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/shaders.ptx)
add_dependencies(OwlSpectralRenderer generate_ptx)

add_custom_command(TARGET OwlSpectralRenderer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${OWL_SDK_DIR}/lib/owl.dll"
    $<TARGET_FILE_DIR:OwlSpectralRenderer>
    
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_BINARY_DIR}/shaders.ptx"
    $<TARGET_FILE_DIR:OwlSpectralRenderer>
)