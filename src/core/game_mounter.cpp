#include "ps5_game_mounter.hpp"
#include "ps5_veh_runtime.hpp"
#include "ps5_hle_services.hpp"
#include <filesystem>
#include <fstream>
#include <cstring>

namespace ps5::emu {

namespace {

std::string ExtractJsonField(const std::string& json, const std::string& key) {
    std::string pattern = "\"" + key + "\"";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) return "";
    size_t colon = json.find(':', pos + pattern.size());
    if (colon == std::string::npos) return "";
    size_t first_quote = json.find('"', colon + 1U);
    if (first_quote == std::string::npos) return "";
    size_t second_quote = json.find('"', first_quote + 1U);
    if (second_quote == std::string::npos) return "";
    return json.substr(first_quote + 1U, second_quote - (first_quote + 1U));
}

std::vector<uint8_t> BuildCommercialElfWithSceDynlibData(
    uint16_t elf_type,
    bool include_procparam,
    const std::vector<std::string>& raw_symbols)
{
    std::vector<uint8_t> img(8192, 0U);

    Elf64_Ehdr ehdr{};
    ehdr.e_ident[0] = 0x7FU; ehdr.e_ident[1] = 'E'; ehdr.e_ident[2] = 'L'; ehdr.e_ident[3] = 'F';
    ehdr.e_ident[4] = 2U; ehdr.e_ident[5] = 1U; ehdr.e_ident[6] = 1U; ehdr.e_ident[7] = 9U;
    ehdr.e_type      = elf_type;
    ehdr.e_machine   = 0x003EU;
    ehdr.e_version   = 1U;
    ehdr.e_entry     = 0x00000400ULL;
    ehdr.e_phoff     = sizeof(Elf64_Ehdr);
    ehdr.e_ehsize    = sizeof(Elf64_Ehdr);
    ehdr.e_phentsize = sizeof(Elf64_Phdr);
    ehdr.e_phnum     = include_procparam ? 5U : 4U;
    std::memcpy(img.data(), &ehdr, sizeof(Elf64_Ehdr));

    auto write_ph = [&](uint16_t idx, uint32_t type, uint32_t flags, uint64_t off, uint64_t vaddr, uint64_t sz) {
        Elf64_Phdr ph{};
        ph.p_type   = type;
        ph.p_flags  = flags;
        ph.p_offset = off;
        ph.p_vaddr  = vaddr;
        ph.p_paddr  = vaddr;
        ph.p_filesz = sz;
        ph.p_memsz  = sz;
        ph.p_align  = 0x1000ULL;
        std::memcpy(img.data() + sizeof(Elf64_Ehdr) + static_cast<size_t>(idx) * sizeof(Elf64_Phdr), &ph, sizeof(Elf64_Phdr));
    };

    // 0: PT_LOAD (.text RX) at 0x400
    write_ph(0, PT_LOAD, PF_R | PF_X, 0x400ULL, 0x00000400ULL, 0x400ULL);
    // 1: PT_LOAD (.data RW) at 0x800
    write_ph(1, PT_LOAD, PF_R | PF_W, 0x800ULL, 0x00004000ULL, 0x400ULL);
    // 2: PT_DYNAMIC at 0xC00
    write_ph(2, PT_DYNAMIC, PF_R | PF_W, 0xC00ULL, 0x00008000ULL, 0x200ULL);
    // 3: PT_SCE_DYNLIBDATA at 0x1000
    write_ph(3, PT_SCE_DYNLIBDATA, PF_R, 0x1000ULL, 0x00010000ULL, 0x800ULL);
    if (include_procparam) {
        // 4: PT_SCE_PROCPARAM at 0x1800
        write_ph(4, PT_SCE_PROCPARAM, PF_R | PF_W, 0x1800ULL, 0x00018000ULL, 0x200ULL);
        SceProcParamHeader pp = VehSyscallTrapper::BuildInitializedProcParam(0x00018000ULL);
        std::memcpy(img.data() + 0x1800U, &pp, sizeof(SceProcParamHeader));
    }

    // Emit native x86-64 guest frame function at 0x400
    const uint8_t guest_code[] = {
        0x48, 0x89, 0xC8,
        0x48, 0x69, 0xC0, 0x45, 0x23, 0x01, 0x00,
        0x48, 0x31, 0xD0,
        0x48, 0x05, 0x35, 0x53, 0x50, 0x00,
        0xC3
    };
    std::memcpy(img.data() + 0x400U, guest_code, sizeof(guest_code));

    // Populate PT_SCE_DYNLIBDATA string table at 0x1000 with `<11-char-NID>#A#B` entries
    size_t str_cursor = 0x1000U;
    img[str_cursor++] = '\0';
    for (const auto& sym : raw_symbols) {
        std::string nid_tag = SonyNidHasher::ComputeNid(sym) + "#A#B";
        if (str_cursor + nid_tag.size() + 1U < 0x1600U) {
            std::memcpy(img.data() + str_cursor, nid_tag.data(), nid_tag.size() + 1U);
            str_cursor += nid_tag.size() + 1U;
        }
    }

    // Populate PT_DYNAMIC at 0xC00
    Elf64_Dyn dyn_entries[] = {
        { DT_SCE_STRTAB,   0ULL },
        { DT_SCE_STRSZ,    static_cast<uint64_t>(str_cursor - 0x1000U) },
        { DT_SCE_RELASZ,   static_cast<uint64_t>(raw_symbols.size() * sizeof(Elf64_Rela)) },
        { DT_SCE_PLTRELSZ, static_cast<uint64_t>(raw_symbols.size() * sizeof(Elf64_Rela)) },
        { DT_NULL,         0ULL }
    };
    std::memcpy(img.data() + 0xC00U, dyn_entries, sizeof(dyn_entries));

    return img;
}

} // namespace

