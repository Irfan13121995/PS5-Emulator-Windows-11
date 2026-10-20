# Security, Privacy & Integrity Audit Log (`v1.0.1`)

This document records the comprehensive low-level security audit and hardening patches applied to **`PS5-Emulator-Windows-11`** (`v1.0.1`).

---

## Remediated Findings Matrix

| ID | Severity | Category | File Reference | Vulnerability Summary | Status |
| :--- | :---: | :--- | :--- | :--- | :---: |
| **SEC-01** | **Critical** | Memory Safety & Concurrency | [`src/core/sparse_memory.cpp`](../src/core/sparse_memory.cpp) | Added `std::mutex` synchronization across `TouchGuestPage`, `EvictAndCompressColdPages`, and `GetStats`; enforced 64 KB guard pages (`[kPageSize, kGuestVirtualSpaceSize - kPageSize)`) and `ram_commit_budget_bytes` ceiling. | **FIXED** |
| **SEC-02** | **High** | Binary Parsing / NCE | [`src/core/loader_self.cpp`](../src/core/loader_self.cpp) | Restricted TLS inline patching to verified 9-byte canonical `mov rax, fs:[disp32]` sequences (`64 48 8B 04 25 <disp32>`) with `disp32 <= 0x0FFF` (4 KB guest TCB boundary). | **FIXED** |
| **SEC-03** | **High** | Virtual Memory (`W^X`) | [`src/host_runner/Program.cs`](../src/host_runner/Program.cs) | Added explicit `VirtualProtect(..., PAGE_EXECUTE_READ)` + `FlushInstructionCache` transitions (`W^X` enforcement) and reserved a 64 KB `PAGE_NOACCESS` guard page at the base of the sparse arena. | **FIXED** |
| **SEC-04** | **Medium** | Input Validation / UB | [`src/gpu/shader_downgrader.cpp`](../src/gpu/shader_downgrader.cpp) | Clamped texture dimensions (`1..16384`), mip levels (`1..15`), and `texture_mip_drop_levels` (`0..4`) to eliminate unsigned integer underflow (`mip_levels == 0`) and bit-shift Undefined Behavior (`>= 64` bits). | **FIXED** |
| **SEC-05** | **Medium** | Resource Exhaustion (DoS) | [`src/audio/stereo_downmix.cpp`](../src/audio/stereo_downmix.cpp) | Clamped `num_voices <= 256` and `sample_frames <= 4096` with early return on `0` to prevent unbounded heap allocation and division-by-zero. | **FIXED** |
| **SEC-06** | **Medium** | Win32 Interop / DLL Hijacking | [`src/host_runner/Program.cs`](../src/host_runner/Program.cs) | Added `[assembly: DefaultDllImportSearchPaths(DllImportSearchPath.System32)]` and startup call to `SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32)`. | **FIXED** |
| **SEC-07** | **Medium** | Build Hardening | [`CMakeLists.txt`](../CMakeLists.txt) | Enabled `/sdl`, `/GS`, `/guard:cf`, `/guard:ehcont`, `/Qspectre`, `/DYNAMICBASE`, `/HIGHENTROPYVA`, `/NXCOMPAT`, `/CETCOMPAT` (MSVC) and `-fstack-protector-strong`, `-D_FORTIFY_SOURCE=3`, `-fcf-protection=full` (GCC/Clang). | **FIXED** |
| **SEC-08** | **Low** | Privacy & File Integrity | [`src/host_runner/Program.cs`](../src/host_runner/Program.cs), [`src/ui/low_spec_config.cpp`](../src/ui/low_spec_config.cpp) | Stripped CR/LF characters from WMI hardware strings before writing `config.ini`, blocked symlink/reparse point overwrites (`FileAttributes.ReparsePoint`), and redacted local user paths in benchmark logs. | **FIXED** |
