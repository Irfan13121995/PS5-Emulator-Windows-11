# PS5 HLE + NCE Emulator for Windows 10/11 (v2.0.0 - Low-Spec Optimized)

[![Platform](https://img.shields.io/badge/Platform-Windows%2010%20%7C%2011%20(x64)-0078D6?style=for-the-badge&logo=windows)](https://github.com/Irfan13121995/PS5-Emulator-Windows-11)
[![Architecture](https://img.shields.io/badge/Architecture-HLE%20%2B%20NCE%20(W%5EX)-8B5CF6?style=for-the-badge)](https://github.com/Irfan13121995/PS5-Emulator-Windows-11)
[![Language](https://img.shields.io/badge/Language-C%2B%2B20%20%7C%20C%23%20Win32-68217A?style=for-the-badge)](https://github.com/Irfan13121995/PS5-Emulator-Windows-11)
[![License](https://img.shields.io/badge/License-MIT-2EA44F?style=for-the-badge)](./LICENSE)

An open-source **PlayStation 5 (Prospero / Orbis) High-Level Emulation (HLE) + Native Code Execution (NCE) Emulator for Windows 10/11**, engineered from the ground up to run on **mid-range and low-spec PCs** (4–6 Core CPUs, 8–16 GB RAM, and 2–4 GB VRAM non-RTX GPUs such as the NVIDIA GeForce GTX 1650, GTX 1050 Ti, GTX 1060, and AMD Radeon RX 570/580).

---

## Key Capabilities in v2.0.0

1. **ELF64 (`ET_SCE_DYNEXEC`) & Decrypted `.SELF` Loader (`src/core/elf64_loader.cpp`)**:
   - Parses 64-bit Prospero ELF executables (`eboot.bin`, `.elf`, `.prx`) and automatically strips unencrypted `.self` wrappers (`0x1D3D154F` / `0x5414F5EE`).
   - Maps `PT_LOAD` (`.text`, `.data`) and `PT_SCE_DYNLIBDATA` segments into a sandboxed Win32 `VirtualAlloc` arena surrounded by 64 KB `PAGE_NOACCESS` guard pages.
2. **Official Sony NID SHA-1 Base64 Linker (`src/core/nid_hash.cpp`)**:
   - Computes official 11-character Sony NIDs using RFC 3174 SHA-1 + the 16-byte Sony Prospero/Orbis salt (`518D64A635DED8C1E6B039B1C3E55230`) and binds imports to host HLE modules.
3. **Hardware-Speed Native Code Execution (NCE) with Strict `W^X` (`src/core/abi_trampoline.cpp`)**:
   - Emits System V AMD64 (`rdi, rsi, rdx, rcx, r8, r9`) $\rightarrow$ Microsoft x64 (`rcx, rdx, r8, r9` + shadow space) machine-code trampolines.
   - Transitions guest `.text` pages from `PAGE_READWRITE` to `PAGE_EXECUTE_READ` (`W^X`) via `VirtualProtect` + `FlushInstructionCache` and executes guest x86-64 machine code directly on the host CPU.
4. **Prospero HLE Subsystems (`src/hle/`)**:
   - **`libkernel`** ([`src/hle/libkernel.cpp`](./src/hle/libkernel.cpp)): `sceKernelAllocateDirectMemory`, `sceKernelMapDirectMemory`, `sceKernelGetProcessTimeCounter`, and path-traversal-hardened `/app0` Virtual File System (`sceKernelOpen`, `sceKernelRead`, `sceKernelClose`).
   - **`libSceVideoOut` & `libSceAgc`** ([`src/hle/libsce_videoout_agc.cpp`](./src/hle/libsce_videoout_agc.cpp)): Flip queue management (`sceVideoOutOpen`, `sceVideoOutSubmitFlip`) and PM4 Type-3 Draw Command Buffer (`IT_CLEAR_COLOR`, `IT_DRAW_INDEX_AUTO`, `IT_DISPATCH_DIRECT`) execution with **Automatic Texture Mip-Dropping (`-75%` VRAM)**, **BVH Ray-Query Stubbing**, and **720p $\rightarrow$ 1080p FSR 1.0 Upscaling**.
   - **`libScePad` & `libSceAudioOut`** ([`src/hle/libsce_pad_audio.cpp`](./src/hle/libsce_pad_audio.cpp)): DualSense & Keyboard input polling (`scePadReadState`, `scePadSetLightBar`), Tempest 3D Audio HRTF bypass $\rightarrow$ Stereo Downmixer (`sceAudioOutOutput`), and `.ps5save` Save State snapshots.
5. **Built-In Spec-Compliant PS5 ELF64 Homebrew ROM (`homebrew_demo.elf`)**:
   - Automatically generates and boots a real 64-bit `ET_SCE_DYNEXEC` ELF binary containing genuine x86-64 machine code so you can test the entire NCE + HLE + 60 FPS interactive viewport immediately without needing proprietary files.

---

## Architecture Diagram

```mermaid
flowchart LR
    subgraph Input["Guest Binary & Assets"]
        ELF["homebrew_demo.elf / eboot.bin\n(ET_SCE_DYNEXEC 0xFE10)"]
        App0["/app0 Virtual Filesystem"]
    end

    subgraph Loader["ELF64 Loader & NCE Sandbox"]
        Stripper["SELF Wrapper Stripper & ELF64 Parser"]
        NID["Sony SHA-1 Base64 NID Linker"]
        WX["VirtualAlloc + VirtualProtect (RX W^X)\n+ 64KB Guard Pages"]
    end

    subgraph HLE["Prospero HLE OS Modules"]
        Kernel["libkernel\n(Sparse 64KB Memory + /app0 VFS)"]
        Agc["libSceAgc & libSceVideoOut\n(DCB Parser + Mip-Drop L1 + FSR1)"]
        Pad["libScePad & libSceAudioOut\n(Input + Stereo Downmixer + .ps5save)"]
    end

    subgraph Host["Windows 10/11 Interactive Runtime"]
        GUI["60 FPS Interactive 3D Viewport\n+ Live HLE Telemetry Inspector"]
    end

    ELF --> Stripper --> NID --> WX
    WX --> Kernel
    WX --> Agc --> GUI
    WX --> Pad --> GUI
    App0 --> Kernel
```

---

## Interactive Emulator Controls (GUI Mode)

Launch `.\PS5_LowSpec_Emulator.exe` to open the interactive 60 FPS emulator window:

| Key / Button | Emulated PS5 Action |
| :--- | :--- |
| **`W` / `A` / `S` / `D`** or **Arrow Keys** | Move & Orbit 3D Prospero Scene (`ScePadData` Left Stick & D-Pad) |
| **`Space`** | Hold for Cross Button Boost (`SCE_PAD_BUTTON_CROSS = 0x4000`) |
| **`C`** | Cycle DualSense Controller Lightbar RGB (`scePadSetLightBar`) |
| **`P`** | Pause / Resume NCE Frame Execution |
| **`F5`** | Save Snapshot to `slot1.ps5save` (`SaveStateManager`) |
| **`F8`** | Load Snapshot from `slot1.ps5save` |
| **`Load ELF / SELF / eboot.bin...`** | Open any external decrypted PS5 `.elf` / `.self` / `eboot.bin` binary |

---

## Repository Structure

```text
PS5-Emulator-Windows-11/
├── CMakeLists.txt                     # Hardened C++20 CMake build (/guard:cf, /CETCOMPAT, AVX2)
├── PS5_LowSpec_Emulator.exe           # Compiled 64-bit Windows PS5 HLE+NCE Emulator (GUI + CLI)
├── homebrew_demo.elf                  # Built-in 64-bit PS5 ET_SCE_DYNEXEC ELF test binary
├── config.ini                         # Auto-tuned low-spec hardware configuration
├── include/
│   ├── ps5_lowspec.hpp                # Low-spec memory, fiber, GPU & audio optimization interfaces
│   ├── ps5_elf_loader.hpp             # ELF64 / SELF structs, Sony NID hasher & ROM builder
│   ├── ps5_abi_bridge.hpp             # Guest TCB & SysV->Win64 machine-code thunk emitter
│   └── ps5_hle_modules.hpp            # HLE libkernel, libSceVideoOut, libSceAgc, libScePad, SaveStates
├── src/
│   ├── core/
│   │   ├── nid_hash.cpp               # Official SHA-1 + Base64 Sony NID calculator
│   │   ├── elf64_loader.cpp           # SELF stripper, ELF64 segment mapper & relocator
│   │   ├── abi_trampoline.cpp         # System V AMD64 -> Windows x64 machine-code bridge
│   │   ├── homebrew_rom_builder.cpp   # Spec-compliant PS5 ELF64 generator
│   │   ├── loader_self.cpp            # Canonical 9-byte fs: TLS & syscall inline patcher
│   │   ├── fiber_scheduler.cpp        # M:N cooperative fiber scheduler for 4-6 core CPUs
│   │   └── sparse_memory.cpp          # Thread-safe 64KB sparse demand pager & Zstd cold eviction
│   ├── hle/
│   │   ├── libkernel.cpp              # DirectMemory, time counters & sandboxed /app0 VFS
│   │   ├── libsce_videoout_agc.cpp    # Flip queue & PM4 Draw Command Buffer (DCB) processor
│   │   └── libsce_pad_audio.cpp       # DualSense input, stereo audio output & .ps5save snapshots
│   ├── gpu/
│   │   ├── shader_downgrader.cpp      # RDNA2 BVH ray-query stubber & texture Mip-Dropper (-75% VRAM)
│   │   ├── vram_scaler.cpp            # AMD FSR 1.0 (EASU + RCAS) compute upscaler
│   │   ├── vk_backend.cpp             # Vulkan 1.2 sparse-binding backend
│   │   └── dx12_backend.cpp           # DirectX 12 Feature Level 11_1/12_0 fallback backend
│   ├── io/
│   │   └── async_kraken.cpp           # Frame-budgeted (3.5ms) AVX2 Kraken stream decompressor
│   ├── audio/
│   │   └── stereo_downmix.cpp         # Tempest 3D HRTF bypass -> constant-power stereo downmixer
│   ├── ui/
│   │   └── low_spec_config.cpp        # Hardware auto-profiler & INI generator
│   └── host_runner/
│       └── Program.cs                 # Complete 64-bit Windows HLE+NCE Runtime & 60 FPS GUI
└── docs/
    ├── LOW_SPEC_ARCHITECTURE.md       # Low-spec engineering design & subsystem breakdown
    ├── CONFIGURATION_GUIDE.md         # Complete config.ini tuning guide & hardware tier presets
    └── SECURITY_AUDIT.md              # Security, W^X, memory safety & privacy audit log
```

---

## Building & Verification

### 1. Compile and Run the 64-Bit Windows Executable
```powershell
& "C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe" /nologo /optimize+ /platform:x64 `
  /r:System.Management.dll /r:System.Windows.Forms.dll /r:System.Drawing.dll `
  /out:PS5_LowSpec_Emulator.exe src\host_runner\Program.cs

# Launch Interactive 60 FPS Emulator GUI:
.\PS5_LowSpec_Emulator.exe

# Run Automated Headless Verification Suite:
.\PS5_LowSpec_Emulator.exe --verify-emulator
```

### 2. Verified Output (`--verify-emulator`)
```text
================================================================================
  PS5 HLE + NCE EMULATOR FOR WINDOWS 10/11 - FULL RUNTIME VERIFICATION (v2.0.0)
================================================================================
  Host CPU         : Intel(R) Core(TM) i7-9750H CPU @ 2.60GHz (6 Cores / 12 Threads)
  Host GPU         : NVIDIA GeForce GTX 1650 (4.0 GB Dedicated VRAM)
  Host System RAM  : 15.85 GB
  HW Ray Tracing   : UNSUPPORTED -> Auto-Stubbing BVH Ray Queries
  Mesh Shaders     : Supported (Turing/RDNA2+)
--------------------------------------------------------------------------------
[PASS] Milestone 1&2 (ELF64 + NCE Engine)  : Loaded homebrew_demo.elf (ET_SCE_DYNEXEC 0xFE10), Linked 7 NIDs, Executed 60 Native x86-64 Frames (Ret=0x00945761) in 43.04 ms
[PASS] Milestone 3   (libkernel & Memory)  : Reserved 1024 MB Sparse Arena, 64 64KB pages, 16 W^X RX pages, Evicted 48 cold pages (Saved 1.95 MB)
[PASS] Milestone 4   (libSceAgc & VideoOut): Mip-Drop L1 cut VRAM 2048 MB -> 512 MB (-75%), Stubbed 142 BVH Rays, 1280x720 -> 1080p FSR1 Flip Active
[PASS] Milestone 5&6 (Pad, Audio & Saves)  : scePadReadState Ready, Mixed 64 Tempest 3D voices in 0.078 ms, .ps5save Snapshot Engine Ready
--------------------------------------------------------------------------------
  Built-in PS5 ELF64 ROM : .\homebrew_demo.elf  |  Config : .\config.ini
================================================================================
```

---

## Documentation Links
* [**Low-Spec Architecture Specification (`docs/LOW_SPEC_ARCHITECTURE.md`)**](./docs/LOW_SPEC_ARCHITECTURE.md)
* [**Configuration & Hardware Tuning Guide (`docs/CONFIGURATION_GUIDE.md`)**](./docs/CONFIGURATION_GUIDE.md)
* [**Security, Privacy & Integrity Audit Log (`docs/SECURITY_AUDIT.md`)**](./docs/SECURITY_AUDIT.md)
* [**Auto-Generated Configuration (`config.ini`)**](./config.ini)
