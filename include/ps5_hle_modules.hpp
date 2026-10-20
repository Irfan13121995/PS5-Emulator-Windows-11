#pragma once

#include "ps5_lowspec.hpp"
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

namespace ps5::emu {

// ============================================================================
// Prospero OS Error Codes
// ============================================================================
constexpr int32_t SCE_OK                          = 0;
constexpr int32_t SCE_KERNEL_ERROR_EINVAL         = static_cast<int32_t>(0x80020016U);
constexpr int32_t SCE_KERNEL_ERROR_ENOMEM         = static_cast<int32_t>(0x8002000CU);
constexpr int32_t SCE_KERNEL_ERROR_ENOENT         = static_cast<int32_t>(0x80020002U);
constexpr int32_t SCE_KERNEL_ERROR_EACCES         = static_cast<int32_t>(0x8002000DU);

// ============================================================================
// 1. `libkernel` HLE: Memory, Pthreads, Time, and Sandboxed VFS (`/app0`)
// ============================================================================

class HleLibKernel {
public:
    explicit HleLibKernel(ps5::lowspec::SparseUnifiedMemory& mem, const std::string& host_app0_root);

    int32_t sceKernelAllocateDirectMemory(
        int64_t search_start,
        int64_t search_end,
        size_t len,
        size_t alignment,
        int32_t memory_type,
        int64_t* out_phys_addr);

    int32_t sceKernelMapDirectMemory(
        uint64_t* inout_vaddr,
        size_t len,
        int32_t prot,
        int32_t flags,
        int64_t direct_memory_start,
        size_t alignment);

    int32_t sceKernelOpen(const std::string& guest_path, int32_t flags, int32_t mode);
    int64_t sceKernelRead(int32_t fd, void* buf, size_t count);
    int32_t sceKernelClose(int32_t fd);
    uint64_t sceKernelGetProcessTimeCounter() const;

    // Security check: resolves `/app0/...` inside `host_app0_root_` and blocks `..` traversal
    bool ResolveSandboxedGuestPath(const std::string& guest_path, std::string& out_host_path) const;

private:
    ps5::lowspec::SparseUnifiedMemory& mem_;
    std::string host_app0_root_;
    mutable std::mutex mutex_;
    int64_t next_direct_phys_addr_ = 0x0000000100000000LL;
    uint64_t next_virtual_base_    = 0x0000000200000000ULL;
    int32_t next_fd_               = 3;
    std::unordered_map<int32_t, std::string> open_files_;
};

// ============================================================================
// 2. `libSceVideoOut` & `libSceAgc` HLE: Display Flip & Command Buffer Processor
// ============================================================================

enum class AgcPm4Opcode : uint8_t {
    IT_NOP              = 0x10,
    IT_CLEAR_COLOR      = 0x12,
    IT_DRAW_INDEX_2     = 0x27,
    IT_DRAW_INDEX_AUTO  = 0x2D,
    IT_DISPATCH_DIRECT  = 0x15,
    IT_SET_CONTEXT_REG  = 0x69,
    IT_SET_SH_REG       = 0x76,
    IT_EVENT_WRITE      = 0x46
};

struct SceVideoOutResolutionStatus {
    uint32_t full_width = 1920;
    uint32_t full_height = 1080;
    uint32_t pane_width = 1280;
    uint32_t pane_height = 720;
};

struct AgcFrameTelemetry {
    uint64_t frame_index = 0;
    uint32_t dcb_packets_parsed = 0;
    uint32_t draw_calls_executed = 0;
    uint32_t compute_dispatches = 0;
    uint32_t clear_color_rgba8 = 0x101828FFU;
    uint32_t internal_width = 1280;
    uint32_t internal_height = 720;
    uint32_t output_width = 1920;
    uint32_t output_height = 1080;
};

class HleVideoAndAgc {
public:
    explicit HleVideoAndAgc(const ps5::lowspec::LowSpecConfig& cfg);

    int32_t sceVideoOutOpen(int32_t user_id, int32_t bus_type, int32_t index, const void* param);
    int32_t sceVideoOutGetResolutionStatus(int32_t handle, SceVideoOutResolutionStatus* out_status) const;
    int32_t sceVideoOutSubmitFlip(int32_t handle, int32_t buffer_index, int32_t flip_mode, int64_t flip_arg);

    AgcFrameTelemetry SubmitAndExecuteAgcCommandBuffer(const std::vector<uint32_t>& dcb_words);

private:
    ps5::lowspec::LowSpecConfig config_;
    ps5::lowspec::GpuLowSpecPipeline gpu_pipeline_;
    uint64_t frame_counter_ = 0;
};

// ============================================================================
// 3. `libScePad` & `libSceAudioOut` HLE: DualSense Input & Stereo Audio
// ============================================================================

constexpr uint32_t SCE_PAD_BUTTON_UP       = 0x00000010U;
constexpr uint32_t SCE_PAD_BUTTON_RIGHT    = 0x00000020U;
constexpr uint32_t SCE_PAD_BUTTON_DOWN     = 0x00000040U;
constexpr uint32_t SCE_PAD_BUTTON_LEFT     = 0x00000080U;
constexpr uint32_t SCE_PAD_BUTTON_L1       = 0x00000400U;
constexpr uint32_t SCE_PAD_BUTTON_R1       = 0x00000800U;
constexpr uint32_t SCE_PAD_BUTTON_TRIANGLE = 0x00001000U;
constexpr uint32_t SCE_PAD_BUTTON_CIRCLE   = 0x00002000U;
constexpr uint32_t SCE_PAD_BUTTON_CROSS    = 0x00004000U;
constexpr uint32_t SCE_PAD_BUTTON_SQUARE   = 0x00008000U;

struct ScePadData {
    uint32_t buttons = 0;
    uint8_t  left_stick_x = 128;
    uint8_t  left_stick_y = 128;
    uint8_t  right_stick_x = 128;
    uint8_t  right_stick_y = 128;
    uint8_t  l2_analog = 0;
    uint8_t  r2_analog = 0;
    bool     connected = true;
};

class HlePadAndAudio {
public:
    int32_t scePadInit();
    int32_t scePadOpen(int32_t user_id, int32_t type, int32_t index);
    int32_t scePadReadState(int32_t handle, ScePadData* out_data) const;
    void UpdateHostInputState(const ScePadData& state);

    int32_t sceAudioOutOpen(int32_t user_id, int32_t port_type, int32_t index, uint32_t len, uint32_t freq, uint32_t param);
    int32_t sceAudioOutOutput(int32_t handle, const void* ptr);

private:
    ScePadData current_pad_{};
    uint64_t audio_blocks_submitted_ = 0;
};

// ============================================================================
// 4. Save State Snapshot Engine (`.ps5save`)
// ============================================================================

struct EmulatorSaveState {
    uint64_t frame_number = 0;
    float    player_pos_x = 0.0f;
    float    player_pos_y = 0.0f;
    float    camera_yaw   = 0.0f;
    uint32_t lightbar_rgb = 0x6E48D2U;
    uint64_t guest_rip    = 0;
    uint64_t guest_rsp    = 0;
    std::vector<uint8_t> guest_ram_snapshot;
};

class SaveStateManager {
public:
    static bool SaveSnapshotToFile(const std::string& path, const EmulatorSaveState& state);
    static bool LoadSnapshotFromFile(const std::string& path, EmulatorSaveState& out_state);
};

} // namespace ps5::emu
