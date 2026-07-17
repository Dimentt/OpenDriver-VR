#include "mf_color_convert.h"

#include <algorithm>

namespace opendriver::driver::video::mfcolor {

namespace {

uint8_t ClampToByte(int value) {
    return static_cast<uint8_t>(std::clamp(value, 0, 255));
}

enum class PixelLayout {
    Bgra,
    Rgba,
    Unsupported,
};

PixelLayout ResolvePixelLayout(DXGI_FORMAT format) {
    switch (format) {
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8X8_UNORM:
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
        return PixelLayout::Bgra;
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        return PixelLayout::Rgba;
    default:
        return PixelLayout::Unsupported;
    }
}

inline void ReadRgbPixel(const uint8_t* pixel, PixelLayout layout, int& r, int& g, int& b) {
    if (layout == PixelLayout::Bgra) {
        b = pixel[0];
        g = pixel[1];
        r = pixel[2];
    } else {
        r = pixel[0];
        g = pixel[1];
        b = pixel[2];
    }
}

} // namespace

void ConvertPackedRgbToNV12(const uint8_t* source,
                            uint32_t source_stride,
                            uint32_t width,
                            uint32_t height,
                            DXGI_FORMAT format,
                            std::vector<uint8_t>& output) {
    const PixelLayout layout = ResolvePixelLayout(format);
    if (layout == PixelLayout::Unsupported) {
        output.clear();
        return;
    }

    const size_t y_plane_size = static_cast<size_t>(width) * height;
    const size_t uv_plane_size = y_plane_size / 2;
    output.resize(y_plane_size + uv_plane_size);

    uint8_t* y_plane = output.data();
    uint8_t* uv_plane = output.data() + y_plane_size;

    for (uint32_t y = 0; y < height; y += 2) {
        const uint8_t* row0 = source + (static_cast<size_t>(y) * source_stride);
        const uint8_t* row1 = row0 + source_stride;
        const size_t row0_y_index = static_cast<size_t>(y) * width;
        const size_t row1_y_index = row0_y_index + width;
        const size_t uv_row_index = (static_cast<size_t>(y) / 2) * width;

        for (uint32_t x = 0; x < width; x += 2) {
            int u_accumulator = 0;
            int v_accumulator = 0;

            for (uint32_t dy = 0; dy < 2; ++dy) {
                const uint8_t* row = (dy == 0) ? row0 : row1;
                const size_t y_index = (dy == 0) ? row0_y_index : row1_y_index;

                for (uint32_t dx = 0; dx < 2; ++dx) {
                    const size_t sample_x = static_cast<size_t>(x + dx);
                    const uint8_t* pixel = row + (sample_x * 4);

                    int r = 0;
                    int g = 0;
                    int b = 0;
                    ReadRgbPixel(pixel, layout, r, g, b);

                    const int y_value = ((66 * r + 129 * g + 25 * b + 128) >> 8) + 16;
                    const int u_value = ((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128;
                    const int v_value = ((112 * r - 94 * g - 18 * b + 128) >> 8) + 128;

                    y_plane[y_index + sample_x] = ClampToByte(y_value);
                    u_accumulator += u_value;
                    v_accumulator += v_value;
                }
            }

            const size_t uv_index = uv_row_index + x;
            uv_plane[uv_index] = ClampToByte(u_accumulator / 4);
            uv_plane[uv_index + 1] = ClampToByte(v_accumulator / 4);
        }
    }
}

} // namespace opendriver::driver::video::mfcolor
