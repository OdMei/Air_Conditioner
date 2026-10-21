# LVGL Dual-Page UI Design

## Goal

Integrate LVGL 9.x into the STM32N647 air-conditioner application, adapt the
existing capacitive touch driver as an LVGL input device, and provide user and
debug pages on the LCD.

## Current Project Context

- The application uses ThreadX and a 800x480 RGB LCD.
- LTDC layer 0 is the camera RGB565 background. Camera capture uses rotating
  buffers in external RAM and changes the layer address on completed frames.
- LTDC layer 1 currently renders diagnostic text into two ARGB4444 buffers
  through `app_lcd.c` and the STM32 LCD utility.
- `app_touch.c` polls the supported FT5206 or GT9xxx driver and returns
  coordinates in `app_touch_state_t`.
- No LVGL source, configuration, or port exists in the repository.

## Architecture

Vendor LVGL 9.x under `Air_Conditioner/Middlewares/LVGL` and add it to the
CubeIDE Release and Debug source lists. Application-owned UI code will live in
`Appli/Core/Src/app_ui.c` with an `app_ui.h` interface; it will own LVGL
initialization, display/input registration, page widgets, event handling, and
periodic refresh. The application display thread will call the UI update entry
point from ThreadX task context.

The existing camera background layer and camera capture buffers remain
unchanged. LVGL will render only to LTDC layer 1. The initial implementation
will use a partial RGB565 draw buffer in internal SRAM and flush rendered
rectangles to the existing LVGL foreground layer through the LCD pixel API,
with explicit cache maintenance and area clipping. This avoids a second full
framebuffer allocation in external RAM and preserves camera buffer rotation.

The LVGL pointer input device will read `app_touch_get_state()`. The input
thread remains the only task polling the touch controller; it will publish a
snapshot protected by a ThreadX mutex. The LVGL callback will only copy that
snapshot and map pressed/released state and coordinates. Coordinates will be
clamped to 800x480 after confirming the BSP's panel orientation.

## Pages and Interaction

- Startup opens the **User** page.
- A visible page-switch button toggles between **User** and **Debug** pages.
- The **User** page shows temperature, humidity, operating/presence state,
  target temperature, power, fan speed, swing, lamp, and energy-saving state.
  Touch controls expose power, target temperature, fan speed, swing, sleep,
  and energy-saving actions through the existing climate action API.
- The **Debug** page keeps the camera visible on layer 0 and presents FPS,
  people count, temperature/humidity, state, target temperature, and four
  relay states on layer 1.
- Existing relay status is displayed from application state. This change does
  not invent relay GPIO assignments or energize hardware whose mapping has not
  been verified.

## Data Flow and Threading

The input thread polls touch and sensor/remote state, then publishes the
current snapshot under a mutex. The display thread owns all LVGL calls,
including `lv_timer_handler`, widget updates, and page transitions. The
`lv_indev` read callback only snapshots input state and does not perform I2C
transactions. The LVGL flush callback performs bounded LCD memory updates and
calls `lv_display_flush_ready()` after the rectangle has been copied and cache
maintenance is complete.

## Error Handling

- LVGL initialization or display/input registration failure is recorded as a
  UI initialization error and shown with the existing low-level LCD text path
  if it remains available during bring-up.
- An unavailable touch controller is reported as released input; the UI
  remains usable only through any existing physical controls.
- Invalid coordinates are clamped to the display bounds.
- LVGL API calls are serialized in the display thread; the driver callback
  does not call LVGL.

## Verification

- Add source-level regression checks for LVGL source inclusion, display
  registration, touch input registration, page switch handlers, and relay
  fields.
- Build Release and Debug configurations and inspect memory usage.
- On hardware verify startup on User page, touch transitions, page switching,
  user controls, debug values, and that camera background remains live while
  LVGL refreshes the foreground.

## Open Hardware Dependency

Four relay output GPIO mappings are not verified by the current project
inspection. The debug UI will read relay states from the climate/control model;
hardware-driving relay integration remains dependent on confirming the board
pin mapping and active polarity.
