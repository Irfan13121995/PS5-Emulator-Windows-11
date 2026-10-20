#include "ps5_abi_bridge.hpp"
#include <cstring>

namespace ps5::emu {

std::vector<uint8_t> AbiTrampolineEmitter::EmitSysVToWin64Thunk(uint64_t target_host_fn_addr) {
    // 32-byte x86-64 machine-code bridge:
    //   49 89 CA                         mov  r10, rcx       ; Save 4th SysV arg
    //   48 89 F9                         mov  rcx, rdi       ; 1st Win64 <- rdi
    //   49 89 D0                         mov  r8,  rdx       ; 3rd Win64 <- rdx
    //   48 89 F2                         mov  rdx, rsi       ; 2nd Win64 <- rsi
    //   4D 89 D1                         mov  r9,  r10       ; 4th Win64 <- saved rcx
    //   48 83 EC 28                      sub  rsp, 0x28      ; Shadow space + alignment
    //   48 B8 <8-byte target_host_fn>    movabs rax, target
    //   FF D0                            call rax
    //   48 83 C4 28                      add  rsp, 0x28
    //   C3                               ret
    std::vector<uint8_t> code = {
        0x49, 0x89, 0xCA,
        0x48, 0x89, 0xF9,
        0x49, 0x89, 0xD0,
        0x48, 0x89, 0xF2,
        0x4D, 0x89, 0xD1,
        0x48, 0x83, 0xEC, 0x28,
        0x48, 0xB8,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xFF, 0xD0,
        0x48, 0x83, 0xC4, 0x28,
        0xC3
    };
    std::memcpy(&code[21], &target_host_fn_addr, sizeof(uint64_t));
    return code;
}

GuestThreadControlBlock AbiTrampolineEmitter::CreateInitializedGuestTcb(
    uint64_t self_address,
    uint32_t tid)
{
    GuestThreadControlBlock tcb{};
    tcb.tcb_self_ptr = self_address;
    tcb.dtv_ptr      = self_address + 0x40ULL;
    tcb.pthread_ptr  = self_address + 0x80ULL;
    tcb.stack_canary = 0x5053355F43414E59ULL; // "PS5_CANY"
    tcb.thread_id    = tid;
    tcb.errno_val    = 0;
    return tcb;
}

} // namespace ps5::emu
