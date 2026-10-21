#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* The script inserts the unmodified production function bodies below.
   This seam models hardware ownership, partial ISP initialization and errors. */
#define CMW_ERROR_NONE 0
#define CMW_ERROR_WRONG_PARAM (-1)
#define CMW_ERROR_PERIPH_FAILURE (-2)
#define CMW_ERROR_COMPONENT_FAILURE (-3)
#define APP_CAMERA_STATUS_TIMEOUT (-1003)
#define APP_CAMERA_STOP_TIMEOUT_MS 100U
#define DCMIPP_PIPE0 0U
#define DCMIPP_PIPE1 1U
#define DCMIPP_PIPE2 2U
#define HAL_DCMIPP_PIPE_STATE_RESET 0U
#define HAL_DCMIPP_PIPE_STATE_READY 1U
#define HAL_DCMIPP_PIPE_STATE_BUSY 2U
#define DCMIPP_P1FCTCR_CPTREQ 1U
#define DCMIPP_P2FCTCR_CPTREQ 1U
#define DCMIPP_CMSR1_P1CPTACT 1U
#define DCMIPP_CMSR1_P2CPTACT 2U
#define DCMIPP_P1FSCR_PIPEN 1U
#define DCMIPP_P2FSCR_PIPEN 1U
#define DCMIPP_P1PPCR_DBM 1U
#define DCMIPP_P2PPCR_DBM 1U
#define DCMIPP_IT_PIPE1_FRAME 0x10U
#define DCMIPP_IT_PIPE1_VSYNC 0x20U
#define DCMIPP_IT_PIPE1_LINE 0x40U
#define DCMIPP_IT_PIPE1_OVR 0x80U
#define DCMIPP_IT_PIPE2_FRAME 0x100U
#define DCMIPP_IT_PIPE2_VSYNC 0x200U
#define DCMIPP_IT_PIPE2_LINE 0x400U
#define DCMIPP_IT_PIPE2_OVR 0x800U
#define CLEAR_BIT(reg, mask) ((reg) &= ~(mask))
#define __HAL_DCMIPP_DISABLE_IT(h, mask) ((h)->Instance->CMIER &= ~(mask))
#define __HAL_DCMIPP_CLEAR_FLAG(h, mask) ((h)->Instance->CMFCR = (mask))
#define ISP_OK 0
#define IMX335_OK 0
#define HAL_OK 0
typedef struct {
    uint32_t P1FCTCR, P2FCTCR, P1FSCR, P2FSCR, P1PPCR, P2PPCR;
    uint32_t CMSR1, CMIER, CMFCR;
} test_dcmipp_registers_t;
typedef struct {
    test_dcmipp_registers_t *Instance;
    uint32_t PipeState[3];
} DCMIPP_HandleTypeDef;
typedef struct { void *algorithm; void *hDcmipp; } test_isp_t;
typedef struct { uint32_t initialized; } test_sensor_t;
typedef struct {
    test_isp_t hIsp;
    test_sensor_t ctx_driver;
} CMW_IMX335_t;

static int32_t CMW_IMX335_DeInit(void *io_ctx);
static DCMIPP_HandleTypeDef hcamera_dcmipp;
static struct { int32_t (*DeInit)(void *); } Camera_Drv;
static union { CMW_IMX335_t imx335; } camera_bsp;
static struct { unsigned unused; } camera_conf;
static int is_camera_init, is_camera_started, is_pipe1_2_shared;
static unsigned hardware_resets, sensor_deinits, isp_deinits, powerdowns;
static unsigned sleep_count, stop_complete_after, test_tick, irq_mask;
static unsigned isp_fails;
static unsigned vsync_calls;

static int HAL_DCMIPP_DeInit(DCMIPP_HandleTypeDef *handle)
{
    hardware_resets++;
    memset(handle->Instance, 0, sizeof(*handle->Instance));
    memset(handle->PipeState, 0, sizeof(handle->PipeState));
    return HAL_OK;
}
static int ISP_DeInit(test_isp_t *isp)
{
    assert(isp->algorithm != NULL);
    isp_deinits++;
    isp->algorithm = NULL;
    return isp_fails ? -1 : ISP_OK;
}
static int IMX335_DeInit(test_sensor_t *sensor)
{ sensor_deinits++; sensor->initialized = 0U; return IMX335_OK; }
static void ISP_IncDumpFrameId(test_isp_t *isp)
{ assert(isp->algorithm && isp->hDcmipp); vsync_calls++; }
static void ISP_IncMainFrameId(test_isp_t *isp)
{ assert(isp->algorithm && isp->hDcmipp); vsync_calls++; }
static void ISP_IncAncillaryFrameId(test_isp_t *isp)
{ assert(isp->algorithm && isp->hDcmipp); vsync_calls++; }
static void ISP_GatherStatistics(test_isp_t *isp)
{ assert(isp->algorithm && isp->hDcmipp); vsync_calls++; }
static void CMW_CAMERA_PwrDown(void) { powerdowns++; }
static DCMIPP_HandleTypeDef *CMW_CAMERA_GetDCMIPPHandle(void)
{ return &hcamera_dcmipp; }
static uint32_t HAL_GetTick(void) { return test_tick; }
static uint32_t __get_PRIMASK(void) { return irq_mask; }
static void __disable_irq(void) { irq_mask = 1U; }
static void __set_PRIMASK(uint32_t state) { irq_mask = state; }
static void tx_thread_sleep(unsigned long ticks)
{
    assert(irq_mask == 0U);
    sleep_count++;
    test_tick += (uint32_t)ticks;
    if (stop_complete_after && sleep_count >= stop_complete_after)
        hcamera_dcmipp.Instance->CMSR1 = 0U;
}

