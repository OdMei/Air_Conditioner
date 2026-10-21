#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "app_camera_geometry.h"
#include "app_ui_port.h"

static void test_matching_square_viewport(void)
{
    const app_camera_roi_t nn = {324U, 0U, 1944U, 1944U};
    const app_camera_rect_t view = {
        LCD_CAMERA_VIEW_X, LCD_CAMERA_VIEW_Y,
        LCD_CAMERA_VIEW_WIDTH, LCD_CAMERA_VIEW_HEIGHT
    };
    const app_camera_rect_t outside = {400U, 0U, 480U, 480U};
    app_camera_rect_t rect;

    assert(LCD_CAMERA_VIEW_WIDTH == LCD_CAMERA_VIEW_HEIGHT);
    assert((LCD_CAMERA_WIDTH * 2U) % 32U == 0U);
    assert((LCD_CAMERA_VIEW_X * 2U) % 32U == 0U);
    assert(LCD_CAMERA_VIEW_X * 2U == 320U);
    assert(LCD_CAMERA_WIDTH * 2U == 1600U);
    for (uint32_t y = 0U; y < LCD_CAMERA_VIEW_HEIGHT; y++)
    {
        uint32_t first = ((y + LCD_CAMERA_VIEW_Y) * LCD_CAMERA_WIDTH +
                          LCD_CAMERA_VIEW_X) * 2U;
        uint32_t last = first + LCD_CAMERA_VIEW_WIDTH * 2U;
        assert(first % 32U == 0U);
        assert(first >= y * LCD_CAMERA_WIDTH * 2U);
        assert(last <= (y + 1U) * LCD_CAMERA_WIDTH * 2U);
        assert(last <= LCD_CAMERA_WIDTH * LCD_CAMERA_HEIGHT * 2U);
    }

    assert(app_camera_geometry_map_view_box(&nn, &nn, &view, 800U, 480U,
                                           0.5f, 0.5f, 1.0f, 1.0f, &rect));
    assert(rect.x == 160U && rect.y == 0U &&
           rect.width == 480U && rect.height == 480U);
    assert(app_camera_geometry_map_view_box(&nn, &nn, &view, 800U, 480U,
                                           0.5f, 0.5f, 0.5f, 0.5f, &rect));
    assert(rect.x == 280U && rect.y == 120U &&
           rect.width == 240U && rect.height == 240U);
    /* A person at the top of NN input is now also visible in the camera. */
    assert(app_camera_geometry_map_view_box(&nn, &nn, &view, 800U, 480U,
                                           0.5f, 0.0f, 0.125f, 0.125f, &rect));
    assert(rect.y == 0U && rect.height == 30U);
    assert(app_camera_geometry_map_view_box(&nn, &nn, &view, 800U, 480U,
                                           0.0f, 0.0f, 0.5f, 0.5f, &rect));
    assert(rect.x == 160U && rect.y == 0U &&
           rect.width == 120U && rect.height == 120U);
    assert(app_camera_geometry_map_view_box(&nn, &nn, &view, 800U, 480U,
                                           1.0f, 1.0f, 0.5f, 0.5f, &rect));
    assert(rect.x == 520U && rect.y == 360U &&
           rect.width == 120U && rect.height == 120U);
    assert(!app_camera_geometry_map_view_box(&nn, &nn, &outside, 800U, 480U,
                                            0.5f, 0.5f, 0.5f, 0.5f, &rect));
    assert(!app_camera_geometry_map_view_box(&nn, &nn, NULL, 800U, 480U,
                                            0.5f, 0.5f, 0.5f, 0.5f, &rect));
    assert(!app_camera_geometry_map_view_box(&nn, &nn, &view, 800U, 480U,
                                            0.5f, 0.5f, 0.5f, 0.5f, NULL));
    for (int x = -10; x <= 110; ++x)
    {
        for (int y = -10; y <= 110; ++y)
        {
            if (app_camera_geometry_map_view_box(&nn, &nn, &view, 800U, 480U,
                                                x / 100.0f, y / 100.0f,
                                                0.3f, 0.5f, &rect))
            {
                assert(rect.width > 0U && rect.height > 0U);
                assert(rect.x >= 160U);
                assert((uint32_t)rect.x + rect.width <= 640U);
                assert((uint32_t)rect.y + rect.height <= 480U);
            }
        }
    }
}

