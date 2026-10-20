#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <array>
#include <atomic>
#include <mutex>
#include <functional>

namespace ps5::lowspec {

// ============================================================================
// 1. Hardware Capability & Low-Spec Tuning Profile
// ============================================================================

struct HostHardwareInfo {
    std::string cpu_name;
    uint32_t physical_cores = 4;
    uint32_t logical_threads = 8;
    uint64_t system_ram_bytes = 8ULL * 1024 * 1024 * 1024;

    std::string gpu_name;
    uint64_t dedicated_vram_bytes = 4ULL * 1024 * 1024 * 1024;
    bool supports_mesh_shaders = false;     // VK_EXT_mesh_shader / DX12 Mesh Shaders
    bool supports_ray_query = false;        // VK_KHR_ray_query / DXR 1.1
    bool supports_wave32 = false;           // RDNA2 native Wave32 subgroup size
    bool supports_fp16_native = true;       // Float16 arithmetic (Turing TU117+ / Polaris)
    bool is_ssd_storage = true;
};

enum class GraphicsBackend : uint8_t {
    Vulkan_1_2 = 0,
    DirectX12_FL11_1 = 1
};

enum class ResolutionPreset : uint8_t {
    Performance_540p = 0,   // 0.50x scale + FSR1/CAS upscale to 1080p (2GB VRAM / iGPU)
    Balanced_720p    = 1,   // 0.67x scale + FSR1/CAS upscale to 1080p (4GB VRAM e.g. GTX 1650)
    Quality_900p     = 2,   // 0.83x scale + CAS sharpening
    Native_1080p     = 3    // 1.00x scale (6GB+ VRAM)
};

struct LowSpecConfig {
    GraphicsBackend backend = GraphicsBackend::Vulkan_1_2;
    ResolutionPreset resolution_preset = ResolutionPreset::Balanced_720p;
    float internal_resolution_scale = 0.67f;
    uint32_t frame_rate_limit = 60;

    // CPU & Threading (Tuned for 4C-6C CPUs vs PS5 8C/16T Zen 2)
    bool enable_nce_direct_execution = true;
    bool patch_tls_at_load_time = true;
    uint32_t fiber_worker_threads = 5;      // Leaves 1 physical core for Flip + Audio
    uint32_t guest_fibers_pool_size = 64;

    // Memory & VRAM (Tuned for 8-16GB RAM & 2-4GB VRAM vs PS5 16GB GDDR6)
    uint64_t vram_budget_bytes = 3072ULL * 1024 * 1024; // 3.0 GB ceiling on 4GB GPU
    uint64_t ram_commit_budget_bytes = 8192ULL * 1024 * 1024;
    bool enable_zstd_cold_page_swap = true;
    uint32_t cold_page_age_frames = 90;
    uint32_t texture_mip_drop_levels = 1;   // Drop Mip 0 (75% VRAM reduction on >=1024x1024)

    // Shader & GPU Downgrades (For GTX 10/16-series, RX 500, and iGPUs)
    bool convert_ngg_to_compute_cull = true;
    bool stub_hardware_ray_tracing = true;  // Essential for GTX 1650 / GTX 1060 / RX 580
    bool async_pipeline_compilation = true;
    bool skip_heavy_post_processing = true; // Bypass Bokeh DOF, Per-Object Motion Blur
    bool enable_fsr1_cas_upscaler = true;

    // I/O & Audio
    double max_kraken_decompression_ms_per_frame = 3.5;
    bool enable_loose_asset_disk_cache = true;
    bool bypass_tempest_3d_hrtf = true;     // Use SIMD constant-power stereo downmix
    uint32_t audio_buffer_ms = 32;
};

// ============================================================================
// 2. Subsystem Interfaces
// ============================================================================

// Phase 1: SELF Loader & SysV -> Win64 ABI Patcher
struct SelfPatchReport {
    std::string module_name;
    size_t code_size_bytes = 0;
    uint32_t abi_trampolines_installed = 0;
    uint32_t tls_fs_instructions_patched = 0;
    uint32_t avx_instructions_verified = 0;
};

class SelfLoader {
public:
    static SelfPatchReport AnalyzeAndPatchModule(
        const std::string& module_name,
        uint8_t* code_buffer,
        size_t code_size,
        bool patch_tls_inline);
};

// Phase 1b: M:N Cooperative Fiber Scheduler
struct FiberStats {
    uint32_t host_worker_threads = 0;
    uint32_t active_guest_fibers = 0;
    uint64_t total_fiber_switches = 0;
    uint64_t coalesced_spinlocks = 0;
    double avg_switch_overhead_ns = 0.0;
};

class CooperativeFiberScheduler {
public:
    explicit CooperativeFiberScheduler(uint32_t host_core_count);
    ~CooperativeFiberScheduler();

