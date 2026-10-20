#pragma once

#include "ps5_elf_loader.hpp"
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace ps5::emu {

// ============================================================================
// Sony Prospero `PT_DYNAMIC` (`DT_SCE_*`) Tags for `PT_SCE_DYNLIBDATA`
// ============================================================================
constexpr int64_t DT_NULL              = 0;
constexpr int64_t DT_NEEDED            = 1;
constexpr int64_t DT_INIT              = 12;
constexpr int64_t DT_FINI              = 13;
constexpr int64_t DT_SCE_IDTABENTSZ    = 0x61000005LL;
constexpr int64_t DT_SCE_FINGERPRINT   = 0x61000007LL;
constexpr int64_t DT_SCE_ORIGINAL_FILENAME = 0x61000009LL;
constexpr int64_t DT_SCE_MODULE_INFO   = 0x6100000DLL;
constexpr int64_t DT_SCE_NEEDED_MODULE = 0x6100000FLL;
constexpr int64_t DT_SCE_MODULE_ATTR   = 0x61000011LL;
constexpr int64_t DT_SCE_EXPORT_LIB    = 0x61000013LL;
constexpr int64_t DT_SCE_IMPORT_LIB    = 0x61000015LL;
constexpr int64_t DT_SCE_JMPREL        = 0x61000029LL;
constexpr int64_t DT_SCE_PLTRELSZ      = 0x6100002DLL;
constexpr int64_t DT_SCE_RELA          = 0x6100002FLL;
constexpr int64_t DT_SCE_RELASZ        = 0x61000031LL;
constexpr int64_t DT_SCE_STRTAB        = 0x61000035LL;
constexpr int64_t DT_SCE_STRSZ         = 0x61000037LL;
constexpr int64_t DT_SCE_SYMTAB        = 0x61000039LL;
constexpr int64_t DT_SCE_SYMENT        = 0x6100003BLL;

#pragma pack(push, 1)
struct Elf64_Dyn {
    int64_t  d_tag;
    uint64_t d_un;
};

struct Elf64_Sym {
    uint32_t st_name;
    uint8_t  st_info;
    uint8_t  st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
};
#pragma pack(pop)

struct DecodedSceSymbol {
    std::string raw_string;      // e.g. "4+h12345678#A#B"
    std::string nid;             // 11-char NID e.g. "4+h12345678"
    std::string library_id;      // "A"
    std::string module_id;       // "B"
    bool is_valid_nid_format = false;
};

struct DynlibLinkReport {
    std::string module_name;
    bool has_sce_dynlibdata = false;
    bool has_sce_procparam = false;
    uint32_t dynamic_tags_parsed = 0;
    uint32_t nid_symbols_decoded = 0;
    uint32_t rela_entries_patched = 0;
    uint32_t plt_entries_patched = 0;
    std::vector<DecodedSceSymbol> symbols;
};

class SceDynlibLinker {
public:
    static DecodedSceSymbol DecodeSceNidSymbol(const std::string& raw_sym);
    static DynlibLinkReport ParseAndLinkSceDynlibData(
        const std::string& module_name,
        const std::vector<uint8_t>& elf_bytes);
};

} // namespace ps5::emu
