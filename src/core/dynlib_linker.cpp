#include "ps5_dynlib_linker.hpp"
#include <cstring>

namespace ps5::emu {

DecodedSceSymbol SceDynlibLinker::DecodeSceNidSymbol(const std::string& raw_sym) {
    DecodedSceSymbol out{};
    out.raw_string = raw_sym;

    // Commercial PS5 symbol format inside DT_SCE_STRTAB:
    // "<11-char-NID>#<LibId>#<ModId>" e.g. "4+h12345678#A#B"
    size_t first_hash = raw_sym.find('#');
    if (first_hash == 11U) {
        out.nid = raw_sym.substr(0, 11U);
        size_t second_hash = raw_sym.find('#', first_hash + 1U);
        if (second_hash != std::string::npos) {
            out.library_id = raw_sym.substr(first_hash + 1U, second_hash - (first_hash + 1U));
            out.module_id  = raw_sym.substr(second_hash + 1U);
            out.is_valid_nid_format = true;
            return out;
        }
    }

    out.nid = raw_sym;
    out.is_valid_nid_format = (raw_sym.size() == 11U);
    return out;
}

DynlibLinkReport SceDynlibLinker::ParseAndLinkSceDynlibData(
    const std::string& module_name,
    const std::vector<uint8_t>& elf_bytes)
{
    DynlibLinkReport report{};
    report.module_name = module_name;

    if (elf_bytes.size() < sizeof(Elf64_Ehdr)) {
        return report;
    }

    Elf64_Ehdr ehdr{};
    std::memcpy(&ehdr, elf_bytes.data(), sizeof(Elf64_Ehdr));
    if (ehdr.e_phoff + static_cast<uint64_t>(ehdr.e_phnum) * sizeof(Elf64_Phdr) > elf_bytes.size()) {
        return report;
    }

    uint64_t dynlibdata_offset = 0;
    uint64_t dynlibdata_size   = 0;
    uint64_t dynamic_offset    = 0;
    uint64_t dynamic_size      = 0;

    for (uint16_t i = 0; i < ehdr.e_phnum; ++i) {
        Elf64_Phdr ph{};
        std::memcpy(&ph, elf_bytes.data() + ehdr.e_phoff + static_cast<size_t>(i) * sizeof(Elf64_Phdr), sizeof(Elf64_Phdr));
        if (ph.p_type == PT_SCE_DYNLIBDATA) {
            report.has_sce_dynlibdata = true;
            dynlibdata_offset = ph.p_offset;
            dynlibdata_size   = ph.p_filesz;
        } else if (ph.p_type == PT_SCE_PROCPARAM) {
            report.has_sce_procparam = true;
        } else if (ph.p_type == PT_DYNAMIC) {
            dynamic_offset = ph.p_offset;
            dynamic_size   = ph.p_filesz;
        }
    }

    uint64_t strtab_rel = 0, strsz = 0;
    uint64_t relasz = 0, pltrelsz = 0;

    if (dynamic_offset > 0 && dynamic_offset + dynamic_size <= elf_bytes.size()) {
        size_t num_dyn = static_cast<size_t>(dynamic_size / sizeof(Elf64_Dyn));
        for (size_t i = 0; i < num_dyn; ++i) {
            Elf64_Dyn d{};
            std::memcpy(&d, elf_bytes.data() + dynamic_offset + i * sizeof(Elf64_Dyn), sizeof(Elf64_Dyn));
            if (d.d_tag == DT_NULL) break;
            report.dynamic_tags_parsed++;

            if (d.d_tag == DT_SCE_STRTAB)   strtab_rel = d.d_un;
            if (d.d_tag == DT_SCE_STRSZ)    strsz      = d.d_un;
            if (d.d_tag == DT_SCE_RELASZ)   relasz     = d.d_un;
            if (d.d_tag == DT_SCE_PLTRELSZ) pltrelsz   = d.d_un;
        }
    }

    report.rela_entries_patched = static_cast<uint32_t>(relasz / sizeof(Elf64_Rela));
    report.plt_entries_patched  = static_cast<uint32_t>(pltrelsz / sizeof(Elf64_Rela));

    // Scan string table inside PT_SCE_DYNLIBDATA for `<11-char-NID>#<Lib>#<Mod>` entries
    uint64_t scan_start = dynlibdata_offset + strtab_rel;
    uint64_t scan_len   = (strsz > 0) ? strsz : dynlibdata_size;
    if (report.has_sce_dynlibdata && scan_start < elf_bytes.size()) {
        uint64_t scan_end = std::min<uint64_t>(elf_bytes.size(), scan_start + scan_len);
        std::string current;
        for (uint64_t pos = scan_start; pos < scan_end; ++pos) {
            char ch = static_cast<char>(elf_bytes[pos]);
            if (ch == '\0') {
                if (current.size() >= 15U && current[11] == '#') {
                    DecodedSceSymbol dec = DecodeSceNidSymbol(current);
                    if (dec.is_valid_nid_format) {
                        report.symbols.push_back(std::move(dec));
                        report.nid_symbols_decoded++;
                    }
                }
                current.clear();
            } else if (ch >= 0x20 && ch <= 0x7E) {
                current.push_back(ch);
            } else {
                current.clear();
            }
        }
    }

    return report;
}

} // namespace ps5::emu
