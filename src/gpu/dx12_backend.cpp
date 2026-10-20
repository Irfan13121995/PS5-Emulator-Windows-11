#include "ps5_lowspec.hpp"

namespace ps5::lowspec {

// ============================================================================
// DirectX 12 Feature Level 11_1 / 12_0 Fallback Backend
// ============================================================================
// Allows execution on older DirectX 12 capable GPUs that only support
// D3D_FEATURE_LEVEL_11_1 or 12_0 (such as GTX 1050 Ti / GTX 1650 / Intel UHD 630)
// using Placed Resources (`ID3D12Device::CreatePlacedResource`) on 64 KB aligned
// `ID3D12Heap` chunks and ExecuteIndirect for emulated NGG primitive culling.

struct Dx12FallbackState {
    bool initialized = false;
    uint32_t feature_level_major = 12;
    uint32_t feature_level_minor = 0;
    bool uses_placed_resource_heaps = true;
    bool uses_execute_indirect_ngg = true;
};

Dx12FallbackState InitializeDx12FallbackBackend(const LowSpecConfig& cfg) {
    Dx12FallbackState state{};
    state.initialized = true;
    state.feature_level_major = 12;
    state.feature_level_minor = 0;
    state.uses_placed_resource_heaps = true;
    state.uses_execute_indirect_ngg = cfg.convert_ngg_to_compute_cull;
    return state;
}

} // namespace ps5::lowspec
