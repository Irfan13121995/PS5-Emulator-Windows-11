cmake_minimum_required(VERSION 3.20)
project(PS5_LowSpec_Emulator VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(ps5_lowspec_core STATIC
    # Low-Spec Optimization Subsystems
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

    # Full PS5 HLE + NCE Emulator Subsystems
    src/core/nid_hash.cpp
    src/core/elf64_loader.cpp
    src/core/abi_trampoline.cpp
    src/core/homebrew_rom_builder.cpp
    src/hle/libkernel.cpp
    src/hle/libsce_videoout_agc.cpp
    src/hle/libsce_pad_audio.cpp
)

target_include_directories(ps5_lowspec_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Hardened Compiler & Linker Exploit Mitigations (ASLR, DEP, CFG, CET Shadow Stack)
if(MSVC)
    target_compile_options(ps5_lowspec_core PRIVATE
        /W4 /O2 /arch:AVX2
        /sdl                # Security Development Lifecycle checks
        /GS                 # Stack buffer security cookie
        /guard:cf           # Control Flow Guard (CFG)
        /guard:ehcont       # CET Exception Handling Continuation metadata
        /Qspectre           # Spectre variant 1 bounds mitigation
    )
    target_link_options(ps5_lowspec_core INTERFACE
        /DYNAMICBASE        # Address Space Layout Randomization (ASLR)
        /HIGHENTROPYVA      # 64-bit High-Entropy ASLR
        /NXCOMPAT           # Data Execution Prevention (DEP / W^X)
        /GUARD:CF           # Linker Control Flow Guard table
        /CETCOMPAT          # Hardware-enforced Shadow Stack (Intel CET / AMD Zen 3+)
    )
else()
    target_compile_options(ps5_lowspec_core PRIVATE
        -Wall -Wextra -O3 -mavx2
        -fstack-protector-strong
        -D_FORTIFY_SOURCE=3
        -fcf-protection=full
        -fPIE
    )
endif()
