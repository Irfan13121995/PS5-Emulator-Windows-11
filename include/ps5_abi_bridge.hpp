#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

namespace ps5::emu {

// ============================================================================
// 1. Guest Thread Control Block (TCB) for `fs:` Segment Access
// ============================================================================
// FreeBSD 11 / Prospero ABI places a self-pointer at `fs:[0x00]`, dynamic TLS
// vector at `fs:[0x08]`, and Stack Protector Canary at `fs:[0x28]`.

#pragma pack(push, 1)
struct GuestThreadControlBlock {
    uint64_t tcb_self_ptr;       // +0x00: Points to this struct in guest/host memory
    uint64_t dtv_ptr;            // +0x08: Dynamic Thread Vector pointer
    uint64_t pthread_ptr;        // +0x10: Guest ScePthread descriptor pointer
    uint64_t reserved_18;        // +0x18
    uint64_t reserved_20;        // +0x20
    uint64_t stack_canary;       // +0x28: Stack guard cookie (0x5053355F43414E59)
    uint32_t thread_id;          // +0x30: Guest thread ID
    uint32_t errno_val;          // +0x34: Per-thread guest errno
};
#pragma pack(pop)

// ============================================================================
// 2. System V AMD64 -> Windows x64 ABI Machine-Code Thunk Emitter
// ============================================================================
// Translates guest calls using the FreeBSD/Prospero System V AMD64 calling
// convention (`rdi, rsi, rdx, rcx, r8, r9`) to the Microsoft x64 calling
// convention (`rcx, rdx, r8, r9` + 32-byte shadow space):
//
//   mov  r10, rcx          ; Preserve 4th SysV arg (rcx)
//   mov  rcx, rdi          ; 1st Win64 arg <- 1st SysV arg (rdi)
//   mov  rdx, rsi          ; 2nd Win64 arg <- 2nd SysV arg (rsi)
//   mov  r8,  rdx_orig     ; 3rd Win64 arg <- 3rd SysV arg (rdx)
//   mov  r9,  r10          ; 4th Win64 arg <- 4th SysV arg (rcx)
//   sub  rsp, 0x28         ; 32-byte Win64 shadow space + 16-byte stack align
//   movabs rax, <host_fn>
//   call rax
//   add  rsp, 0x28
//   ret

class AbiTrampolineEmitter {
public:
    static std::vector<uint8_t> EmitSysVToWin64Thunk(uint64_t target_host_fn_addr);
    static GuestThreadControlBlock CreateInitializedGuestTcb(uint64_t self_address, uint32_t tid);
};

} // namespace ps5::emu
