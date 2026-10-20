#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace ps5::emu {

// ============================================================================
// 1. `PT_SCE_PROCPARAM` (`SceProcParam` & `SceLibcParam`) Structures
// ============================================================================
// Commercial PS5 `eboot.bin` binaries read `PT_SCE_PROCPARAM` (`0x61000001`)
// inside `_start` -> `_init_env` before `main()` is invoked.

constexpr uint32_t kSceProcParamMagic = 0x4942524FU; // "ORBI" / Prospero ABI magic
constexpr uint32_t kProsperoSdkVer900 = 0x09000000U; // PS5 Firmware 9.00 SDK

#pragma pack(push, 1)
struct SceLibcParam {
    uint64_t size;                  // 0x00: sizeof(SceLibcParam) = 0x40
    uint32_t magic;                 // 0x08: 0x00000001
    uint32_t pad0;
    uint64_t heap_size_ptr;         // 0x10: guest ptr to uint64_t heap size
    uint64_t default_heap_size;     // 0x18: e.g. 256 MB
    uint64_t malloc_replace_ptr;    // 0x20: 0 (use default)
    uint64_t new_replace_ptr;       // 0x28: 0
    uint64_t need_sce_libc;         // 0x30: 1
    uint64_t reserved;              // 0x38: 0
};

struct SceProcParamHeader {
    uint64_t size;                  // 0x00: sizeof(SceProcParamHeader) = 0x50
    uint32_t magic;                 // 0x08: kSceProcParamMagic ("ORBI")
    uint32_t entry_count;           // 0x0C: 4
    uint64_t sdk_version;           // 0x10: 0x09000000
    uint64_t process_name_ptr;      // 0x18
    uint64_t main_thread_name_ptr;  // 0x20
    uint64_t main_thread_prio_ptr;  // 0x28
    uint64_t main_thread_stack_ptr; // 0x30
    uint64_t libc_param_ptr;        // 0x38: Points to SceLibcParam
    uint64_t mem_param_ptr;         // 0x40: Points to SceKernelMemParam
    uint64_t fs_param_ptr;          // 0x48
};
#pragma pack(pop)

// ============================================================================
// 2. Prospero FreeBSD-11 Ring-0 Syscall Numbers & VEH Trap Dispatcher
// ============================================================================
constexpr uint64_t SYS_read                 = 3;
constexpr uint64_t SYS_write                = 4;
constexpr uint64_t SYS_open                 = 5;
constexpr uint64_t SYS_close                = 6;
constexpr uint64_t SYS_getpid               = 20;
constexpr uint64_t SYS_munmap               = 73;
constexpr uint64_t SYS_mprotect             = 74;
constexpr uint64_t SYS_sigprocmask          = 340;
constexpr uint64_t SYS_thr_self             = 432;
constexpr uint64_t SYS_mmap                 = 477;
constexpr uint64_t SYS_namedobj_create      = 557;
constexpr uint64_t SYS_dmem_container       = 586;
constexpr uint64_t SYS_dynlib_dlsym         = 591;
constexpr uint64_t SYS_dynlib_get_list      = 592;
constexpr uint64_t SYS_dynlib_get_info_ex   = 608;
constexpr uint64_t SYS_dynlib_get_proc_param= 598;

struct SyscallTrapResult {
    uint64_t syscall_number = 0;
    std::string syscall_name;
    int64_t return_rax = 0;
    bool carry_flag_error = false;
    uint32_t instruction_bytes_advanced = 2; // `0F 05` is 2 bytes
};

class VehSyscallTrapper {
public:
    static SceProcParamHeader BuildInitializedProcParam(uint64_t guest_param_base_addr);
    static SyscallTrapResult DispatchRing0Syscall(
        uint64_t rax_sysno,
        uint64_t rdi,
        uint64_t rsi,
        uint64_t rdx,
        uint64_t r10,
        uint64_t r8,
        uint64_t r9);
};

} // namespace ps5::emu
