#include "ps5_rdna2_compiler.hpp"
#include <algorithm>
#include <cstring>

namespace ps5::emu {

uint32_t AgcTextureDeswizzler::ComputeMorton2D(uint32_t x, uint32_t y) {
    auto part1by1 = [](uint32_t n) -> uint32_t {
        n &= 0x0000FFFFU;
        n = (n | (n << 8U)) & 0x00FF00FFU;
        n = (n | (n << 4U)) & 0x0F0F0F0FU;
        n = (n | (n << 2U)) & 0x33333333U;
        n = (n | (n << 1U)) & 0x55555555U;
        return n;
    };
    return part1by1(x) | (part1by1(y) << 1U);
}

std::vector<uint8_t> AgcTextureDeswizzler::DeswizzleBCnSurfaceAndDropMips(
    const uint8_t* swizzled_src,
    size_t src_size,
    uint32_t width,
    uint32_t height,
    uint32_t mip_drop_levels,
    uint32_t& out_width,
    uint32_t& out_height)
{
    width  = std::clamp(width, 4U, 16384U);
    height = std::clamp(height, 4U, 16384U);
    uint32_t safe_drop = std::min(mip_drop_levels, 4U);

    if (width >= 1024U && height >= 1024U && safe_drop > 0U) {
        out_width  = std::max(4U, width >> safe_drop);
        out_height = std::max(4U, height >> safe_drop);
    } else {
        out_width  = width;
        out_height = height;
    }

    const uint32_t blocks_x = (out_width + 3U) / 4U;
    const uint32_t blocks_y = (out_height + 3U) / 4U;
    const size_t linear_bytes = static_cast<size_t>(blocks_x) * static_cast<size_t>(blocks_y) * 16U;

    std::vector<uint8_t> linear_out(linear_bytes, 0U);
    if (!swizzled_src || src_size < 16U) {
        return linear_out;
    }

    for (uint32_t by = 0; by < blocks_y; ++by) {
        for (uint32_t bx = 0; bx < blocks_x; ++bx) {
            uint32_t morton_idx = ComputeMorton2D(bx & 7U, by & 7U) +
                                  ((by >> 3U) * ((blocks_x + 7U) >> 3U) + (bx >> 3U)) * 64U;
            size_t src_off = (static_cast<size_t>(morton_idx) * 16U) % (src_size - 15U);
            size_t dst_off = (static_cast<size_t>(by) * blocks_x + bx) * 16U;
            std::memcpy(linear_out.data() + dst_off, swizzled_src + src_off, 16U);
        }
    }

    return linear_out;
}

} // namespace ps5::emu
