# AP3216C and IR Air Conditioner Controls Implementation Plan

> **For agentic workers:** Execute the steps task-by-task with verification checkpoints.

**Goal:** Integrate onboard AP3216C light sensing and the official ALIENTEK IR remote into the Air_Conditioner application, exposing automatic presence/sleep/energy-saving behavior and logical AC controls on the LCD.

**Architecture:** A pure climate state module handles light hysteresis, occupancy timeout, automatic/manual modes, and remote action mapping. The existing low-priority input thread performs blocking sensor reads and remote polling, then publishes a mutex-protected display snapshot. TIM1 callbacks only feed the official NEC decoder state.

**Tech Stack:** STM32N6 HAL, ThreadX, ALIENTEK BSP drivers, host GCC tests, STM32CubeIDE Release/Debug builds.

---

### Task 1: Define and test pure climate policy

**Files:**
- Create: `Project/Project/Air_Conditioner/Appli/Core/Inc/app_climate_logic.h`
- Create: `Project/Project/Air_Conditioner/Appli/Core/Src/app_climate_logic.c`
- Create/modify: `tests/test_app_climate_logic.c`
- Modify: `Appli/Core/Inc/app_input_logic.h/.c` for IR action mapping

- [x] Write failing tests for ALS hysteresis, occupancy state, 30-minute shutdown, manual toggles, and official remote command mapping.
- [x] Run host tests and confirm expected failures.
- [x] Implement the pure state machine and action mapper with bounded temperatures/fan values.
- [x] Cross-compile the pure logic with `arm-none-eabi-gcc -Wall -Wextra -Werror`.

### Task 2: Add AP3216C and TIM1 hardware initialization

**Files:**
- Modify: `Appli/Core/Src/main.c`
- Modify: `Appli/Core/Src/stm32n6xx_hal_msp.c`
- Modify: `Appli/Core/Src/stm32n6xx_it.c`

- [x] Copy the official I2C4 timing/pin and TIM1 PE9 input-capture configuration from Projects/24_AP3216C and 28_Remote.
- [x] Set GPIO security attributes for I2C4 pins and PE9.
- [x] Add initialization calls before `app_run`.
- [x] Add only decoder-safe TIM1 callbacks/IRQ wrappers.

### Task 3: Connect policy to application and LCD

**Files:**
- Modify: `Appli/Core/Src/app.c`
- Modify: `Appli/Core/Inc/app_config.h`

- [x] Initialize AP3216C/remote from the input thread.
- [x] Poll light/IR every 500 ms, apply policy and actions under the display mutex.
- [x] Remove CPU load/Inference labels and show light, occupancy, fan, swing, lamp, sleep, eco, and power.
- [x] Keep camera/FPS/people and DHT11 information.

### Task 4: Update project metadata and docs

**Files:**
- Modify: `STM32CubeIDE/Appli/.project`
- Modify: `STM32CubeIDE/Appli/.cproject`
- Modify: `README.md`
- Modify: existing design/burn docs

- [x] Add AP3216C/REMOTE source links and include paths.
- [x] Document remote codes, automatic policy, and logical-only relay limitation.

### Task 5: Build and safety review

- [x] Attempt host tests; MSYS2 UCRT runtime exits before compilation, so target cross-compile is the available pure-module check.
- [x] Run STM32CubeIDE Release and Debug builds.
- [x] Restore generated environment metadata noise if changed.
- [x] Generate and verify `Binary/appli.hex`.
- [x] Execute the embedded safety checklist review; no new fatal/high issues remain.