int main(void)
{
    test_matching_square_viewport();
    const app_camera_roi_t nn = {324U, 0U, 1944U, 1944U};
    const app_camera_roi_t display = {0U, 195U, 2592U, 1555U};
    const app_camera_roi_t identity = {0U, 0U, 100U, 100U};
    const app_camera_roi_t zero = {0U, 0U, 0U, 100U};
    app_camera_rect_t rect;

    assert(app_camera_geometry_map_box(&nn, &display, 800U, 480U,
                                      0.5f, 0.5f, 1.0f, 1.0f, &rect));
    assert(rect.x == 100U && rect.y == 0U);
    assert(rect.width == 600U && rect.height == 480U);
    assert(app_camera_geometry_map_box(&nn, &display, 800U, 480U,
                                      0.5f, 0.5f, 0.5f, 0.5f, &rect));
    assert(rect.x == 250U && rect.y == 89U);
    assert(rect.width == 300U && rect.height == 301U);
    assert(!app_camera_geometry_map_box(&nn, &display, 800U, 480U,
                                       0.5f, 0.0f, 0.1f, 0.1f, &rect));

    assert(app_camera_geometry_map_box(&identity, &identity, 100U, 100U,
                                      0.5f, 0.5f, 0.5f, 0.5f, &rect));
    assert(rect.x == 25U && rect.y == 25U);
    assert(rect.width == 50U && rect.height == 50U);
    assert(app_camera_geometry_map_box(&identity, &identity, 100U, 100U,
                                      0.0f, 0.0f, 0.5f, 0.5f, &rect));
    assert(rect.x == 0U && rect.y == 0U);
    assert(rect.width == 25U && rect.height == 25U);
    assert(app_camera_geometry_map_box(&identity, &identity, 100U, 100U,
                                      1.0f, 1.0f, 0.5f, 0.5f, &rect));
    assert(rect.x == 75U && rect.y == 75U);
    assert(rect.width == 25U && rect.height == 25U);
    assert(!app_camera_geometry_map_box(&identity, &identity, 100U, 100U,
                                       -1.0f, 0.5f, 0.1f, 0.1f, &rect));
    assert(!app_camera_geometry_map_box(&zero, &identity, 100U, 100U,
                                       0.5f, 0.5f, 0.5f, 0.5f, &rect));
    assert(!app_camera_geometry_map_box(&identity, &identity, 100U, 100U,
                                       NAN, 0.5f, 0.5f, 0.5f, &rect));
    assert(!app_camera_geometry_map_box(&identity, &identity, 100U, 100U,
                                       0.5f, INFINITY, 0.5f, 0.5f, &rect));
    assert(!app_camera_geometry_map_box(&identity, &identity, 100U, 100U,
                                       0.5f, 0.5f, -0.5f, 0.5f, &rect));
    assert(!app_camera_geometry_map_box(&identity, &identity, 100U, 100U,
                                       0.5f, 0.5f, 0.5f, 0.5f, NULL));

    /* Sweep predicted boxes through all display boundaries. */
    for (int x = -10; x <= 110; ++x)
    {
        for (int y = -10; y <= 110; ++y)
        {
            if (app_camera_geometry_map_box(&nn, &display, 800U, 480U,
                                           x / 100.0f, y / 100.0f,
                                           0.3f, 0.5f, &rect))
            {
                assert(rect.width > 0U && rect.height > 0U);
                assert((uint32_t)rect.x + rect.width <= 800U);
                assert((uint32_t)rect.y + rect.height <= 480U);
            }
        }
    }
    puts("PASS: matching camera/NN square ROI in centered 480x480 viewport, legacy crop mapping, clipping and invalid boxes.");
    return 0;
}
