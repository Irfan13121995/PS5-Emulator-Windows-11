#include "ps5_veh_runtime.hpp"

namespace ps5::emu {

SceProcParamHeader VehSyscallTrapper::BuildInitializedProcParam(uint64_t guest_param_base_addr) {
    SceProcParamHeader hdr{};
    hdr.size                  = sizeof(SceProcParamHeader);
    hdr.magic                 = kSceProcParamMagic; // "ORBI" (0x4942524F)
    hdr.entry_count           = 4;
    hdr.sdk_version           = kProsperoSdkVer900; // PS5 SDK 9.00
    hdr.process_name_ptr      = guest_param_base_addr + 0x100ULL;
    hdr.main_thread_name_ptr  = guest_param_base_addr + 0x140ULL;
    hdr.main_thread_prio_ptr  = guest_param_base_addr + 0x180ULL;
    hdr.main_thread_stack_ptr = guest_param_base_addr + 0x190ULL;
    hdr.libc_param_ptr        = guest_param_base_addr + 0x200ULL;
    hdr.mem_param_ptr         = guest_param_base_addr + 0x280ULL;
    hdr.fs_param_ptr          = guest_param_base_addr + 0x300ULL;
    return hdr;
}

SyscallTrapResult VehSyscallTrapper::DispatchRing0Syscall(
    uint64_t rax_sysno,
    uint64_t rdi,
    uint64_t rsi,
    uint64_t /*rdx*/,
    uint64_t /*r10*/,
    uint64_t /*r8*/,
    uint64_t /*r9*/)
{
    SyscallTrapResult res{};
    res.syscall_number = rax_sysno;
    res.instruction_bytes_advanced = 2; // Advance past `0F 05`

    switch (rax_sysno) {
        case SYS_getpid:
            res.syscall_name = "sys_getpid";
            res.return_rax = 0x1042;
            break;
        case SYS_thr_self:
            res.syscall_name = "sys_thr_self";
            res.return_rax = 0;
            break;
        case SYS_sigprocmask:
            res.syscall_name = "sys_sigprocmask";
            res.return_rax = 0;
            break;
        case SYS_mmap:
            res.syscall_name = "sys_mmap";
            res.return_rax = static_cast<int64_t>(rdi != 0 ? rdi : 0x0000000400000000ULL);
            break;
        case SYS_munmap:
            res.syscall_name = "sys_munmap";
            res.return_rax = 0;
            break;
        case SYS_mprotect:
            res.syscall_name = "sys_mprotect";
            res.return_rax = 0;
            break;
        case SYS_namedobj_create:
            res.syscall_name = "sys_namedobj_create";
            res.return_rax = 0x200;
            break;
        case SYS_dmem_container:
            res.syscall_name = "sys_dmem_container";
            res.return_rax = 0;
            break;
        case SYS_dynlib_get_proc_param:
            res.syscall_name = "sys_dynlib_get_proc_param";
            res.return_rax = 0;
            break;
        case SYS_dynlib_get_info_ex:
            res.syscall_name = "sys_dynlib_get_info_ex";
            res.return_rax = 0;
            break;
        case SYS_dynlib_dlsym:
            res.syscall_name = "sys_dynlib_dlsym";
            res.return_rax = static_cast<int64_t>(rsi);
            break;
        default:
            res.syscall_name = "sys_prospero_generic_ok";
            res.return_rax = 0;
            break;
    }

    return res;
}

} // namespace ps5::emu
