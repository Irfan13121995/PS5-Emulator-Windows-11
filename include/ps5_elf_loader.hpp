#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <unordered_map>

namespace ps5::emu {

// ============================================================================
// 1. ELF64 & Sony Prospero (PS5) Binary Format Constants
// ============================================================================

constexpr uint32_t kElfMagic          = 0x464C457FU; // "\x7FELF" little-endian
constexpr uint32_t kProsperoSelfMagic = 0x1D3D154FU; // PS5 Prospero SELF magic
constexpr uint32_t kOrbisSelfMagic    = 0x5414F5EEU; // PS4/PS5 legacy SELF magic

// Sony Prospero e_type values
constexpr uint16_t ET_SCE_EXEC        = 0xFE00U;
constexpr uint16_t ET_SCE_REPLAY_EXEC = 0xFE01U;
constexpr uint16_t ET_SCE_DYNEXEC     = 0xFE10U; // Main eboot.bin executable
constexpr uint16_t ET_SCE_DYNAMIC     = 0xFE18U; // .prx / .sprx shared module

// Sony Prospero p_type segment types
constexpr uint32_t PT_LOAD            = 0x00000001U;
constexpr uint32_t PT_DYNAMIC         = 0x00000002U;
constexpr uint32_t PT_TLS             = 0x00000007U;
constexpr uint32_t PT_SCE_RELA        = 0x60000000U;
constexpr uint32_t PT_SCE_DYNLIBDATA  = 0x61000000U;
constexpr uint32_t PT_SCE_PROCPARAM   = 0x61000001U;
constexpr uint32_t PT_SCE_MODULE_PARAM= 0x61000002U;
constexpr uint32_t PT_SCE_RELRO       = 0x61000010U;

// Segment permission flags
constexpr uint32_t PF_X               = 0x1U;
constexpr uint32_t PF_W               = 0x2U;
constexpr uint32_t PF_R               = 0x4U;

// x86-64 Relocation types
constexpr uint32_t R_X86_64_NONE      = 0U;
constexpr uint32_t R_X86_64_64        = 1U;
constexpr uint32_t R_X86_64_GLOB_DAT  = 6U;
constexpr uint32_t R_X86_64_JUMP_SLOT = 7U;
constexpr uint32_t R_X86_64_RELATIVE  = 8U;

#pragma pack(push, 1)
struct Elf64_Ehdr {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

struct Elf64_Phdr {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
};

struct Elf64_Rela {
    uint64_t r_offset;
    uint64_t r_info;
    int64_t  r_addend;
};

struct SelfHeader {
    uint32_t magic;
    uint8_t  version;
    uint8_t  mode;
    uint8_t  endian;
    uint8_t  attribs;
    uint32_t key_type;
    uint16_t header_size;
    uint16_t meta_size;
    uint64_t file_size;
    uint16_t num_entries;
    uint16_t flags;
    uint32_t reserved;
};
#pragma pack(pop)

// ============================================================================
// 2. Sony NID Hashing & Symbol Resolution
// ============================================================================

struct NidSymbolBinding {
    std::string symbol_name;
    std::string nid_hash;
    std::string module_name;
    uint64_t host_handler_addr = 0;
    bool is_stub = false;
};

class SonyNidHasher {
public:
    // Computes official 11-character Sony NID:
    // Base64_SCE( SHA1(symbol_name + 518D64A635DED8C1E6B039B1C3E55230_hex)[0..7] )
    static std::string ComputeNid(const std::string& symbol_name);
};

// ============================================================================
// 3. ELF64 / Decrypted SELF Image Loader & Linker
// ============================================================================

struct LoadedSegmentInfo {
    uint32_t type = 0;
    uint32_t flags = 0;
    uint64_t guest_vaddr = 0;
    uint64_t file_size = 0;
    uint64_t mem_size = 0;
};

struct LoadedElfModule {
    bool valid = false;
    bool was_self_wrapped = false;
    uint16_t elf_type = 0;
    uint64_t image_base = 0;
    uint64_t entry_point = 0;
    uint32_t relocations_applied = 0;
    std::vector<LoadedSegmentInfo> segments;
    std::vector<NidSymbolBinding> resolved_imports;
    std::string error_message;
};

class Ps5ElfLoader {
public:
    static bool StripSelfWrapperIfPresent(
        const std::vector<uint8_t>& raw_file,
        std::vector<uint8_t>& out_elf_bytes,
        bool& out_was_self);

    static LoadedElfModule LoadAndRelocateElfImage(
        const std::vector<uint8_t>& raw_file_bytes,
        uint64_t preferred_base_addr,
        const std::vector<std::string>& imported_symbol_names);
};

// ============================================================================
// 4. Built-In Spec-Compliant PS5 ELF64 Homebrew Test ROM Builder
// ============================================================================

class HomebrewRomBuilder {
public:
    // Generates a valid 64-bit Prospero `ET_SCE_DYNEXEC` ELF binary containing
    // real x86-64 guest instructions and Sony NID symbol imports.
    static std::vector<uint8_t> BuildSamplePs5Elf64();
};

} // namespace ps5::emu
