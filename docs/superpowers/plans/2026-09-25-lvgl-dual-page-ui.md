# LVGL Dual-Page UI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add LVGL 9.x user/debug pages with a touch `lv_indev` while preserving the camera background layer.

**Architecture:** LVGL owns LTDC layer 1 and is called only from the ThreadX display task. The existing touch polling thread publishes a snapshot; LVGL reads that snapshot without I2C access. The camera continues to own LTDC layer 0 and its rotating external-RAM buffers.

**Tech Stack:** LVGL 9.x, STM32N6 HAL/LTDC, ThreadX, existing FT5206/GT9xxx BSP, STM32CubeIDE GNU Make.

---

### Task 1: Vendor LVGL and establish configuration

**Files:**
- Create: `Air_Conditioner/Middlewares/LVGL/` from the official LVGL 9.x source release
- Create: `Air_Conditioner/Project/Project/Air_Conditioner/Appli/Core/Inc/lv_conf.h`
- Modify: `Air_Conditioner/Project/Project/Air_Conditioner/STM32CubeIDE/Appli/Debug/*subdir.mk`, `sources.mk`, and include settings
- Modify: matching Release build metadata

- [ ] Obtain the pinned LVGL 9.x source release and record its version in the project README.
- [ ] Configure `LV_COLOR_DEPTH 16`, disable unused demos/examples, enable standard widgets, and use `LV_MEM_CUSTOM 0` with the project allocator defaults.
- [ ] Add LVGL core, draw, display, indev, widget, font, and platform source directories to both CubeIDE configurations and add `Core/Inc` plus `Middlewares/LVGL` include paths.
- [ ] Add a source-level check that the configured LVGL version and required source directories exist.
- [ ] Build the unmodified application to catch source-list and include-path errors before UI code is added.

### Task 2: Add the LVGL display and input port

**Files:**
- Create: `Air_Conditioner/Project/Project/Air_Conditioner/Appli/Core/Inc/app_ui.h`
- Create: `Air_Conditioner/Project/Project/Air_Conditioner/Appli/Core/Src/app_ui.c`
- Modify: `Air_Conditioner/Project/Project/Air_Conditioner/Appli/Core/Inc/app_touch.h`
- Modify: `Air_Conditioner/Project/Project/Air_Conditioner/Appli/Core/Src/app_touch.c`
- Modify: `Air_Conditioner/Project/Project/Air_Conditioner/Appli/Core/Src/app.c`

- [ ] Define `app_ui_init`, `app_ui_process`, `app_ui_set_snapshot`, and a UI mode enum in `app_ui.h`.
- [ ] Create a partial RGB565 LVGL draw buffer in internal SRAM with 32-byte alignment and a flush callback that clips, copies to the active layer-1 target, cleans D-cache, and calls `lv_display_flush_ready`.
- [ ] Register an LVGL display at 800x480 and an `LV_INDEV_TYPE_POINTER` device whose read callback copies the latest `app_touch_state_t`, clamps coordinates, and reports released input when no controller is available.
- [ ] Keep all LVGL calls in `dp_thread_entry`; call `lv_timer_handler` with a bounded period and publish data snapshots under the existing display mutex.
- [ ] Add a touch snapshot test for ready/released/pressed coordinate mapping and bounds clamping.

### Task 3: Build the user and debug pages

**Files:**
- Modify: `Air_Conditioner/Project/Project/Air_Conditioner/Appli/Core/Src/app_ui.c`
- Modify: `Air_Conditioner/Project/Project/Air_Conditioner/Appli/Core/Inc/app_ui.h`

- [ ] Build the User page with labels for temperature, humidity, presence/state, target temperature, fan, swing, lamp, and energy saving.
- [ ] Add touch buttons for power, target temperature up/down, fan selection, swing, sleep, eco, and page switching; route events to the existing climate action API through an event callback supplied by `app.c`.
- [ ] Build the Debug page with camera-overlay-safe foreground labels for FPS, people, environment, state, target, and four relay states, plus a page-switch button.
- [ ] Ensure widgets are created once and updated from snapshots instead of being recreated on every frame.
- [ ] Add a UI source check that both pages and all required data labels/actions are present.

### Task 4: Integrate application state and relay display

**Files:**
- Modify: `Air_Conditioner/Project/Project/Air_Conditioner/Appli/Core/Src/app.c`
- Modify: `Air_Conditioner/Project/Project/Air_Conditioner/Appli/Core/Inc/app_climate_logic.h` if relay state fields are missing
- Modify: `Air_Conditioner/README.md`

- [ ] Replace the low-level foreground text renderer in the display thread with `app_ui_set_snapshot` and `app_ui_process`, retaining the camera frame callbacks unchanged.
- [ ] Connect UI actions to `app_climate_apply_action` and keep the existing physical key/remote paths intact.
- [ ] Expose four relay state values from the control model for display only; do not assign unverified GPIO pins.
- [ ] Document the page behavior, input mapping, LVGL version, and the unresolved relay pin mapping.

### Task 5: Verification and hardware bring-up

**Files:**
- Modify: `Air_Conditioner/tests/test_touch_diagnostics.ps1` or add a focused LVGL source check

- [ ] Run the LVGL source/config/UI static checks and existing climate/touch tests.
- [ ] Build Release and Debug with CubeIDE Make and confirm no compiler warnings or link failures.
- [ ] Inspect `arm-none-eabi-size` output for external-RAM and internal-RAM fit.
- [ ] Generate `Binary/appli.hex` from the verified Release binary at `0x70100400`.
- [ ] On hardware verify User startup, touch button operation, page switching, Debug values, four relay labels, and live camera background.
