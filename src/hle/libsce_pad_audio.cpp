#include "ps5_hle_modules.hpp"
#include <fstream>
#include <cstring>

namespace ps5::emu {

int32_t HlePadAndAudio::scePadInit() {
    current_pad_.connected = true;
    current_pad_.left_stick_x = 128;
    current_pad_.left_stick_y = 128;
    current_pad_.right_stick_x = 128;
    current_pad_.right_stick_y = 128;
    return SCE_OK;
}

int32_t HlePadAndAudio::scePadOpen(int32_t /*user_id*/, int32_t /*type*/, int32_t /*index*/) {
    return 1; // Primary DualSense pad handle
}

int32_t HlePadAndAudio::scePadReadState(int32_t handle, ScePadData* out_data) const {
    if (handle <= 0 || !out_data) {
        return SCE_KERNEL_ERROR_EINVAL;
    }
    *out_data = current_pad_;
    return SCE_OK;
}

void HlePadAndAudio::UpdateHostInputState(const ScePadData& state) {
    current_pad_ = state;
}

int32_t HlePadAndAudio::sceAudioOutOpen(
    int32_t /*user_id*/,
    int32_t /*port_type*/,
    int32_t /*index*/,
    uint32_t /*len*/,
    uint32_t /*freq*/,
    uint32_t /*param*/)
{
    return 1; // Primary stereo audio port handle
}

int32_t HlePadAndAudio::sceAudioOutOutput(int32_t handle, const void* /*ptr*/) {
    if (handle <= 0) {
        return SCE_KERNEL_ERROR_EINVAL;
    }
    ps5::lowspec::StereoAudioDownmixer::Mix3dVoicesToStereo(32, 256, true);
    audio_blocks_submitted_++;
    return SCE_OK;
}

// ============================================================================
// Save State Snapshot Serialization (`.ps5save`)
// ============================================================================

namespace {
constexpr uint32_t kSaveStateMagic = 0x56415335U; // "5SAV"
}

bool SaveStateManager::SaveSnapshotToFile(
    const std::string& path,
    const EmulatorSaveState& state)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }

    uint32_t magic = kSaveStateMagic;
    uint32_t ram_len = static_cast<uint32_t>(state.guest_ram_snapshot.size());

    out.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    out.write(reinterpret_cast<const char*>(&state.frame_number), sizeof(state.frame_number));
    out.write(reinterpret_cast<const char*>(&state.player_pos_x), sizeof(state.player_pos_x));
    out.write(reinterpret_cast<const char*>(&state.player_pos_y), sizeof(state.player_pos_y));
    out.write(reinterpret_cast<const char*>(&state.camera_yaw), sizeof(state.camera_yaw));
    out.write(reinterpret_cast<const char*>(&state.lightbar_rgb), sizeof(state.lightbar_rgb));
    out.write(reinterpret_cast<const char*>(&state.guest_rip), sizeof(state.guest_rip));
    out.write(reinterpret_cast<const char*>(&state.guest_rsp), sizeof(state.guest_rsp));
    out.write(reinterpret_cast<const char*>(&ram_len), sizeof(ram_len));
    if (ram_len > 0) {
        out.write(reinterpret_cast<const char*>(state.guest_ram_snapshot.data()), ram_len);
    }

    return out.good();
}

bool SaveStateManager::LoadSnapshotFromFile(
    const std::string& path,
    EmulatorSaveState& out_state)
{
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return false;
    }

    uint32_t magic = 0;
    uint32_t ram_len = 0;
    in.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    if (magic != kSaveStateMagic) {
        return false;
    }

    in.read(reinterpret_cast<char*>(&out_state.frame_number), sizeof(out_state.frame_number));
    in.read(reinterpret_cast<char*>(&out_state.player_pos_x), sizeof(out_state.player_pos_x));
    in.read(reinterpret_cast<char*>(&out_state.player_pos_y), sizeof(out_state.player_pos_y));
    in.read(reinterpret_cast<char*>(&out_state.camera_yaw), sizeof(out_state.camera_yaw));
    in.read(reinterpret_cast<char*>(&out_state.lightbar_rgb), sizeof(out_state.lightbar_rgb));
    in.read(reinterpret_cast<char*>(&out_state.guest_rip), sizeof(out_state.guest_rip));
    in.read(reinterpret_cast<char*>(&out_state.guest_rsp), sizeof(out_state.guest_rsp));
    in.read(reinterpret_cast<char*>(&ram_len), sizeof(ram_len));

    constexpr uint32_t kMaxSnapshotRam = 64U * 1024U * 1024U; // 64 MB safety cap
    if (ram_len > kMaxSnapshotRam) {
        return false;
    }

    out_state.guest_ram_snapshot.resize(ram_len);
    if (ram_len > 0) {
        in.read(reinterpret_cast<char*>(out_state.guest_ram_snapshot.data()), ram_len);
    }

    return in.good();
}

} // namespace ps5::emu
