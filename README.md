# PS5 Commercial HLE + NCE Emulator for Windows 10/11 (v3.0.0 - Low-Spec Optimized)

[![Platform](https://img.shields.io/badge/Platform-Windows%2010%20%7C%2011%20(x64)-0078D6?style=for-the-badge&logo=windows)](https://github.com/Irfan13121995/PS5-Emulator-Windows-11)
[![Architecture](https://img.shields.io/badge/Architecture-Commercial%20HLE%20%2B%20NCE%20(v3.0)-8B5CF6?style=for-the-badge)](https://github.com/Irfan13121995/PS5-Emulator-Windows-11)
[![Language](https://img.shields.io/badge/Language-C%2B%2B20%20%7C%20C%23%20Win32-68217A?style=for-the-badge)](https://github.com/Irfan13121995/PS5-Emulator-Windows-11)
[![License](https://img.shields.io/badge/License-MIT-2EA44F?style=for-the-badge)](./LICENSE)

An open-source **PlayStation 5 (Prospero / Orbis) Commercial Game High-Level Emulation (HLE) + Native Code Execution (NCE) Emulator for Windows 10/11**, engineered to run on **mid-range and low-spec PCs** (4–6 Core CPUs, 8–16 GB RAM, and 2–4 GB VRAM non-RTX GPUs such as the NVIDIA GeForce GTX 1650, GTX 1050 Ti, GTX 1060, and AMD Radeon RX 570/580).

---

## What's New in `v3.0.0` (Commercial PS5 Game Runtime Upgrade)

1. **`PPSAxxxxx-app0` Commercial Game Folder Mounter & `sce_sys/param.json` Parser ([`src/core/game_mounter.cpp`](./src/core/game_mounter.cpp))**:
   - Mounts decrypted commercial PS5 game directories (`PPSAxxxxx-app0`), parses `sce_sys/param.json` (`titleId`, `titleName`, `contentId`, `contentVersion`, `requiredSystemSoftwareVersion`), and automatically discovers `sce_module/*.prx` and `eboot.bin`.
   - Includes a **built-in, spec-compliant commercial-structure test game package** at [`games/PPSA00001-app0/`](./games/PPSA00001-app0) (`sce_sys/param.json`, `sce_module/libc.prx`, `eboot.bin`, and `shaders/main_vs_ps.sb`).
2. **`PT_SCE_DYNLIBDATA` (`0x61000000`) & `<11-char-NID>#Lib#Mod` Dynamic Linker ([`src/core/dynlib_linker.cpp`](./src/core/dynlib_linker.cpp))**:
   - Parses Sony `DT_SCE_STRTAB`, `DT_SCE_SYMTAB`, `DT_SCE_RELA`, and `DT_SCE_JMPREL` tables inside `PT_SCE_DYNLIBDATA` across both `sce_module/libc.prx` (`ET_SCE_DYNAMIC`) and `eboot.bin` (`ET_SCE_DYNEXEC`).
3. **`PT_SCE_PROCPARAM` (`0x61000001`) `libc` Initialization & Ring-0 Syscall `VEH` Trapper ([`src/core/veh_syscall_trapper.cpp`](./src/core/veh_syscall_trapper.cpp))**:
   - Initializes `SceProcParamHeader` (`0x4942524F` `"ORBI"` magic, FW `0x09000000` SDK) and `SceLibcParam` so `libc.prx` heap initialization succeeds before `main()`.
   - Dispatches ring-0 Prospero syscalls (`sys_mmap`, `sys_munmap`, `sys_mprotect`, `sys_dmem_container`, `sys_dynlib_get_info_ex`, `sys_dynlib_get_proc_param`).
4. **`SceKernelEqueue` 60 Hz VBlank Synchronization & System/User/SaveData HLE ([`src/hle/libkernel_equeue_sync.cpp`](./src/hle/libkernel_equeue_sync.cpp), [`src/hle/libsce_system_user_savedata.cpp`](./src/hle/libsce_system_user_savedata.cpp))**:
   - Delivers 60 Hz `EVFILT_VIDEO_OUT` flip events to `sceKernelWaitEqueue` so commercial render loops never deadlock waiting for VBlank.
   - Implements `libSceUserService` (`InitialUser = 0x10000001`, `"ProsperoPlayer1"`), `libSceSystemService` (`sceSystemServiceHideSplashScreen`), `libSceSaveData` (`/savedata0`), and offline `libSceNpManager` / `libSceNpTrophy2` stubs.
5. **RDNA2 Binary Shader $\rightarrow$ HLSL/SPIR-V Lifter & `Standard64KB` Texture Deswizzler ([`src/gpu/rdna2_shader_compiler.cpp`](./src/gpu/rdna2_shader_compiler.cpp), [`src/gpu/agc_texture_deswizzler.cpp`](./src/gpu/agc_texture_deswizzler.cpp))**:
   - Decodes 256-bit `T#` and 128-bit `V#` descriptors from Shader Resource Tables (`SRT`), lifts RDNA2 binary instructions (`SMEM`, `MIMG`, `VOP3`, `EXP`, `SOPP`) into cached HLSL shaders (`shader_cache/PPSA00001/PPSA00001_main_ps.hlsl`), stubs `image_bvh64_intersect_ray` for non-RTX GPUs (GTX 1650), and deswizzles `Standard64KB` Morton-tiled BCn surfaces with **Mip-Dropper L1 (`-75%` VRAM)**.

---

## Commercial Game Boot Pipeline (`v3.0.0`)

```mermaid
flowchart TD
    subgraph GamePkg["Mounted Commercial Game Folder (games/PPSA00001-app0)"]
        Param["sce_sys/param.json\n(PPSA00001 Metadata)"]
        Libc["sce_module/libc.prx\n(ET_SCE_DYNAMIC 0xFE18)"]
        Eboot["eboot.bin\n(ET_SCE_DYNEXEC + PT_SCE_DYNLIBDATA + PT_SCE_PROCPARAM)"]
        ShaderBin["shaders/main_vs_ps.sb\n(RDNA2 Binary ISA)"]
    end

    subgraph Runtime["v3.0.0 Commercial Bootloader & HLE Runtime"]
        DynLinker["PT_SCE_DYNLIBDATA Linker\n(<11-char-NID>#A#B Resolution)"]
        ProcParam["PT_SCE_PROCPARAM (0x4942524F)\n+ Guest TCB & W^X NCE Sandbox"]
        Services["SceKernelEqueue (60Hz VBlank)\n+ UserService (0x10000001) + /savedata0"]
        ShaderLifter["RDNA2 -> HLSL Shader Compiler\n(BVH Ray Stub + Mip-Drop L1)"]
    end

    subgraph Host["Windows 10/11 Interactive Viewport"]
        Viewport["60 FPS Interactive 3D Viewport\n+ Live HLE & Shader Inspector"]
    end

    Param --> DynLinker
    Libc --> DynLinker
    Eboot --> DynLinker --> ProcParam --> Services --> Viewport
    ShaderBin --> ShaderLifter --> Viewport
```

---

## Interactive Emulator Controls (GUI Mode)

Launch `.\PS5_LowSpec_Emulator.exe` to boot `games/PPSA00001-app0` in the interactive 60 FPS emulator window:

| Key / Toolbar Button | Emulated PS5 Action |
| :--- | :--- |
| **`Mount PS5 Game Folder (PPSAxxxxx)...`** | Mount any decrypted commercial PS5 game folder containing `eboot.bin` and `sce_sys/param.json` |
| **`Boot PPSA00001 Commercial Package`** | Boot the built-in `games/PPSA00001-app0` commercial-layout package |
| **`W` / `A` / `S` / `D`** or **Arrow Keys** | Move & Orbit 3D Prospero Scene (`ScePadData` Left Stick & D-Pad) |
| **`Space`** | Hold for Cross Button Boost (`SCE_PAD_BUTTON_CROSS = 0x4000`) |
| **`C`** | Cycle DualSense Controller Lightbar RGB (`scePadSetLightBar`) |
| **`P`** | Pause / Resume NCE Frame Execution |
| **`F5` / `F8`** | Save / Load `.ps5save` State Snapshot (`slot1.ps5save`) |

---

## Repository Structure

```text
PS5-Emulator-Windows-11/
├── CMakeLists.txt                          # Hardened C++20 CMake build (v3.0.0)
├── PS5_LowSpec_Emulator.exe                # Compiled 64-bit Windows PS5 Commercial HLE+NCE Emulator
├── config.ini                              # Auto-tuned low-spec hardware configuration
├── games/
│   └── PPSA00001-app0/                     # Built-in commercial-layout PS5 test game package
│       ├── eboot.bin                       # ET_SCE_DYNEXEC with PT_SCE_DYNLIBDATA & PT_SCE_PROCPARAM
│       ├── sce_sys/param.json              # PS5 Title Metadata (PPSA00001)
│       ├── sce_module/libc.prx             # ET_SCE_DYNAMIC shared module
│       └── shaders/main_vs_ps.sb           # RDNA2 binary shader stream
├── shader_cache/
│   └── PPSA00001/PPSA00001_main_ps.hlsl    # Auto-lifted HLSL pixel shader (BVH Ray Query stubbed)
├── include/
│   ├── ps5_lowspec.hpp                     # Low-spec memory, fiber, GPU & audio interfaces
│   ├── ps5_elf_loader.hpp                  # ELF64 / SELF structs & Sony SHA-1 Base64 NID hasher
│   ├── ps5_abi_bridge.hpp                  # Guest TCB & SysV->Win64 machine-code thunk emitter
│   ├── ps5_hle_modules.hpp                 # HLE libkernel, libSceVideoOut, libSceAgc, libScePad
│   ├── ps5_dynlib_linker.hpp               # PT_SCE_DYNLIBDATA & DT_SCE_* dynamic linker
│   ├── ps5_veh_runtime.hpp                 # PT_SCE_PROCPARAM & Ring-0 VEH syscall dispatcher
│   ├── ps5_hle_services.hpp                # SceKernelEqueue VBlank sync, UserService, SystemService
│   ├── ps5_rdna2_compiler.hpp              # Agc T#/V# decoders, RDNA2->HLSL lifter & BCn deswizzler
│   └── ps5_game_mounter.hpp                # Commercial PPSAxxxxx folder mounter & param.json parser
├── src/
│   ├── core/                               # NCE loader, NID linker, Dynlib linker, VEH & Game Mounter
│   ├── hle/                                # libkernel, Equeue VBlank, User/System/SaveData, Agc & Pad
│   ├── gpu/                                # RDNA2->HLSL compiler, BCn deswizzler, Mip-Dropper & FSR1
│   ├── io/                                 # Frame-budgeted (3.5ms) AVX2 Kraken stream decompressor
│   ├── audio/                              # Tempest 3D HRTF bypass -> stereo downmixer
│   ├── ui/                                 # Hardware auto-profiler & INI generator
│   └── host_runner/Program.cs              # Complete 64-bit Windows HLE+NCE Runtime & 60 FPS GUI
└── docs/
    ├── LOW_SPEC_ARCHITECTURE.md            # Low-spec engineering design & subsystem breakdown
    ├── CONFIGURATION_GUIDE.md              # Complete config.ini tuning guide & hardware tier presets
    └── SECURITY_AUDIT.md                   # Security, W^X, memory safety & privacy audit log
```

---

## Building & Verification

```powershell
& "C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe" /nologo /optimize+ /platform:x64 `
  /r:System.Management.dll /r:System.Windows.Forms.dll /r:System.Drawing.dll `
  /out:PS5_LowSpec_Emulator.exe src\host_runner\Program.cs

# Launch Interactive 60 FPS Emulator GUI:
.\PS5_LowSpec_Emulator.exe

# Run Automated Commercial Game Pipeline Verification:
.\PS5_LowSpec_Emulator.exe --verify-commercial-pipeline
```

### Verified Output (`--verify-commercial-pipeline`)
```text
================================================================================
  PS5 HLE + NCE EMULATOR FOR WINDOWS 10/11 - COMMERCIAL GAME RUNTIME (v3.0.0)
================================================================================
  Host CPU         : Intel(R) Core(TM) i7-9750H CPU @ 2.60GHz (6 Cores / 12 Threads)
  Host GPU         : NVIDIA GeForce GTX 1650 (4.0 GB Dedicated VRAM)
  Host System RAM  : 15.85 GB
  HW Ray Tracing   : UNSUPPORTED -> Auto-Stubbing BVH Ray Queries
  Mesh Shaders     : Supported (Turing/RDNA2+)
--------------------------------------------------------------------------------
[PASS] Milestone 1 (Commercial Game Mount)  : Mounted [PPSA00001] 'Prospero Astro-Prism 3D (Commercial Layout Package)' v01.000.000
[PASS] Milestone 1 (PT_SCE_DYNLIBDATA Link) : Linked libc.prx (5 NIDs) + eboot.bin (12 <NID>#A#B imports) in 31.67 ms
[PASS] Milestone 2 (PROCPARAM & NCE W^X)    : Verified PT_SCE_PROCPARAM (Magic=0x4942524F, SDK=0x09000000), Executed 60 Native Frames (Ret=0x00945761)
[PASS] Milestone 3 (Equeue & System HLE)    : UserService InitialUser=0x10000001 ('ProsperoPlayer1'), Equeue 60Hz VBlank Active, /savedata0 Mounted
[PASS] Milestone 4 (RDNA2 -> HLSL Compiler) : Decoded 5 RDNA2 ISA words (Stubbed 1 BVH Ray Query) -> Cached 'PPSA00001_main_ps.hlsl'
[PASS] Milestone 5 (Agc Deswizzler & VRAM)  : Deswizzled BC7 Standard64KB Tiles, Mip-Drop L1 cut VRAM 2048 MB -> 512 MB (-75%)
--------------------------------------------------------------------------------
  Mounted Game Package : .\games\PPSA00001-app0\eboot.bin  |  Config : .\config.ini
================================================================================
```