    uint32_t SpawnGuestPthread(std::function<void()> entry_point, bool high_priority = false);
    void YieldCurrentGuestThread();
    FiberStats RunFrameQuantumSimulation(uint32_t guest_thread_count, uint32_t iterations);

private:
    uint32_t worker_threads_;
    std::atomic<uint64_t> switch_counter_{0};
    std::atomic<uint64_t> spinlock_yields_{0};
};

// Phase 2: Sparse 64KB Demand-Paged Unified Memory Manager
struct MemoryStats {
    uint64_t virtual_reserved_bytes = 0;
    uint64_t physical_committed_bytes = 0;
    uint64_t vram_resident_bytes = 0;
    uint64_t compressed_cold_pages = 0;
    uint64_t bytes_saved_by_compression = 0;
    double compression_ratio = 1.0;
};

class SparseUnifiedMemory {
public:
    static constexpr size_t kPageSize = 64 * 1024; // 64 KB Prospero/Windows granularity
    static constexpr size_t kGuestVirtualSpaceSize = 16ULL * 1024 * 1024 * 1024; // 16 GB

    explicit SparseUnifiedMemory(const LowSpecConfig& config);
    ~SparseUnifiedMemory();

    bool TouchGuestPage(uint64_t guest_virtual_addr, bool gpu_hot, uint64_t current_frame);
    size_t EvictAndCompressColdPages(uint64_t current_frame);
    MemoryStats GetStats() const;

private:
    struct PageDescriptor {
        uint64_t guest_addr = 0;
        uint64_t last_touched_frame = 0;
        bool is_committed = false;
        bool is_gpu_resident = false;
        bool is_compressed = false;
        std::vector<uint8_t> compressed_payload;
    };

    LowSpecConfig config_;
    std::vector<PageDescriptor> page_table_;
    uint64_t committed_bytes_ = 0;
    uint64_t vram_bytes_ = 0;
    uint64_t compressed_count_ = 0;
    uint64_t saved_bytes_ = 0;
};

// Phase 3: GPU Texture Mip-Dropper & RDNA2 Shader Downgrader
struct TextureDescriptorTSharp {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t mip_levels = 1;
    uint32_t base_level = 0;
    bool is_render_target = false;
    bool is_ui_texture = false;
    uint64_t original_vram_bytes = 0;
    uint64_t downgraded_vram_bytes = 0;
};

struct ShaderDowngradeReport {
    uint32_t total_instructions = 0;
    uint32_t ngg_prim_Export_rewritten = 0;
    uint32_t bvh_ray_queries_stubbed = 0;
    uint32_t wave32_ballots_widened = 0;
    bool compiled_asynchronously = false;
};

class GpuLowSpecPipeline {
public:
    explicit GpuLowSpecPipeline(const LowSpecConfig& config);

    TextureDescriptorTSharp RewriteTextureDescriptor(TextureDescriptorTSharp desc) const;
    ShaderDowngradeReport DowngradeRdna2Shader(const std::vector<uint32_t>& rdna2_isa_words) const;
    void ComputeRenderResolution(uint32_t native_w, uint32_t native_h, uint32_t& out_w, uint32_t& out_h) const;

private:
    LowSpecConfig config_;
};

// Phase 4: Frame-Budgeted Async Kraken Decompressor
struct KrakenIoStats {
    uint32_t chunks_requested = 0;
    uint32_t chunks_completed_in_budget = 0;
    uint32_t chunks_deferred_next_frame = 0;
    uint32_t chunks_served_from_disk_cache = 0;
    double frame_decompression_time_ms = 0.0;
};

class AsyncKrakenEngine {
public:
    explicit AsyncKrakenEngine(const LowSpecConfig& config);
    KrakenIoStats ProcessFrameStreamingQueue(uint32_t pending_chunks, uint32_t cached_chunks);

private:
    LowSpecConfig config_;
};

// Phase 5: Tempest 3D Audio Bypass -> Stereo Downmixer
struct AudioMixStats {
    uint32_t input_3d_sources = 0;
    uint32_t output_frames_mixed = 0;
    double cpu_mix_time_us = 0.0;
    double estimated_hrtf_saved_ms = 0.0;
};

class StereoAudioDownmixer {
public:
    static AudioMixStats Mix3dVoicesToStereo(
        uint32_t num_voices,
        uint32_t sample_frames,
        bool bypass_hrtf);
};

// Phase 6: Hardware Auto-Profiler & Config Generator
class LowSpecAutoTuner {
public:
    static HostHardwareInfo DetectHostHardware();
    static LowSpecConfig BuildOptimalConfig(const HostHardwareInfo& hw);
    static bool WriteConfigIni(const std::string& path, const HostHardwareInfo& hw, const LowSpecConfig& cfg);
};

} // namespace ps5::lowspec
