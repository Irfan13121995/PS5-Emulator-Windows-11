// Auto-Lifted from PS5 RDNA2 Binary ISA (v3.0.0 Commercial Shader Lifter)
// Title ID: PPSA00001 (Prospero Astro-Prism 3D (Commercial Layout Package))
cbuffer SrtRootConstants : register(b0) { float4 u_TintAndTime; };
Texture2D<float4> t_SurfaceMipDropped : register(t0);
SamplerState s_Linear : register(s0);
float4 PSMain(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target0 {
    float4 v0 = t_SurfaceMipDropped.Sample(s_Linear, uv);
    // [SMEM] s_load_dwordx8 T# Descriptor from SRT SGPR s[0:1]
    // [LOW-SPEC RT STUB] image_bvh64_intersect_ray -> Baked Probe Fallback
    float3 ray_probe = float3(0.42, 0.36, 0.68);
    v0.rgb = mad(v0.rgb, u_TintAndTime.rgb, float3(0.08, 0.10, 0.16));
    return float4(v0.rgb, 1.0);
}
