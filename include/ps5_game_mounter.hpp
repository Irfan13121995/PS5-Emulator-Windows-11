#pragma once

#include "ps5_dynlib_linker.hpp"
#include "ps5_rdna2_compiler.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace ps5::emu {

// ============================================================================
// Commercial PS5 Game Directory (`PPSAxxxxx-app0`) Mounter & `param.json` Parser
// ============================================================================

struct Ps5GameMetadata {
    bool valid = false;
    std::string title_id;           // e.g. "PPSA00001"
    std::string title_name;         // e.g. "Prospero Astro-Prism 3D"
    std::string content_id;         // e.g. "UP0001-PPSA00001_00-ASTROPRISM000001"
    std::string content_version;    // e.g. "01.000.000"
    std::string required_fw_version;// e.g. "0x0900000000000000"
    std::string root_directory;
    std::string eboot_path;
    std::vector<std::string> prx_modules;
    std::vector<std::string> shader_blobs;
};

struct CommercialGameBootReport {
    Ps5GameMetadata metadata;
    DynlibLinkReport libc_link_report;
    DynlibLinkReport eboot_link_report;
    TranslatedShaderArtifact lifted_shader;
    bool procparam_initialized = false;
    bool equeue_vblank_active = false;
    bool user_service_logged_in = false;
    bool savedata_mounted = false;
    uint32_t frames_executed_via_nce = 0;
};

class Ps5CommercialGameMounter {
public:
    static bool BuildSampleCommercialGamePackage(const std::string& target_root_dir);
    static Ps5GameMetadata ParseGameDirectory(const std::string& game_root_dir);
    static CommercialGameBootReport BootCommercialGameFolder(
        const std::string& game_root_dir,
        const ps5::lowspec::LowSpecConfig& lowspec_cfg);
};

} // namespace ps5::emu
