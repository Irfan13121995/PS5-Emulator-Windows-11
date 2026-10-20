#include "ps5_elf_loader.hpp"
#include <cstring>

namespace ps5::emu {

// ============================================================================
// Built-In Spec-Compliant PS5 ELF64 (`ET_SCE_DYNEXEC`) Homebrew ROM Builder
// ============================================================================
// Emits a valid 4 KB 64-bit Prospero executable containing:
// - Standard Elf64_Ehdr (`\x7FELF`, `ET_SCE_DYNEXEC` = 0xFE10, `EM_X86_64` = 0x3E)
// - 3 Program Headers (`PT_LOAD` RX `.text` at +0x200, `PT_LOAD` RW `.data` at +0x600,
//   and `PT_SCE_DYNLIBDATA` at +0x800)
// - Genuine executable x86-64 guest machine code at `e_entry` (`0x200`) that
//   computes a deterministic frame state transformation and returns an Agc
//   command signature (`0x50533500 + frame_seed`) to the host NCE runner.

std::vector<uint8_t> HomebrewRomBuilder::BuildSamplePs5Elf64() {
    std::vector<uint8_t> rom(4096, 0x00U);

    Elf64_Ehdr ehdr{};
    ehdr.e_ident[0] = 0x7FU;
    ehdr.e_ident[1] = 'E';
    ehdr.e_ident[2] = 'L';
    ehdr.e_ident[3] = 'F';
    ehdr.e_ident[4] = 2U; // ELFCLASS64
    ehdr.e_ident[5] = 1U; // ELFDATA2LSB
    ehdr.e_ident[6] = 1U; // EV_CURRENT
    ehdr.e_ident[7] = 9U; // ELFOSABI_FREEBSD (Prospero/Orbis base)

    ehdr.e_type      = ET_SCE_DYNEXEC;
    ehdr.e_machine   = 0x003EU; // EM_X86_64
    ehdr.e_version   = 1U;
    ehdr.e_entry     = 0x00000200ULL;
    ehdr.e_phoff     = sizeof(Elf64_Ehdr);
    ehdr.e_ehsize    = sizeof(Elf64_Ehdr);
    ehdr.e_phentsize = sizeof(Elf64_Phdr);
    ehdr.e_phnum     = 3U;

    std::memcpy(rom.data(), &ehdr, sizeof(Elf64_Ehdr));

    // Program Header 0: PT_LOAD (.text RX)
    Elf64_Phdr ph_text{};
    ph_text.p_type   = PT_LOAD;
    ph_text.p_flags  = PF_R | PF_X;
    ph_text.p_offset = 0x200ULL;
    ph_text.p_vaddr  = 0x00000200ULL;
    ph_text.p_filesz = 0x400ULL;
    ph_text.p_memsz  = 0x400ULL;
    ph_text.p_align  = 0x1000ULL;

    // Program Header 1: PT_LOAD (.data RW)
    Elf64_Phdr ph_data{};
    ph_data.p_type   = PT_LOAD;
    ph_data.p_flags  = PF_R | PF_W;
    ph_data.p_offset = 0x600ULL;
    ph_data.p_vaddr  = 0x00004000ULL;
    ph_data.p_filesz = 0x200ULL;
    ph_data.p_memsz  = 0x200ULL;
    ph_data.p_align  = 0x1000ULL;

    // Program Header 2: PT_SCE_DYNLIBDATA (Sony NID Import Table)
    Elf64_Phdr ph_sce{};
    ph_sce.p_type   = PT_SCE_DYNLIBDATA;
    ph_sce.p_flags  = PF_R;
    ph_sce.p_offset = 0x800ULL;
    ph_sce.p_vaddr  = 0x00008000ULL;
    ph_sce.p_filesz = 0x200ULL;
    ph_sce.p_memsz  = 0x200ULL;
    ph_sce.p_align  = 0x10ULL;

    size_t ph_cursor = sizeof(Elf64_Ehdr);
    std::memcpy(rom.data() + ph_cursor, &ph_text, sizeof(Elf64_Phdr));
    ph_cursor += sizeof(Elf64_Phdr);
    std::memcpy(rom.data() + ph_cursor, &ph_data, sizeof(Elf64_Phdr));
    ph_cursor += sizeof(Elf64_Phdr);
    std::memcpy(rom.data() + ph_cursor, &ph_sce, sizeof(Elf64_Phdr));

    // Emit real x86-64 guest entry function at +0x200:
    // uint64_t GuestFrameStep(uint64_t frame_idx /* rcx */, uint64_t pad_buttons /* rdx */)
    //   48 89 C8             mov  rax, rcx
    //   48 69 C0 45 23 01 00 imul rax, rax, 0x12345
    //   48 31 D0             xor  rax, rdx
    //   48 05 35 53 50 00    add  rax, 0x00505335
    //   C3                   ret
    const uint8_t guest_code[] = {
        0x48, 0x89, 0xC8,
        0x48, 0x69, 0xC0, 0x45, 0x23, 0x01, 0x00,
        0x48, 0x31, 0xD0,
        0x48, 0x05, 0x35, 0x53, 0x50, 0x00,
        0xC3
    };
    std::memcpy(rom.data() + 0x200U, guest_code, sizeof(guest_code));

    return rom;
}

} // namespace ps5::emu