/* GENERATED_PORT_FUNCTIONS */

static void configure_partial(test_dcmipp_registers_t *regs)
{
    memset(regs, 0, sizeof(*regs));
    hcamera_dcmipp.Instance = regs;
    Camera_Drv.DeInit = CMW_IMX335_DeInit;
    camera_bsp.imx335.ctx_driver.initialized = 1U;
}

int main(void)
{
    test_dcmipp_registers_t regs;

    /* Deinit before any initialization and repeat calls are harmless. */
    assert(CMW_CAMERA_DeInit() == CMW_ERROR_NONE);
    assert(CMW_CAMERA_DeInit() == CMW_ERROR_NONE);
    assert(hardware_resets == 0U && isp_deinits == 0U);
    assert(is_camera_init == 0 && is_camera_started == 0);

    /* Probe/configure succeeded, ISP never started: no null-list deinit. */
    configure_partial(&regs);
    is_camera_init = 1;
    is_camera_started = 0;
    is_pipe1_2_shared = 1;
    assert(CMW_CAMERA_DeInit() == CMW_ERROR_NONE);
    assert(hardware_resets == 1U && sensor_deinits == 1U && isp_deinits == 0U);
    assert(is_camera_init == 0 && is_camera_started == 0 && is_pipe1_2_shared == 0);
    assert(Camera_Drv.DeInit == NULL);

    /* Start partially failed after installing its ISP algorithm list. */
    configure_partial(&regs);
    camera_bsp.imx335.hIsp.algorithm = &regs;
    assert(CMW_CAMERA_DeInit() == CMW_ERROR_NONE);
    assert(isp_deinits == 1U && sensor_deinits == 2U);

    /* Component cleanup failures are reported but counters never go negative. */
    configure_partial(&regs);
    camera_bsp.imx335.hIsp.algorithm = &regs;
    isp_fails = 1U;
    assert(CMW_CAMERA_DeInit() == CMW_ERROR_COMPONENT_FAILURE);
    assert(is_camera_init == 0 && is_camera_started == 0 && is_pipe1_2_shared == 0);
    isp_fails = 0U;

    configure_partial(&regs);
    hcamera_dcmipp.PipeState[1] = HAL_DCMIPP_PIPE_STATE_BUSY;
    hcamera_dcmipp.PipeState[2] = HAL_DCMIPP_PIPE_STATE_BUSY;
    regs.P1FCTCR = regs.P2FCTCR = 1U;
    regs.P1FSCR = regs.P2FSCR = 1U;
    regs.P1PPCR = regs.P2PPCR = 1U;
    regs.CMIER = 0xFFFFU;
    regs.CMSR1 = DCMIPP_CMSR1_P2CPTACT;
    stop_complete_after = 0U;
    sleep_count = 0U;
    assert(app_camera_pipe_stop(DCMIPP_PIPE2) == APP_CAMERA_STATUS_TIMEOUT);
    assert(sleep_count == 100U);
    assert(hcamera_dcmipp.PipeState[2] == HAL_DCMIPP_PIPE_STATE_BUSY);
    assert(regs.P2FSCR == 1U); /* DMA not released by a timeout. */
    assert(regs.P1FSCR == 1U && regs.P1FCTCR == 1U);

    stop_complete_after = 3U;
    sleep_count = 0U;
    assert(app_camera_pipe_stop(DCMIPP_PIPE2) == CMW_ERROR_NONE);
    assert(sleep_count == 3U);
    assert(regs.P2FSCR == 0U && regs.P2PPCR == 0U);
    assert(hcamera_dcmipp.PipeState[2] == HAL_DCMIPP_PIPE_STATE_READY);
    assert(regs.P1FSCR == 1U && regs.P1FCTCR == 1U && regs.P1PPCR == 1U);
    assert((regs.CMIER & DCMIPP_IT_PIPE1_FRAME) != 0U);
    assert((regs.CMIER & DCMIPP_IT_PIPE2_FRAME) == 0U);
    assert(regs.CMFCR == (DCMIPP_IT_PIPE2_FRAME | DCMIPP_IT_PIPE2_VSYNC |
                         DCMIPP_IT_PIPE2_LINE | DCMIPP_IT_PIPE2_OVR));
    assert(irq_mask == 0U);
    CMW_IMX335_VsyncEventCallback(NULL, DCMIPP_PIPE1);
    camera_bsp.imx335.hIsp.algorithm = NULL;
    camera_bsp.imx335.hIsp.hDcmipp = &regs;
    CMW_IMX335_VsyncEventCallback(&camera_bsp.imx335, DCMIPP_PIPE1);
    camera_bsp.imx335.hIsp.algorithm = &regs;
    camera_bsp.imx335.hIsp.hDcmipp = NULL;
    CMW_IMX335_VsyncEventCallback(&camera_bsp.imx335, DCMIPP_PIPE1);
    assert(vsync_calls == 0U);
    camera_bsp.imx335.hIsp.hDcmipp = &regs;
    CMW_IMX335_VsyncEventCallback(&camera_bsp.imx335, DCMIPP_PIPE1);
    assert(vsync_calls == 2U);
    puts("PASS: camera partial-init reset, ISP guard and bounded independent stop.");
    return 0;
}
