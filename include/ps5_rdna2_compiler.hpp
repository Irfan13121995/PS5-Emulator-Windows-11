#pragma once

#include "ps5_lowspec.hpp"
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace ps5::emu {

// ============================================================================
// 1. RDNA2 `sce::Agc` Hardware Resource Descriptors (`T#` 32B & `V#` 16B)
// ============================================================================

struct AgcBufferDescriptorVSharp {
    uint64_t base_address = 0; // 48-bit GPU virtual address
    uint32_t stride       = 0; // 14-bit byte stride
    uint32_t num_records  = 0; // 32-bit element count
};

struct AgcTextureDescriptorTSharp {
    uint64_t base_address_256b = 0;
    uint32_t data_format       = 0; // e.g. BC1=0x23, BC7=0x29, RGBA8=0x0A
    uint32_t width             = 1;
    uint32_t height            = 1;
    uint32_t base_level        = 0;
    uint32_t last_level        = 0;
    uint32_t tile_mode         = 0; // Standard64KB swizzle vs Linear
};

// ============================================================================
// 2. RDNA2 Binary Shader Disassembler & HLSL/SPIR-V Lifter
// ============================================================================

enum class Rdna2ShaderStage : uint8_t {
    Vertex   = 0,
    Pixel    = 1,
    Compute  = 2
};

struct TranslatedShaderArtifact {
    Rdna2ShaderStage stage = Rdna2ShaderStage::Pixel;
    uint32_t binary_size_bytes = 0;
    uint32_t instructions_decoded = 0;
    uint32_t srt_sgpr_loads = 0;
    uint32_t bvh_ray_queries_stubbed = 0;
    uint32_t export_instructions = 0;
    std::string generated_hlsl_source;
};

class Rdna2ShaderCompiler {
public:
    static AgcTextureDescriptorTSharp DecodeTSharp(const uint32_t dwords[8]);
    static AgcBufferDescriptorVSharp DecodeVSharp(const uint32_t dwords[4]);

    static TranslatedShaderArtifact LiftRdna2BinaryToHlsl(
        const std::vector<uint32_t>& rdna2_dwords,
        Rdna2ShaderStage stage,
        const ps5::lowspec::LowSpecConfig& lowspec_cfg);

    static std::vector<uint32_t> BuildSampleCommercialRdna2ShaderBinary();
};

// ============================================================================
// 3. RDNA2 Micro-Tiled Texture Deswizzler (`Standard64KB` -> Linear)
// ============================================================================

class AgcTextureDeswizzler {
public:
    static uint32_t ComputeMorton2D(uint32_t x, uint32_t y);
    static std::vector<uint8_t> DeswizzleBCnSurfaceAndDropMips(
        const uint8_t* swizzled_src,
        size_t src_size,
        uint32_t width,
        uint32_t height,
        uint32_t mip_drop_levels,
        uint32_t& out_width,
        uint32_t& out_height);
};

} // namespace ps5::emu
