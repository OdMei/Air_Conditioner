#include "app_ui_pixels.h"

#include <stddef.h>
#include <string.h>

#if !defined(APP_UI_PIXELS_DISABLE_HELIUM) && \
    defined(__ARM_FEATURE_MVE) && ((__ARM_FEATURE_MVE & 1) != 0) && \
    defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#include <arm_mve.h>
#define APP_UI_PIXELS_HELIUM (1U)
#else
#define APP_UI_PIXELS_HELIUM (0U)
#endif

static uint16_t app_ui_pixels_scalar(const uint8_t *bgra)
{
    return (uint16_t)(((uint16_t)(bgra[3] & 0xF0U) << 8U) |
                      ((uint16_t)(bgra[2] & 0xF0U) << 4U) |
                      (uint16_t)(bgra[1] & 0xF0U) |
                      ((uint16_t)bgra[0] >> 4U));
}

void app_ui_pixels_convert(uint16_t *dst, uint32_t dst_stride_pixels,
                           const uint8_t *src_bgra,
                           uint32_t width, uint32_t height)
{
    uint32_t row;
    uint64_t source_bytes;
    uint64_t destination_pixels;

    if ((dst == NULL) || (src_bgra == NULL) || (width == 0U) ||
        (height == 0U) || (dst_stride_pixels < width) ||
        (width > UINT32_MAX / 4U))
    {
        return;
    }
    source_bytes = (uint64_t)width * height * 4U;
    destination_pixels = (uint64_t)(height - 1U) * dst_stride_pixels + width;
    if ((source_bytes > SIZE_MAX) ||
        (destination_pixels > SIZE_MAX / sizeof(*dst)))
    {
        return;
    }

    for (row = 0U; row < height; row++)
    {
        const uint8_t *source = src_bgra;
        uint16_t *target = dst;
        uint32_t remaining = width;

#if APP_UI_PIXELS_HELIUM
        /* Four complete pixels per iteration: no predication, tail overread
         * or dependency on the source's alignment/effective type. The memcpy
         * becomes a vector byte load with the installed Cortex-M55 compiler.
         * Compress BGRA's high nibbles into each lane's low halfword. */
        while (remaining >= 4U)
        {
            uint32x4_t pixels;
            memcpy(&pixels, source, sizeof(pixels));
            pixels = vandq_u32(vshrq_n_u32(pixels, 4),
                               vdupq_n_u32(0x0F0F0F0FU));
            pixels = vandq_u32(vorrq_u32(pixels, vshrq_n_u32(pixels, 4)),
                               vdupq_n_u32(0x00FF00FFU));
            pixels = vorrq_u32(pixels, vshrq_n_u32(pixels, 8));
            vstrhq_u32(target, pixels);
            source += 16U;
            target += 4U;
            remaining -= 4U;
        }
#endif
        while (remaining != 0U)
        {
            *target++ = app_ui_pixels_scalar(source);
            source += 4U;
            remaining--;
        }
        /* Avoid forming pointers beyond the final allocated row. */
        if (row + 1U < height)
        {
            src_bgra += (size_t)width * 4U;
            dst += dst_stride_pixels;
        }
    }
}

uint8_t app_ui_pixels_uses_helium(void)
{
    return APP_UI_PIXELS_HELIUM;
}
