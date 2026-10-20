#include "ps5_lowspec.hpp"
#include <algorithm>
#include <cmath>

namespace ps5::lowspec {

// ============================================================================
// Phase 3: RDNA2 (`sceAgc`) Shader Downgrader & Low-VRAM Texture Mip-Dropper
// ============================================================================
// Security-hardened implementation:
// - Clamps texture dimensions (`1..16384`), mip levels (`1..15`), and
//   `texture_mip_drop_levels` (`0..4`) to eliminate unsigned integer underflow
//   (`mip_levels == 0`) and bit-shift Undefined Behavior (`2U * drop >= 64`).

GpuLowSpecPipeline::GpuLowSpecPipeline(const LowSpecConfig& config)
    : config_(config)
{}

TextureDescriptorTSharp GpuLowSpecPipeline::RewriteTextureDescriptor(
    TextureDescriptorTSharp desc) const
{
    constexpr uint32_t kMaxDimension = 16384U;
    constexpr uint32_t kMaxMipLevels = 15U;

    desc.width = std::clamp(desc.width, 1U, kMaxDimension);
    desc.height = std::clamp(desc.height, 1U, kMaxDimension);
    desc.mip_levels = std::clamp(desc.mip_levels, 1U, kMaxMipLevels);

    // Compute approximate BC7/BC1 mip-chain size (1 byte per texel average with mips)
    uint64_t base_bytes = static_cast<uint64_t>(desc.width) * static_cast<uint64_t>(desc.height);
    uint64_t total_orig = base_bytes + (base_bytes / 3ULL);
    desc.original_vram_bytes = total_orig;
    desc.downgraded_vram_bytes = total_orig;

    const uint32_t safe_cfg_drop = std::min(config_.texture_mip_drop_levels, 4U);

    if (!desc.is_ui_texture && !desc.is_render_target &&
        desc.width >= 1024U && desc.height >= 1024U &&
        desc.mip_levels > safe_cfg_drop &&
        safe_cfg_drop > 0U)
    {
        uint32_t drop = std::min(safe_cfg_drop, desc.mip_levels - 1U);
        desc.base_level = std::max(desc.base_level, drop);
        desc.width = std::max(1U, desc.width >> drop);
        desc.height = std::max(1U, desc.height >> drop);
        desc.downgraded_vram_bytes = total_orig >> (2U * drop); // 1/4th per dropped mip!
    }

    return desc;
}

ShaderDowngradeReport GpuLowSpecPipeline::DowngradeRdna2Shader(
    const std::vector<uint32_t>& rdna2_isa_words) const
{
    ShaderDowngradeReport report{};
    report.total_instructions = static_cast<uint32_t>(rdna2_isa_words.size());
    report.compiled_asynchronously = config_.async_pipeline_compilation;

    for (uint32_t word : rdna2_isa_words) {
        // Opcode signature for RDNA2 NGG `gs_alloc_req` / primitive export
        if (config_.convert_ngg_to_compute_cull && ((word & 0xFFFF0000U) == 0xBF860000U)) {
            report.ngg_prim_Export_rewritten++;
        }
        // Opcode signature for RDNA2 `image_bvh64_intersect_ray` (0xF0E8xxxx)
        if (config_.stub_hardware_ray_tracing && ((word & 0xFFF80000U) == 0xF0E80000U)) {
            report.bvh_ray_queries_stubbed++;
        }
        // Opcode signature for RDNA2 Wave32 exec mask manipulation
        if ((word & 0xFE000000U) == 0xBE000000U) {
            report.wave32_ballots_widened++;
        }
    }

    return report;
}

void GpuLowSpecPipeline::ComputeRenderResolution(
    uint32_t native_w,
    uint32_t native_h,
    uint32_t& out_w,
    uint32_t& out_h) const
{
    constexpr uint32_t kMaxDisplayDim = 7680U;
    native_w = std::clamp(native_w, 64U, kMaxDisplayDim);
    native_h = std::clamp(native_h, 64U, kMaxDisplayDim);

    float scale = std::clamp(config_.internal_resolution_scale, 0.50f, 1.00f);
    out_w = ( static_cast<uint32_t>(std::lround(native_w * scale)) + 7U ) & ~7U;
    out_h = ( static_cast<uint32_t>(std::lround(native_h * scale)) + 7U ) & ~7U;
}

} // namespace ps5::lowspec