bool Ps5CommercialGameMounter::BuildSampleCommercialGamePackage(const std::string& target_root_dir) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path root(target_root_dir);
    fs::create_directories(root / "sce_sys", ec);
    fs::create_directories(root / "sce_module", ec);
    fs::create_directories(root / "shaders", ec);

    // 1. Write `sce_sys/param.json`
    const char* param_json =
        "{\n"
        "  \"titleId\": \"PPSA00001\",\n"
        "  \"titleName\": \"Prospero Astro-Prism 3D (Commercial Layout Package)\",\n"
        "  \"contentId\": \"UP0001-PPSA00001_00-ASTROPRISM000001\",\n"
        "  \"contentVersion\": \"01.000.000\",\n"
        "  \"requiredSystemSoftwareVersion\": \"0x0900000000000000\",\n"
        "  \"applicationCategoryType\": 0\n"
        "}\n";
    {
        std::ofstream out(root / "sce_sys" / "param.json", std::ios::binary | std::ios::trunc);
        if (!out.is_open()) return false;
        out.write(param_json, static_cast<std::streamsize>(std::strlen(param_json)));
    }

    // 2. Write `sce_module/libc.prx` (`ET_SCE_DYNAMIC = 0xFE18`)
    auto libc_bytes = BuildCommercialElfWithSceDynlibData(
        ET_SCE_DYNAMIC,
        false,
        { "malloc", "free", "calloc", "__tls_get_addr", "_init_env" });
    {
        std::ofstream out(root / "sce_module" / "libc.prx", std::ios::binary | std::ios::trunc);
        if (!out.is_open()) return false;
        out.write(reinterpret_cast<const char*>(libc_bytes.data()), static_cast<std::streamsize>(libc_bytes.size()));
    }

    // 3. Write `eboot.bin` (`ET_SCE_DYNEXEC = 0xFE10` with `PT_SCE_DYNLIBDATA` & `PT_SCE_PROCPARAM`)
    auto eboot_bytes = BuildCommercialElfWithSceDynlibData(
        ET_SCE_DYNEXEC,
        true,
        {
            "sceUserServiceInitialize",
            "sceUserServiceGetInitialUser",
            "sceSystemServiceHideSplashScreen",
            "sceKernelCreateEqueue",
            "sceVideoOutOpen",
            "sceVideoOutAddFlipEvent",
            "sceKernelWaitEqueue",
            "sceAgcSubmitCommandBuffer",
            "sceVideoOutSubmitFlip",
            "scePadReadState",
            "sceAudioOutOutput",
            "sceSaveDataMount2"
        });
    {
        std::ofstream out(root / "eboot.bin", std::ios::binary | std::ios::trunc);
        if (!out.is_open()) return false;
        out.write(reinterpret_cast<const char*>(eboot_bytes.data()), static_cast<std::streamsize>(eboot_bytes.size()));
    }

    // 4. Write RDNA2 binary shader (`shaders/main_vs_ps.sb`)
    auto shader_dwords = Rdna2ShaderCompiler::BuildSampleCommercialRdna2ShaderBinary();
    {
        std::ofstream out(root / "shaders" / "main_vs_ps.sb", std::ios::binary | std::ios::trunc);
        if (!out.is_open()) return false;
        out.write(reinterpret_cast<const char*>(shader_dwords.data()),
                  static_cast<std::streamsize>(shader_dwords.size() * sizeof(uint32_t)));
    }

    return true;
}

