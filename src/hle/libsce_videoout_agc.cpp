#include "ps5_hle_modules.hpp"

namespace ps5::emu {

HleVideoAndAgc::HleVideoAndAgc(const ps5::lowspec::LowSpecConfig& cfg)
    : config_(cfg)
    , gpu_pipeline_(cfg)
{}

int32_t HleVideoAndAgc::sceVideoOutOpen(
    int32_t /*user_id*/,
    int32_t /*bus_type*/,
    int32_t /*index*/,
    const void* /*param*/)
{
    return 1; // Primary display handle
}

int32_t HleVideoAndAgc::sceVideoOutGetResolutionStatus(
    int32_t handle,
    SceVideoOutResolutionStatus* out_status) const
{
    if (handle <= 0 || !out_status) {
        return SCE_KERNEL_ERROR_EINVAL;
    }

    uint32_t internal_w = 1280, internal_h = 720;
    gpu_pipeline_.ComputeRenderResolution(1920, 1080, internal_w, internal_h);

    out_status->full_width  = 1920;
    out_status->full_height = 1080;
    out_status->pane_width  = internal_w;
    out_status->pane_height = internal_h;
    return SCE_OK;
}

int32_t HleVideoAndAgc::sceVideoOutSubmitFlip(
    int32_t handle,
    int32_t /*buffer_index*/,
    int32_t /*flip_mode*/,
    int64_t /*flip_arg*/)
{
    if (handle <= 0) {
        return SCE_KERNEL_ERROR_EINVAL;
    }
    frame_counter_++;
    return SCE_OK;
}

AgcFrameTelemetry HleVideoAndAgc::SubmitAndExecuteAgcCommandBuffer(
    const std::vector<uint32_t>& dcb_words)
{
    AgcFrameTelemetry tel{};
    tel.frame_index = ++frame_counter_;
    gpu_pipeline_.ComputeRenderResolution(1920, 1080, tel.internal_width, tel.internal_height);
    tel.output_width  = 1920;
    tel.output_height = 1080;

    // Parse PM4 Type-3 packets (`0xC000XX00 | (opcode << 8)`)
    for (size_t i = 0; i < dcb_words.size(); ++i) {
        uint32_t header = dcb_words[i];
        if ((header >> 30U) != 3U) {
            continue;
        }
        tel.dcb_packets_parsed++;
        uint8_t opcode = static_cast<uint8_t>((header >> 8U) & 0xFFU);

        switch (static_cast<AgcPm4Opcode>(opcode)) {
            case AgcPm4Opcode::IT_CLEAR_COLOR:
                if (i + 1 < dcb_words.size()) {
                    tel.clear_color_rgba8 = dcb_words[++i];
                }
                break;
            case AgcPm4Opcode::IT_DRAW_INDEX_2:
            case AgcPm4Opcode::IT_DRAW_INDEX_AUTO:
                tel.draw_calls_executed++;
                break;
            case AgcPm4Opcode::IT_DISPATCH_DIRECT:
                tel.compute_dispatches++;
                break;
            default:
                break;
        }
    }

    return tel;
}

} // namespace ps5::emu
