#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_ui_pixels.h"

#define PIXEL_GUARD (0xA55AU)
#define TEST_COLORS (65536U)

static uint32_t random_state = 0x6E647202U;

static uint8_t random_byte(void)
{
    random_state = random_state * 1664525U + 1013904223U;
    return (uint8_t)(random_state >> 24U);
}

static uint16_t reference_pixel(const uint8_t *bgra)
{
    /* Separate component arithmetic, independent of packed/vector masks. */
    return (uint16_t)((bgra[3] / 16U) * 4096U +
                      (bgra[2] / 16U) * 256U +
                      (bgra[1] / 16U) * 16U + bgra[0] / 16U);
}

static void check_all_colors(void)
{
    uint8_t *storage = malloc(TEST_COLORS * 4U + 2U);
    uint16_t *output = malloc((TEST_COLORS + 2U) * sizeof(*output));
    assert(storage != NULL && output != NULL);
    uint8_t *source = storage + 1U; /* Deliberately unaligned BGRA source. */

    for (unsigned pass = 0U; pass < 3U; pass++)
    {
        for (uint32_t color = 0U; color < TEST_COLORS; color++)
        {
            for (unsigned component = 0U; component < 4U; component++)
            {
                uint8_t low = pass == 0U ? 0U :
                              pass == 1U ? 15U : random_byte() % 16U;
                source[color * 4U + component] = (uint8_t)(
                    (((color >> (component * 4U)) & 15U) * 16U) + low);
            }
        }
        storage[0] = 0x91U;
        storage[TEST_COLORS * 4U + 1U] = 0x29U;
        output[0] = PIXEL_GUARD;
        output[TEST_COLORS + 1U] = PIXEL_GUARD;
        app_ui_pixels_convert(output + 1U, TEST_COLORS, source,
                              TEST_COLORS, 1U);
        for (uint32_t color = 0U; color < TEST_COLORS; color++)
        {
            assert(output[color + 1U] == (uint16_t)color);
            assert(output[color + 1U] == reference_pixel(source + color * 4U));
        }
        assert(output[0] == PIXEL_GUARD);
        assert(output[TEST_COLORS + 1U] == PIXEL_GUARD);
        assert(storage[0] == 0x91U);
        assert(storage[TEST_COLORS * 4U + 1U] == 0x29U);
    }
    free(output);
    free(storage);
}

static void check_layouts(void)
{
    static const uint32_t widths[] = {
        1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 15U, 16U, 17U, 799U, 800U
    };
    static const uint32_t heights[] = {1U, 2U, 7U, 20U};
    for (unsigned wi = 0U; wi < sizeof(widths) / sizeof(widths[0]); wi++)
    {
        for (unsigned hi = 0U; hi < sizeof(heights) / sizeof(heights[0]); hi++)
        {
            for (unsigned misalignment = 0U; misalignment < 4U; misalignment++)
            {
                uint32_t width = widths[wi];
                uint32_t height = heights[hi];
                uint32_t stride = width + 5U;
                size_t source_size = (size_t)width * height * 4U;
                size_t output_pixels = (size_t)stride * height + 2U;
                uint8_t *storage = malloc(source_size + 4U);
                uint8_t *before = malloc(source_size);
                uint16_t *output = malloc(output_pixels * sizeof(*output));
                assert(storage != NULL && before != NULL && output != NULL);
                uint8_t *source = storage + misalignment;
                for (size_t i = 0U; i < source_size; i++) source[i] = random_byte();
                memcpy(before, source, source_size);
                for (size_t i = 0U; i < output_pixels; i++) output[i] = PIXEL_GUARD;
                app_ui_pixels_convert(output + 1U, stride, source, width, height);
                assert(memcmp(before, source, source_size) == 0);
                assert(output[0] == PIXEL_GUARD);
                assert(output[output_pixels - 1U] == PIXEL_GUARD);
                for (uint32_t row = 0U; row < height; row++)
                {
                    for (uint32_t column = 0U; column < stride; column++)
                    {
                        uint16_t actual = output[1U + (size_t)row * stride + column];
                        if (column < width)
                            assert(actual == reference_pixel(
                                source + ((size_t)row * width + column) * 4U));
                        else
                            assert(actual == PIXEL_GUARD);
                    }
                }
                free(output);
                free(before);
                free(storage);
            }
        }
    }
}

static void check_rejected_inputs(void)
{
    uint8_t source[16] = {0U};
    uint16_t output[8];
    for (unsigned i = 0U; i < 8U; i++) output[i] = PIXEL_GUARD;
    app_ui_pixels_convert(NULL, 4U, source, 4U, 1U);
    app_ui_pixels_convert(output, 4U, NULL, 4U, 1U);
    app_ui_pixels_convert(output, 4U, source, 0U, 1U);
    app_ui_pixels_convert(output, 4U, source, 4U, 0U);
    app_ui_pixels_convert(output, 3U, source, 4U, 1U);
    app_ui_pixels_convert(output, UINT32_MAX, source, UINT32_MAX, 1U);
    app_ui_pixels_convert(output, UINT32_MAX, source, 1U, UINT32_MAX);
    for (unsigned i = 0U; i < 8U; i++) assert(output[i] == PIXEL_GUARD);
#ifdef APP_UI_PIXELS_EXPECT_HELIUM
    assert(app_ui_pixels_uses_helium() == 1U);
#else
    assert(app_ui_pixels_uses_helium() == 0U);
#endif
}

int main(void)
{
    check_all_colors();
    check_layouts();
    check_rejected_inputs();
    puts("PASS: 65536 ARGB4444 colors, alpha, low-nibble noise, row strides, unaligned source and tail guards.");
    return 0;
}