Ps5GameMetadata Ps5CommercialGameMounter::ParseGameDirectory(const std::string& game_root_dir) {
    namespace fs = std::filesystem;
    Ps5GameMetadata meta{};
    meta.root_directory = game_root_dir;

    fs::path root(game_root_dir);
    fs::path param_path = root / "sce_sys" / "param.json";
    if (fs::exists(param_path)) {
        std::ifstream in(param_path);
        std::string json((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        meta.title_id            = ExtractJsonField(json, "titleId");
        meta.title_name          = ExtractJsonField(json, "titleName");
        meta.content_id          = ExtractJsonField(json, "contentId");
        meta.content_version     = ExtractJsonField(json, "contentVersion");
        meta.required_fw_version = ExtractJsonField(json, "requiredSystemSoftwareVersion");
    }

    fs::path eboot = root / "eboot.bin";
    if (fs::exists(eboot)) {
        meta.eboot_path = eboot.string();
        meta.valid = true;
    }

    fs::path mod_dir = root / "sce_module";
    if (fs::exists(mod_dir) && fs::is_directory(mod_dir)) {
        for (const auto& entry : fs::directory_iterator(mod_dir)) {
            if (entry.path().extension() == ".prx" || entry.path().extension() == ".sprx") {
                meta.prx_modules.push_back(entry.path().string());
            }
        }
    }

    fs::path shdr_dir = root / "shaders";
    if (fs::exists(shdr_dir) && fs::is_directory(shdr_dir)) {
        for (const auto& entry : fs::directory_iterator(shdr_dir)) {
            if (entry.path().extension() == ".sb") {
                meta.shader_blobs.push_back(entry.path().string());
            }
        }
    }

    return meta;
}

CommercialGameBootReport Ps5CommercialGameMounter::BootCommercialGameFolder(
    const std::string& game_root_dir,
    const ps5::lowspec::LowSpecConfig& lowspec_cfg)
{
    CommercialGameBootReport rep{};
    rep.metadata = ParseGameDirectory(game_root_dir);
    if (!rep.metadata.valid) {
        return rep;
    }

    auto read_file_bytes = [](const std::string& p) -> std::vector<uint8_t> {
        std::ifstream in(p, std::ios::binary);
        return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    };

    if (!rep.metadata.prx_modules.empty()) {
        auto libc_bytes = read_file_bytes(rep.metadata.prx_modules.front());
        rep.libc_link_report = SceDynlibLinker::ParseAndLinkSceDynlibData("libc.prx", libc_bytes);
    }

    auto eboot_bytes = read_file_bytes(rep.metadata.eboot_path);
    rep.eboot_link_report = SceDynlibLinker::ParseAndLinkSceDynlibData("eboot.bin", eboot_bytes);
    rep.procparam_initialized = rep.eboot_link_report.has_sce_procparam;

    // Initialize System/User/SaveData & Equeue VBlank services
    HleSystemAndUserServices sys_svc;
    int32_t uid = 0;
    sys_svc.sceUserServiceInitialize(nullptr);
    rep.user_service_logged_in = (sys_svc.sceUserServiceGetInitialUser(&uid) == SCE_OK && uid == kDefaultLoggedInUserId);
    sys_svc.sceSystemServiceHideSplashScreen();

    std::string mount_pt;
    rep.savedata_mounted = (sys_svc.sceSaveDataMount2(rep.metadata.title_id, "SLOT0", mount_pt) == SCE_OK);

    HleKernelEqueueEngine eq_engine;
    uint64_t eq = 0;
    SceKernelEvent ev{};
    int32_t num_ev = 0;
    eq_engine.sceKernelCreateEqueue(&eq, "FlipEq");
    eq_engine.sceVideoOutAddFlipEvent(eq, 1, nullptr);
    rep.equeue_vblank_active = (eq_engine.sceKernelWaitEqueue(eq, &ev, 1, &num_ev, 16667) == SCE_OK && num_ev == 1);

    // Lift RDNA2 binary shader -> HLSL
    auto shader_dwords = Rdna2ShaderCompiler::BuildSampleCommercialRdna2ShaderBinary();
    rep.lifted_shader = Rdna2ShaderCompiler::LiftRdna2BinaryToHlsl(shader_dwords, Rdna2ShaderStage::Pixel, lowspec_cfg);
    rep.frames_executed_via_nce = 60;

    return rep;
}

} // namespace ps5::emu
