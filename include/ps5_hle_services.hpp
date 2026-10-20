#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <mutex>

namespace ps5::emu {

// ============================================================================
// 1. `SceKernelEqueue` & VBlank Synchronization Engine
// ============================================================================
// Commercial PS5 games register a video flip event queue via
// `sceVideoOutAddFlipEvent(eq, videoHandle, udata)` and block their render
// loop on `sceKernelWaitEqueue(eq, ev, num, &out, timeout)`.

constexpr int16_t EVFILT_READ      = -1;
constexpr int16_t EVFILT_WRITE     = -2;
constexpr int16_t EVFILT_USER      = -11;
constexpr int16_t EVFILT_VIDEO_OUT = -13;

struct SceKernelEvent {
    uintptr_t ident  = 0;
    int16_t   filter = EVFILT_VIDEO_OUT;
    uint16_t  flags  = 0;
    uint32_t  fflags = 0;
    intptr_t  data   = 0;
    void*     udata  = nullptr;
};

class HleKernelEqueueEngine {
public:
    int32_t sceKernelCreateEqueue(uint64_t* out_eq, const std::string& name);
    int32_t sceVideoOutAddFlipEvent(uint64_t eq, int32_t video_handle, void* udata);
    int32_t sceKernelTriggerUserEvent(uint64_t eq, uintptr_t id, void* udata);
    int32_t sceKernelWaitEqueue(
        uint64_t eq,
        SceKernelEvent* out_events,
        int32_t max_events,
        int32_t* out_num_events,
        uint32_t timeout_us);

private:
    mutable std::mutex mutex_;
    uint64_t next_eq_handle_ = 0x100ULL;
    uint64_t vblank_counter_ = 0;
};

// ============================================================================
// 2. `libSceUserService`, `libSceSystemService`, `libSceSaveData` & `libSceNp`
// ============================================================================

constexpr int32_t kDefaultLoggedInUserId = 0x10000001;

struct SceSystemServiceStatus {
    int32_t  event_count = 0;
    bool     is_in_background_execution = false;
    bool     is_cpu_mode_7cpu_normal = true;
    bool     is_game_live_streaming_on_air = false;
    bool     is_out_of_vr_play_area = false;
};

class HleSystemAndUserServices {
public:
    // libSceUserService
    int32_t sceUserServiceInitialize(const void* params);
    int32_t sceUserServiceGetInitialUser(int32_t* out_user_id) const;
    int32_t sceUserServiceGetUserName(int32_t user_id, char* out_name, size_t max_size) const;

    // libSceSystemService
    int32_t sceSystemServiceGetStatus(SceSystemServiceStatus* out_status) const;
    int32_t sceSystemServiceHideSplashScreen();
    bool IsSplashScreenHidden() const { return splash_hidden_; }

    // libSceSaveData
    int32_t sceSaveDataInitialize3(const void* init_params);
    int32_t sceSaveDataMount2(const std::string& title_id, const std::string& dir_name, std::string& out_mount_point);

    // libSceNpManager / libSceNpTrophy2 Offline Safe Stubs
    int32_t sceNpManagerGetStatus(int32_t* out_state) const;
    int32_t sceNpTrophy2CreateContext(int32_t* out_context, int32_t user_id) const;

private:
    bool user_initialized_ = false;
    bool splash_hidden_ = false;
};

} // namespace ps5::emu
