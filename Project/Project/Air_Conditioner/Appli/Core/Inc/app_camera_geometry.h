#ifndef APP_CAMERA_GEOMETRY_H
#define APP_CAMERA_GEOMETRY_H

#include <math.h>
#include <stdint.h>

/* Both crops use sensor-space coordinates after the sensor mirror/flip.
   Rectangles use an exclusive right/bottom edge (x + width, y + height). */
typedef struct
{
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
} app_camera_roi_t;

typedef struct
{
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
} app_camera_rect_t;

static inline uint8_t app_camera_geometry_map_box(
    const app_camera_roi_t *nn_roi, const app_camera_roi_t *display_roi,
    uint32_t display_width, uint32_t display_height,
    float cx, float cy, float width, float height, app_camera_rect_t *rect)
{
    float left;
    float top;
    float right;
    float bottom;
    uint32_t x1;
    uint32_t y1;
    uint32_t x2;
    uint32_t y2;

    if ((nn_roi == 0) || (display_roi == 0) || (rect == 0) ||
        (nn_roi->width == 0U) || (nn_roi->height == 0U) ||
        (display_roi->width == 0U) || (display_roi->height == 0U) ||
        (display_width == 0U) || (display_height == 0U) ||
        (display_width > UINT16_MAX) || (display_height > UINT16_MAX) ||
        !isfinite(cx) || !isfinite(cy) || !isfinite(width) ||
        !isfinite(height) || (width <= 0.0f) || (height <= 0.0f))
    {
        return 0U;
    }

    left = cx - width * 0.5f;
    top = cy - height * 0.5f;
    right = cx + width * 0.5f;
    bottom = cy + height * 0.5f;
    if ((right <= 0.0f) || (bottom <= 0.0f) ||
        (left >= 1.0f) || (top >= 1.0f))
    {
        return 0U;
    }
    left = fmaxf(left, 0.0f);
    top = fmaxf(top, 0.0f);
    right = fminf(right, 1.0f);
    bottom = fminf(bottom, 1.0f);

    left = ((float)nn_roi->x + left * nn_roi->width - display_roi->x) *
           display_width / display_roi->width;
    right = ((float)nn_roi->x + right * nn_roi->width - display_roi->x) *
            display_width / display_roi->width;
    top = ((float)nn_roi->y + top * nn_roi->height - display_roi->y) *
          display_height / display_roi->height;
    bottom = ((float)nn_roi->y + bottom * nn_roi->height - display_roi->y) *
             display_height / display_roi->height;
    left = fmaxf(left, 0.0f);
    top = fmaxf(top, 0.0f);
    right = fminf(right, (float)display_width);
    bottom = fminf(bottom, (float)display_height);
    if ((right <= left) || (bottom <= top))
    {
        return 0U;
    }

    x1 = (uint32_t)floorf(left);
    y1 = (uint32_t)floorf(top);
    x2 = (uint32_t)ceilf(right);
    y2 = (uint32_t)ceilf(bottom);
    rect->x = (uint16_t)x1;
    rect->y = (uint16_t)y1;
    rect->width = (uint16_t)(x2 - x1);
    rect->height = (uint16_t)(y2 - y1);
    return 1U;
}

/* Map into the actual camera viewport rather than stretching a square ROI
   across the 800x480 panel. View coordinates refer to the full LCD buffer. */
static inline uint8_t app_camera_geometry_map_view_box(
    const app_camera_roi_t *nn_roi, const app_camera_roi_t *display_roi,
    const app_camera_rect_t *view, uint32_t buffer_width,
    uint32_t buffer_height, float cx, float cy, float width, float height,
    app_camera_rect_t *rect)
{
    app_camera_rect_t local;

    if ((view == 0) || (rect == 0) ||
        ((uint32_t)view->x + view->width > buffer_width) ||
        ((uint32_t)view->y + view->height > buffer_height) ||
        (buffer_width > UINT16_MAX) || (buffer_height > UINT16_MAX) ||
        !app_camera_geometry_map_box(nn_roi, display_roi,
                                    view->width, view->height,
                                    cx, cy, width, height, &local))
    {
        return 0U;
    }
    *rect = local;
    rect->x = (uint16_t)(rect->x + view->x);
    rect->y = (uint16_t)(rect->y + view->y);
    return 1U;
}

#endif /* APP_CAMERA_GEOMETRY_H */
