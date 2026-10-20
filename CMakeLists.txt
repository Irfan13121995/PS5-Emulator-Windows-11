cmake_minimum_required(VERSION 3.20)
project(PS5_LowSpec_Emulator VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(ps5_lowspec_core STATIC
    src/core/loader_self.cpp
    src/core/fiber_scheduler.cpp
    src/core/sparse_memory.cpp
    src/gpu/vk_backend.cpp
    src/gpu/dx12_backend.cpp
    src/gpu/shader_downgrader.cpp
    src/gpu/vram_scaler.cpp
    src/io/async_kraken.cpp
    src/audio/stereo_downmix.cpp
    src/ui/low_spec_config.cpp
)

target_include_directories(ps5_lowspec_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(MSVC)
    target_compile_options(ps5_lowspec_core PRIVATE /W4 /O2 /arch:AVX2)
else()
    target_compile_options(ps5_lowspec_core PRIVATE -Wall -Wextra -O3 -mavx2)
endif()
