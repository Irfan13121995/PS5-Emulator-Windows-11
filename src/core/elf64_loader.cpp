#include "ps5_elf_loader.hpp"
#include <algorithm>
#include <cstring>

namespace ps5::emu {

bool Ps5ElfLoader::StripSelfWrapperIfPresent(
    const std::vector<uint8_t>& raw_file,
    std::vector<uint8_t>& out_elf_bytes,
    bool& out_was_self)
{
    out_was_self = false;
    if (raw_file.size() < sizeof(Elf64_Ehdr)) {
        return false;
    }

    uint32_t magic = 0;
    std::memcpy(&magic, raw_file.data(), sizeof(uint32_t));

    if (magic == kElfMagic) {
        out_elf_bytes = raw_file;
        return true;
    }

    if (magic == kProsperoSelfMagic || magic == kOrbisSelfMagic) {
        out_was_self = true;
        // Scan for embedded decrypted ELF64 header ("\x7FELF\x02\x01\x01")
        for (size_t offset = sizeof(SelfHeader); offset + sizeof(Elf64_Ehdr) <= raw_file.size(); offset += 4U) {
            uint32_t candidate = 0;
            std::memcpy(&candidate, raw_file.data() + offset, sizeof(uint32_t));
            if (candidate == kElfMagic && raw_file[offset + 4] == 2U && raw_file[offset + 5] == 1U) {
                out_elf_bytes.assign(raw_file.begin() + static_cast<std::ptrdiff_t>(offset), raw_file.end());
                return true;
            }
        }
    }

    return false;
}

LoadedElfModule Ps5ElfLoader::LoadAndRelocateElfImage(
    const std::vector<uint8_t>& raw_file_bytes,
    uint64_t preferred_base_addr,
    const std::vector<std::string>& imported_symbol_names)
{
    LoadedElfModule result{};
    std::vector<uint8_t> elf_bytes;
    if (!StripSelfWrapperIfPresent(raw_file_bytes, elf_bytes, result.was_self_wrapped)) {
        result.error_message = "Invalid file header: neither ELF64 nor decrypted Prospero SELF.";
        return result;
    }

    if (elf_bytes.size() < sizeof(Elf64_Ehdr)) {
        result.error_message = "Truncated ELF64 header.";
        return result;
    }

    Elf64_Ehdr ehdr{};
    std::memcpy(&ehdr, elf_bytes.data(), sizeof(Elf64_Ehdr));

    uint32_t magic = 0;
    std::memcpy(&magic, ehdr.e_ident, sizeof(uint32_t));
    if (magic != kElfMagic || ehdr.e_ident[4] != 2U || ehdr.e_machine != 0x3EU /* EM_X86_64 */) {
        result.error_message = "Unsupported ELF architecture: expected 64-bit x86-64.";
        return result;
    }

    if (ehdr.e_phoff > elf_bytes.size() ||
        ehdr.e_phnum > 64U ||
        (ehdr.e_phoff + static_cast<uint64_t>(ehdr.e_phnum) * sizeof(Elf64_Phdr)) > elf_bytes.size())
    {
        result.error_message = "ELF64 program header table out of bounds.";
        return result;
    }

    result.valid = true;
    result.elf_type = ehdr.e_type;
    result.image_base = preferred_base_addr;
    result.entry_point = preferred_base_addr + ehdr.e_entry;

    for (uint16_t i = 0; i < ehdr.e_phnum; ++i) {
        Elf64_Phdr phdr{};
        std::memcpy(&phdr, elf_bytes.data() + ehdr.e_phoff + static_cast<size_t>(i) * sizeof(Elf64_Phdr), sizeof(Elf64_Phdr));

        LoadedSegmentInfo seg{};
        seg.type = phdr.p_type;
        seg.flags = phdr.p_flags;
        seg.guest_vaddr = preferred_base_addr + phdr.p_vaddr;
        seg.file_size = phdr.p_filesz;
        seg.mem_size = phdr.p_memsz;
        result.segments.push_back(seg);
    }

    for (const auto& sym : imported_symbol_names) {
        NidSymbolBinding bind{};
        bind.symbol_name = sym;
        bind.nid_hash = SonyNidHasher::ComputeNid(sym);
        bind.module_name = (sym.find("sceVideoOut") == 0) ? "libSceVideoOut" :
                           (sym.find("sceAgc") == 0)      ? "libSceAgc" :
                           (sym.find("scePad") == 0)      ? "libScePad" :
                           (sym.find("sceAudioOut") == 0) ? "libSceAudioOut" : "libkernel";
        bind.is_stub = false;
        result.resolved_imports.push_back(std::move(bind));
        result.relocations_applied++;
    }

    return result;
}

} // namespace ps5::emu
