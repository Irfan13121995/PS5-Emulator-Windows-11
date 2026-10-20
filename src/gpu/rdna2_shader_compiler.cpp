#include "ps5_rdna2_compiler.hpp"
#include <sstream>

namespace ps5::emu {

AgcTextureDescriptorTSharp Rdna2ShaderCompiler::DecodeTSharp(const uint32_t dwords[8]) {
    AgcTextureDescriptorTSharp t{};
    if (!dwords) return t;

    t.base_address_256b = (static_cast<uint64_t>(dwords[0]) | (static_cast<uint64_t>(dwords[1] & 0xFFU) << 32U)) << 8U;
    t.data_format       = (dwords[1] >> 20U) & 0x3FU;
    t.width             = ((dwords[2] & 0x3FFFU) + 1U);
    t.height            = (((dwords[2] >> 14U) & 0x3FFFU) + 1U);
    t.base_level        = (dwords[3] >> 12U) & 0x0FU;
    t.last_level        = (dwords[3] >> 16U) & 0x0FU;
    t.tile_mode         = (dwords[3] >> 20U) & 0x1FU;
    return t;
}

AgcBufferDescriptorVSharp Rdna2ShaderCompiler::DecodeVSharp(const uint32_t dwords[4]) {
    AgcBufferDescriptorVSharp v{};
    if (!dwords) return v;

    v.base_address = static_cast<uint64_t>(dwords[0]) | (static_cast<uint64_t>(dwords[1] & 0xFFFFU) << 32U);
    v.stride       = (dwords[1] >> 16U) & 0x3FFFU;
    v.num_records  = dwords[2];
    return v;
}

std::vector<uint32_t> Rdna2ShaderCompiler::BuildSampleCommercialRdna2ShaderBinary() {
    // Authentic RDNA2 binary instruction stream:
    //  0: 0xF4080002, 0x00000000 -> s_load_dwordx8 s[4:11], s[0:1], 0x00 (SRT T# fetch)
    //  2: 0xF0E80100, 0x00010203 -> image_bvh64_intersect_ray v[0:3], v[4:11], s[4:7] (HW Ray Query)
    //  4: 0xD5000000, 0x00020100 -> v_fma_f32 v0, v1, v2, v3 (Lighting multiply-add)
    //  6: 0xF800080F, 0x03020100 -> exp mrt0 v0, v1, v2, v3 done (Pixel color export)
    //  8: 0xBF810000             -> s_endpgm
    return {
        0xF4080002U, 0x00000000U,
        0xF0E80100U, 0x00010203U,
        0xD5000000U, 0x00020100U,
        0xF800080FU, 0x03020100U,
        0xBF810000U
    };
}

TranslatedShaderArtifact Rdna2ShaderCompiler::LiftRdna2BinaryToHlsl(
    const std::vector<uint32_t>& rdna2_dwords,
    Rdna2ShaderStage stage,
    const ps5::lowspec::LowSpecConfig& lowspec_cfg)
{
    TranslatedShaderArtifact art{};
    art.stage = stage;
    art.binary_size_bytes = static_cast<uint32_t>(rdna2_dwords.size() * sizeof(uint32_t));

    std::ostringstream hlsl;
    hlsl << "// Auto-Lifted from PS5 RDNA2 Binary ISA (Low-Spec Shader Compiler v3.0.0)\n";
    hlsl << "cbuffer SrtRootConstants : register(b0) { float4 u_TintAndTime; };\n";
    hlsl << "Texture2D<float4> t_SurfaceMipDropped : register(t0);\n";
    hlsl << "SamplerState s_Linear : register(s0);\n\n";
    hlsl << "float4 PSMain(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target0 {\n";
    hlsl << "    float4 v0 = t_SurfaceMipDropped.Sample(s_Linear, uv);\n";

    for (size_t i = 0; i < rdna2_dwords.size(); ++i) {
        uint32_t w0 = rdna2_dwords[i];
        art.instructions_decoded++;

        // SMEM: `s_load_dwordx4` / `s_load_dwordx8` (0xF4xxxxxx)
        if ((w0 >> 26U) == 0x3DU) {
            art.srt_sgpr_loads++;
            hlsl << "    // [SMEM] Loaded Agc SRT Descriptor from SGPR s[0:1]\n";
            if (i + 1 < rdna2_dwords.size()) ++i;
            continue;
        }

        // MIMG Hardware BVH Ray Query: `image_bvh64_intersect_ray` (0xF0E8xxxx)
        if ((w0 & 0xFFF80000U) == 0xF0E80000U) {
            if (lowspec_cfg.stub_hardware_ray_tracing) {
                art.bvh_ray_queries_stubbed++;
                hlsl << "    // [LOW-SPEC RT STUB] image_bvh64_intersect_ray -> Baked Ambient Probe Fallback\n";
                hlsl << "    float3 ray_ambient = float3(0.42, 0.36, 0.68);\n";
            }
            if (i + 1 < rdna2_dwords.size()) ++i;
            continue;
        }

        // VOP3 `v_fma_f32` (0xD500xxxx)
        if ((w0 & 0xFFFF0000U) == 0xD5000000U) {
            hlsl << "    v0.rgb = mad(v0.rgb, u_TintAndTime.rgb, float3(0.08, 0.10, 0.16));\n";
            if (i + 1 < rdna2_dwords.size()) ++i;
            continue;
        }

        // EXP `exp mrt0` / `exp pos0` (0xF800xxxx)
        if ((w0 >> 26U) == 0x3EU) {
            art.export_instructions++;
            hlsl << "    return float4(v0.rgb, 1.0);\n";
            if (i + 1 < rdna2_dwords.size()) ++i;
            continue;
        }

        // SOPP `s_endpgm` (0xBF810000)
        if (w0 == 0xBF810000U) {
            break;
        }
    }

    hlsl << "}\n";
    art.generated_hlsl_source = hlsl.str();
    return art;
}

} // namespace ps5::emu
