#include "ps5_hle_services.hpp"
#include "ps5_hle_modules.hpp"

namespace ps5::emu {

int32_t HleKernelEqueueEngine::sceKernelCreateEqueue(
    uint64_t* out_eq,
    const std::string& /*name*/)
{
    if (!out_eq) {
        return SCE_KERNEL_ERROR_EINVAL;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    *out_eq = next_eq_handle_++;
    return SCE_OK;
}

int32_t HleKernelEqueueEngine::sceVideoOutAddFlipEvent(
    uint64_t eq,
    int32_t video_handle,
    void* /*udata*/)
{
    if (eq == 0 || video_handle <= 0) {
        return SCE_KERNEL_ERROR_EINVAL;
    }
    return SCE_OK;
}

int32_t HleKernelEqueueEngine::sceKernelTriggerUserEvent(
    uint64_t eq,
    uintptr_t /*id*/,
    void* /*udata*/)
{
    if (eq == 0) {
        return SCE_KERNEL_ERROR_EINVAL;
    }
    return SCE_OK;
}

int32_t HleKernelEqueueEngine::sceKernelWaitEqueue(
    uint64_t eq,
    SceKernelEvent* out_events,
    int32_t max_events,
    int32_t* out_num_events,
    uint32_t /*timeout_us*/)
{
    if (eq == 0 || !out_events || max_events <= 0 || !out_num_events) {
        return SCE_KERNEL_ERROR_EINVAL;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    vblank_counter_++;

    // Immediately deliver a 60 Hz EVFILT_VIDEO_OUT VBlank/Flip completion event
    // so commercial PS5 render threads never deadlock waiting for display flip.
    out_events[0].ident  = 1;
    out_events[0].filter = EVFILT_VIDEO_OUT;
    out_events[0].flags  = 0;
    out_events[0].fflags = 0;
    out_events[0].data   = static_cast<intptr_t>(vblank_counter_);
    out_events[0].udata  = nullptr;

    *out_num_events = 1;
    return SCE_OK;
}

} // namespace ps5::emu
