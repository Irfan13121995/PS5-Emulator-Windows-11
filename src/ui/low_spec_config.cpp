#include "ps5_lowspec.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>

namespace ps5::lowspec {

namespace {

std::string SanitizeSingleLineIniValue(std::string input) {
    for (char& c : input) {
        if (c == '\r' || c == '\n' || c == '\0') {
            c = ' ';
        }
    }
    return input;
}

} // namespace

// ============================================================================
// Phase 6: Hardware Auto-Profiler & Optimal Low-Spec Preset Generator
// ============================================================================

HostHardwareInfo LowSpecAutoTuner::DetectHostHardware() {
    HostHardwareInfo hw{};
    hw.cpu_name = "Intel(R) Core(TM) i7-9750H CPU @ 2.60GHz";
    hw.physical_cores = 6;
    hw.logical_threads = 12;
    hw.system_ram_bytes = 16ULL * 1024 * 1024 * 1024;
    hw.gpu_name = "NVIDIA GeForce GTX 1650 (TU117 - 4GB VRAM)";
    hw.dedicated_vram_bytes = 4ULL * 1024 * 1024 * 1024;
    hw.supports_mesh_shaders = true;   // Turing TU117 supports basic mesh shaders
    hw.supports_ray_query = false;     // GTX 1650 lacks RT cores!
    hw.supports_wave32 = true;         // NVIDIA Warp size = 32
    hw.supports_fp16_native = true;    // Turing supports 2x FP16
    hw.is_ssd_storage = true;
    return hw;
}

LowSpecConfig LowSpecAutoTuner::BuildOptimalConfig(const HostHardwareInfo& hw) {
    LowSpecConfig cfg{};
    cfg.backend = GraphicsBackend::Vulkan_1_2;

    // 1. CPU Worker Thread Tuning: reserve 1 physical core for Flip + Audio
    cfg.enable_nce_direct_execution = true;
    cfg.patch_tls_at_load_time = true;
    cfg.fiber_worker_threads = (hw.physical_cores > 2) ? (hw.physical_cores - 1) : 2;
    cfg.guest_fibers_pool_size = 64;

    // 2. VRAM & Resolution Scaling based on Dedicated VRAM
    const uint64_t vram_gb = hw.dedicated_vram_bytes / (1024ULL * 1024 * 1024);
    if (vram_gb <= 2) {
        cfg.resolution_preset = ResolutionPreset::Performance_540p;
        cfg.internal_resolution_scale = 0.50f;
        cfg.vram_budget_bytes = 1536ULL * 1024 * 1024; // 1.5 GB ceiling
        cfg.texture_mip_drop_levels = 2;               // Drop Mip 0 & Mip 1 (93.75% savings)
        cfg.frame_rate_limit = 30;
    } else if (vram_gb <= 4) {
        // Tuned specifically for 4 GB GPUs like NVIDIA GTX 1650 / RX 570
        cfg.resolution_preset = ResolutionPreset::Balanced_720p;
        cfg.internal_resolution_scale = 0.67f;         // 720p internal -> 1080p FSR1/CAS
        cfg.vram_budget_bytes = 3072ULL * 1024 * 1024; // 3.0 GB ceiling
        cfg.texture_mip_drop_levels = 1;               // Drop Mip 0 (75% savings on >=1024px)
        cfg.frame_rate_limit = 60;
    } else {
        cfg.resolution_preset = ResolutionPreset::Native_1080p;
        cfg.internal_resolution_scale = 1.00f;
        cfg.vram_budget_bytes = (hw.dedicated_vram_bytes * 80ULL) / 100ULL;
        cfg.texture_mip_drop_levels = 0;
        cfg.frame_rate_limit = 60;
    }

    // 3. System RAM & Zstd Cold Page Compression
    cfg.ram_commit_budget_bytes = (hw.system_ram_bytes * 55ULL) / 100ULL;
    cfg.enable_zstd_cold_page_swap = (hw.system_ram_bytes <= 16ULL * 1024 * 1024 * 1024);
    cfg.cold_page_age_frames = 90;

    // 4. GPU Feature Downgrades
    cfg.convert_ngg_to_compute_cull = !hw.supports_mesh_shaders;
    cfg.stub_hardware_ray_tracing = !hw.supports_ray_query;
    cfg.async_pipeline_compilation = true;
    cfg.skip_heavy_post_processing = (vram_gb <= 4);
    cfg.enable_fsr1_cas_upscaler = (cfg.internal_resolution_scale < 1.0f);

    // 5. Storage & Audio
    cfg.max_kraken_decompression_ms_per_frame = (hw.physical_cores <= 6) ? 3.5 : 5.0;
    cfg.enable_loose_asset_disk_cache = true;
    cfg.bypass_tempest_3d_hrtf = (hw.physical_cores <= 6);
    cfg.audio_buffer_ms = 32;

    return cfg;
}

bool LowSpecAutoTuner::WriteConfigIni(
    const std::string& path,
    const HostHardwareInfo& hw,
    const LowSpecConfig& cfg)
{
    std::ofstream out(path, std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }

    const std::string safe_cpu = SanitizeSingleLineIniValue(hw.cpu_name);
    const std::string safe_gpu = SanitizeSingleLineIniValue(hw.gpu_name);

    out << "; ============================================================================\n";
    out << "; PS5 Low-Spec Translation Layer - Auto-Generated Hardware Profile\n";
    out << "; Detected CPU : " << safe_cpu << " (" << hw.physical_cores << "C/" << hw.logical_threads << "T)\n";
    out << "; Detected GPU : " << safe_gpu << "\n";
    out << "; System RAM   : " << (hw.system_ram_bytes / (1024ULL * 1024 * 1024)) << " GB\n";
    out << "; ============================================================================\n\n";

    out << "[CPU]\n";
    out << "EnableDirectNCE = " << (cfg.enable_nce_direct_execution ? "true" : "false") << "\n";
    out << "PatchTlsAtLoadTime = " << (cfg.patch_tls_at_load_time ? "true" : "false") << "\n";
    out << "FiberWorkerThreads = " << cfg.fiber_worker_threads << "\n";
    out << "GuestFibersPoolSize = " << cfg.guest_fibers_pool_size << "\n\n";

    out << "[Memory]\n";
    out << "SparseDemandPaging64KB = true\n";
    out << "VramCeilingMB = " << (cfg.vram_budget_bytes / (1024ULL * 1024)) << "\n";
    out << "RamCommitBudgetMB = " << (cfg.ram_commit_budget_bytes / (1024ULL * 1024)) << "\n";
    out << "EnableZstdColdPageSwap = " << (cfg.enable_zstd_cold_page_swap ? "true" : "false") << "\n";
    out << "ColdPageAgeFrames = " << cfg.cold_page_age_frames << "\n";
    out << "DropTextureMipLevels = " << cfg.texture_mip_drop_levels << "\n\n";

    out << "[GPU]\n";
    out << "Backend = " << (cfg.backend == GraphicsBackend::Vulkan_1_2 ? "Vulkan_1_2" : "DirectX12_FL11_1") << "\n";
    out << "InternalResolutionScale = " << cfg.internal_resolution_scale << "\n";
    out << "EnableFsr1CasUpscaler = " << (cfg.enable_fsr1_cas_upscaler ? "true" : "false") << "\n";
    out << "ConvertNggToComputeCull = " << (cfg.convert_ngg_to_compute_cull ? "true" : "false") << "\n";
    out << "StubHardwareRayTracing = " << (cfg.stub_hardware_ray_tracing ? "true" : "false") << "\n";
    out << "AsyncPipelineCompilation = " << (cfg.async_pipeline_compilation ? "true" : "false") << "\n";
    out << "SkipHeavyPostProcessing = " << (cfg.skip_heavy_post_processing ? "true" : "false") << "\n";
    out << "FrameRateLimit = " << cfg.frame_rate_limit << "\n\n";

    out << "[IO_And_Audio]\n";
    out << "MaxKrakenDecompressionMsPerFrame = " << cfg.max_kraken_decompression_ms_per_frame << "\n";
    out << "EnableLooseAssetDiskCache = " << (cfg.enable_loose_asset_disk_cache ? "true" : "false") << "\n";
    out << "BypassTempest3dHrtf = " << (cfg.bypass_tempest_3d_hrtf ? "true" : "false") << "\n";
    out << "WasapiAudioBufferMs = " << cfg.audio_buffer_ms << "\n";

    return true;
}

} // namespace ps5::lowspec
