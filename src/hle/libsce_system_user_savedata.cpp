#include "ps5_hle_services.hpp"
#include "ps5_hle_modules.hpp"
#include <cstring>
#include <algorithm>

namespace ps5::emu {

int32_t HleSystemAndUserServices::sceUserServiceInitialize(const void* /*params*/) {
    user_initialized_ = true;
    return SCE_OK;
}

int32_t HleSystemAndUserServices::sceUserServiceGetInitialUser(int32_t* out_user_id) const {
    if (!out_user_id) {
        return SCE_KERNEL_ERROR_EINVAL;
    }
    *out_user_id = kDefaultLoggedInUserId; // 0x10000001
    return SCE_OK;
}

int32_t HleSystemAndUserServices::sceUserServiceGetUserName(
    int32_t /*user_id*/,
    char* out_name,
    size_t max_size) const
{
    if (!out_name || max_size == 0) {
        return SCE_KERNEL_ERROR_EINVAL;
    }
    constexpr const char* kName = "ProsperoPlayer1";
    size_t copy_len = std::min(std::strlen(kName), max_size - 1U);
    std::memcpy(out_name, kName, copy_len);
    out_name[copy_len] = '\0';
    return SCE_OK;
}

int32_t HleSystemAndUserServices::sceSystemServiceGetStatus(
    SceSystemServiceStatus* out_status) const
{
    if (!out_status) {
        return SCE_KERNEL_ERROR_EINVAL;
    }
    out_status->event_count = 0;
    out_status->is_in_background_execution = false;
    out_status->is_cpu_mode_7cpu_normal = true;
    out_status->is_game_live_streaming_on_air = false;
    out_status->is_out_of_vr_play_area = false;
    return SCE_OK;
}

int32_t HleSystemAndUserServices::sceSystemServiceHideSplashScreen() {
    splash_hidden_ = true;
    return SCE_OK;
}

int32_t HleSystemAndUserServices::sceSaveDataInitialize3(const void* /*init_params*/) {
    return SCE_OK;
}

int32_t HleSystemAndUserServices::sceSaveDataMount2(
    const std::string& title_id,
    const std::string& /*dir_name*/,
    std::string& out_mount_point)
{
    if (title_id.empty()) {
        return SCE_KERNEL_ERROR_EINVAL;
    }
    out_mount_point = "/savedata0";
    return SCE_OK;
}

int32_t HleSystemAndUserServices::sceNpManagerGetStatus(int32_t* out_state) const {
    if (!out_state) {
        return SCE_KERNEL_ERROR_EINVAL;
    }
    *out_state = 1; // Signed-in local profile stub
    return SCE_OK;
}

int32_t HleSystemAndUserServices::sceNpTrophy2CreateContext(
    int32_t* out_context,
    int32_t /*user_id*/) const
{
    if (!out_context) {
        return SCE_KERNEL_ERROR_EINVAL;
    }
    *out_context = 1;
    return SCE_OK;
}

} // namespace ps5::emu
