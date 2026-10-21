#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "app_lcd.h"
#include "app_bg_buffers.h"
#include "bsp_lcd.h"

/* CFBAR is a shadow register. Active scanout changes only when a simulated
 * hardware VBlank acknowledges this particular layer's VBR request. */
LTDC_Layer_TypeDef test_lcd_layers[2];
const unsigned Font20=0U;
const unsigned bsp_lcd_driver=0U;
static LTDC_HandleTypeDef handle;
static uint32_t active_address[2],irq_mask,test_tick;
static uint32_t set_calls[2],reload_calls[2],invalidate_calls,cache_calls,sleeps;
static uint8_t address_fails,address_writes_on_failure;
static uint8_t reload_fails,reload_sets_vbr_on_failure;
static uint8_t *dma_target;
static uint8_t probe_preempt,probe_preemption_ran;
static uint8_t *pinned_pixels;
static uint8_t thumbnail_stop, thumbnail_nested;
static uint32_t thumbnail_age_advance,thumbnail_cache_calls;

static uint32_t hw_address(const void *p) { return (uint32_t)(uintptr_t)p; }
static void hw_vblank(unsigned layer)
{
    assert(layer<2U && (test_lcd_layers[layer].RCR&LTDC_LxRCR_VBR));
    active_address[layer]=test_lcd_layers[layer].CFBAR;
    test_lcd_layers[layer].RCR&=~LTDC_LxRCR_VBR;
}
static void paint_samples(uint8_t *buffer,uint16_t value)
{
    uint16_t *pixels=(uint16_t *)buffer;
    for (unsigned y=1U;y<=3U;y++)
        for (unsigned x=1U;x<=3U;x++)
            pixels[(LCD_CAMERA_HEIGHT*y/4U)*LCD_CAMERA_WIDTH+
                LCD_CAMERA_WIDTH*x/4U]=value;
}
static void assert_dma_safe(void)
{
    assert(dma_target && hw_address(dma_target)!=active_address[0]);
    if (test_lcd_layers[0].RCR&LTDC_LxRCR_VBR)
        assert(hw_address(dma_target)!=test_lcd_layers[0].CFBAR);
}
static uint8_t capture_frame(uint16_t value,uint8_t retarget_success)
{
    assert_dma_safe();
    assert(app_lcd_get_bg_buffer()==dma_target);
    paint_samples(dma_target,value);
    uint8_t needed=app_lcd_switch_bg_buffer();
    if (needed)
    {
        uint8_t *next=app_lcd_get_bg_buffer();
        assert(next!=dma_target && hw_address(next)!=active_address[0]);
        if (pinned_pixels) { assert(next!=pinned_pixels); }
        if (retarget_success) { dma_target=next; }
        app_lcd_bg_capture_target_result(retarget_success);
    }
    assert(app_lcd_get_bg_buffer()==dma_target);
    assert_dma_safe();
    return needed;
}
static app_lcd_display_diag_t diag(void)
{
    app_lcd_display_diag_t result;
    memset(&result,0,sizeof(result));
    app_lcd_get_display_diag(&result);
    assert(!irq_mask);
    return result;
}

uint32_t HAL_GetTick(void) { return test_tick; }
uint32_t __get_PRIMASK(void) { return irq_mask; }
void __disable_irq(void) { irq_mask=1U; }
void __set_PRIMASK(uint32_t state) { irq_mask=state; }
void SCB_CleanInvalidateDCache_by_Addr(void *a,int32_t n)
{ (void)a;assert(!irq_mask && n>0);cache_calls++; }
void SCB_CleanDCache_by_Addr(void *a,int32_t n)
{ (void)a;assert(!irq_mask && n>0);cache_calls++; }
void SCB_InvalidateDCache_by_Addr(volatile void *a,int32_t n)
{
    assert(!irq_mask && (n==2 || n==LCD_CAMERA_VIEW_WIDTH*2) && a!=NULL);
    invalidate_calls++;
    if (n==LCD_CAMERA_VIEW_WIDTH*2)
    {
        assert(((uintptr_t)a&31U)==0U);thumbnail_cache_calls++;
        if (thumbnail_stop) { thumbnail_stop=0U;app_lcd_stop_bg_capture(); }
        if (thumbnail_age_advance) { test_tick+=thumbnail_age_advance;thumbnail_age_advance=0U; }
        if (thumbnail_nested)
        {
            uint16_t other[4];app_camera_thumbnail_info_t info;
            thumbnail_nested=0U;memset(&info,0xFF,sizeof(info));
            assert(!app_lcd_copy_camera_thumbnail(other,2U,2U,&info));
            for (size_t i=0U;i<sizeof(info);i++) assert(((uint8_t *)&info)[i]==0U);
        }
    }
    if (probe_preempt)
    {
        probe_preempt=0U;
        /* The diagnostic is reading the confirmed frame. While it is pinned,
         * another frame can become active, but camera DMA cannot reclaim it. */
        assert(pinned_pixels && hw_address(pinned_pixels)==active_address[0]);
        assert(capture_frame(0x2222U,1U));
        app_lcd_present_bg_buffer();
        hw_vblank(0U);
        assert(hw_address(pinned_pixels)!=active_address[0]);
        assert(!capture_frame(0x3333U,1U));
        assert(dma_target!=pinned_pixels);
        probe_preemption_ran=1U;
    }
}
HAL_StatusTypeDef HAL_LTDC_SetAddress_NoReload(LTDC_HandleTypeDef *h,uint32_t a,uint32_t layer)
{
    assert(h==&handle && layer<2U);set_calls[layer]++;
    if (!address_fails || address_writes_on_failure)
        test_lcd_layers[layer].CFBAR=a;
    return address_fails?HAL_ERROR:HAL_OK;
}
HAL_StatusTypeDef HAL_LTDC_ReloadLayer(LTDC_HandleTypeDef *h,uint32_t type,uint32_t layer)
{
    assert(h==&handle && layer<2U && type==LTDC_RELOAD_VERTICAL_BLANKING);
    reload_calls[layer]++;
    if (!reload_fails || reload_sets_vbr_on_failure)
        test_lcd_layers[layer].RCR|=type;
    return reload_fails?HAL_ERROR:HAL_OK;
}
void bsp_lcd_init(void) {}
void bsp_lcd_set_brightness(uint8_t b) { assert(b==100U); }
void bsp_lcd_config_layer(unsigned layer,bsp_lcd_layer_config_t *c)
{
    assert(layer<2U);
    test_lcd_layers[layer].CFBAR=c->address;
    test_lcd_layers[layer].CFBLR=LCD_CAMERA_WIDTH*2U;
    active_address[layer]=c->address;
}
LTDC_HandleTypeDef *bsp_lcd_get_ltdc_handle(void) { return &handle; }
void UTIL_LCD_SetFuncDriver(const void *p) { (void)p; }
void UTIL_LCD_SetLayer(unsigned n) { (void)n; }
void UTIL_LCD_Clear(unsigned n) { (void)n; }
void UTIL_LCD_SetFont(const void *p) { (void)p; }
void UTIL_LCD_SetTextColor(unsigned n) { (void)n; }
void tx_thread_sleep(unsigned long ticks)
{ assert(!irq_mask);sleeps++;test_tick+=(uint32_t)ticks; }

static void fixture(void)
{
    memset(test_lcd_layers,0,sizeof(test_lcd_layers));
    memset(active_address,0,sizeof(active_address));
    memset(set_calls,0,sizeof(set_calls));memset(reload_calls,0,sizeof(reload_calls));
    irq_mask=test_tick=invalidate_calls=cache_calls=sleeps=0U;
    address_fails=address_writes_on_failure=0U;
    reload_fails=reload_sets_vbr_on_failure=0U;
    probe_preempt=probe_preemption_ran=0U;pinned_pixels=NULL;
    thumbnail_stop=thumbnail_nested=0U;
    thumbnail_age_advance=thumbnail_cache_calls=0U;
    app_lcd_init();
    dma_target=app_lcd_get_bg_buffer();
    assert(app_lcd_restart_bg_capture());
    assert_dma_safe();
    app_lcd_display_diag_t d=diag();
    assert(d.display_index==1U && d.fill_index==0U &&
        d.pending_index==APP_BG_BUFFER_NONE && d.background_frame_id==0U);
}

static void test_pending_and_layer_ack(void)
{
    fixture();
    uint8_t *completed=dma_target;
    uint32_t old_front=active_address[0];
    assert(capture_frame(0x1357U,1U));
    assert(reload_calls[0]==0U && active_address[0]==old_front);
    app_lcd_present_bg_buffer();
    app_lcd_display_diag_t d=diag();
    assert(d.display_index==1U && d.fill_index==2U && d.pending_index==0U &&
        d.background_reload_pending && d.background_presented==0U);
    assert(active_address[0]==old_front && test_lcd_layers[0].CFBAR==hw_address(completed));
    /* A foreground acknowledgement cannot release a camera background. */
    test_lcd_layers[1].RCR=LTDC_LxRCR_VBR;hw_vblank(1U);
    assert(diag().display_index==1U && diag().pending_index==0U);
    uint8_t *fill=dma_target;
    for (unsigned i=0U;i<100U;i++)
    {
        assert(!capture_frame((uint16_t)i,1U));
        app_lcd_present_bg_buffer();
        assert(dma_target==fill && dma_target!=completed && active_address[0]==old_front);
    }
    d=diag();assert(d.background_dropped==100U && reload_calls[0]==1U &&
        d.background_reload_pending && d.background_frame_id==0U);
    /* Stop/restart during a delayed reload keeps both immutable slots. */
    app_lcd_stop_bg_capture();
    assert(!app_lcd_switch_bg_buffer());
    assert(app_lcd_get_bg_buffer()==fill && diag().pending_index==0U);
    assert(app_lcd_restart_bg_capture());
    assert(!capture_frame(0x5555U,1U) && dma_target==fill);
    hw_vblank(0U);
    d=diag();
    assert(d.display_index==0U && d.fill_index==2U && !d.background_reload_pending &&
        d.background_frame_id==1U && d.background_presented==1U);
    assert(d.pixel_min==0x1357U && d.pixel_max==0x1357U && invalidate_calls==9U);
    assert(capture_frame(0x2468U,1U));
    assert(hw_address(dma_target)==old_front);
    app_lcd_present_bg_buffer();hw_vblank(0U);
    d=diag();assert(d.display_index==2U && d.background_presented==2U &&
        d.pixel_min==0x2468U && d.pixel_max==0x2468U);
    assert(cache_calls==2U && !sleeps && !irq_mask);
}

static void test_failed_submissions(uint8_t fail_address,uint8_t ambiguous)
{
    fixture();
    uint8_t *completed=dma_target;
    uint32_t old_front=active_address[0];
    assert(capture_frame(0xABCDU,1U));
    address_fails=fail_address;
    address_writes_on_failure=fail_address;
    reload_fails=!fail_address;
    reload_sets_vbr_on_failure=ambiguous;
    app_lcd_present_bg_buffer();
    app_lcd_display_diag_t d=diag();
    assert(d.display_index==1U && d.fill_index==2U && !d.background_reload_pending &&
        d.background_reload_errors==1U && d.background_last_status==HAL_ERROR);
    assert(test_lcd_layers[0].CFBAR==hw_address(completed));
    /* HAL failure never turns a stale shadow address into acknowledged front. */
    assert(active_address[0]==old_front);
    assert(!capture_frame(0x0101U,1U) && dma_target!=completed);
    address_fails=reload_fails=address_writes_on_failure=0U;
    if (ambiguous)
    {
        uint32_t calls=set_calls[0];
        for (unsigned i=0U;i<100U;i++)
        {
            assert(!capture_frame((uint16_t)i,1U));
            app_lcd_present_bg_buffer();
            assert(set_calls[0]==calls && dma_target!=completed);
        }
        assert(diag().display_index==1U);
        hw_vblank(0U);
        assert(active_address[0]==hw_address(completed));
        assert(diag().display_index==1U); /* Failed call still not accepted. */
        assert(!capture_frame(0x4545U,1U));
    }
    app_lcd_present_bg_buffer();
    d=diag();assert(d.pending_index==0U && d.background_reload_pending &&
        d.background_last_status==HAL_OK && d.background_reload_errors==1U);
    hw_vblank(0U);
    d=diag();assert(d.display_index==0U && d.background_frame_id==1U &&
        d.background_presented==1U && d.pixel_min==0xABCDU && d.pixel_max==0xABCDU);
    assert_dma_safe();
}

static void test_target_failure_and_unpublished_stop(void)
{
    fixture();
    uint8_t *old_fill=dma_target;
    assert(capture_frame(0x1111U,0U));
    app_lcd_present_bg_buffer();
    app_lcd_display_diag_t d=diag();
    assert(d.fill_index==0U && d.display_index==1U && dma_target==old_fill &&
        d.pending_index==APP_BG_BUFFER_NONE && d.background_target_errors==1U &&
        d.background_dropped==1U && !reload_calls[0]);
    /* The failed frame is reused only because no LTDC submission happened. */
    assert(capture_frame(0x4444U,1U));
    app_lcd_present_bg_buffer();hw_vblank(0U);
    d=diag();assert(d.background_frame_id==2U && d.pixel_min==0x4444U);

    fixture();
    old_fill=dma_target;
    assert(app_lcd_switch_bg_buffer());
    assert(app_lcd_get_bg_buffer()!=old_fill);
    /* DMA was stopped before its retarget could occur. Prepared frame has
     * no scanout claim and must be cancelled on stop. */
    app_lcd_stop_bg_capture();
    assert(app_lcd_get_bg_buffer()==old_fill && app_lcd_restart_bg_capture());
    app_lcd_present_bg_buffer();assert(!reload_calls[0]);
    assert(capture_frame(0x6666U,1U));
    app_lcd_present_bg_buffer();hw_vblank(0U);
    d=diag();assert(d.pixel_min==0x6666U && d.background_presented==1U);
}

static void test_probe_preemption(void)
{
    fixture();
    pinned_pixels=dma_target;
    assert(capture_frame(0x1111U,1U));
    app_lcd_present_bg_buffer();hw_vblank(0U);
    probe_preempt=1U;
    app_lcd_display_diag_t d=diag();
    assert(probe_preemption_ran && d.display_index==2U && d.background_frame_id==2U &&
        d.background_presented==2U && d.background_dropped==1U);
    assert(d.pixel_min==0x1111U && d.pixel_max==0x1111U);
    /* The pinned frame is released only after its nine CPU reads finish. */
    assert(dma_target!=pinned_pixels && invalidate_calls==9U);
    d=diag();assert(d.pixel_min==0x2222U && d.pixel_max==0x2222U && invalidate_calls==18U);
    pinned_pixels=NULL;
    assert(capture_frame(0x7777U,1U));
    assert(hw_address(dma_target)!=active_address[0]);
    app_lcd_present_bg_buffer();hw_vblank(0U);
    d=diag();assert(d.pixel_min==0x7777U && d.background_frame_id==4U);
}

static uint16_t viewport_pattern(unsigned x,unsigned y)
{ return (uint16_t)(y*LCD_CAMERA_WIDTH+x); }

static uint8_t *capture_pattern(uint32_t tick)
{
    uint8_t *completed=dma_target;
    uint16_t *pixels=(uint16_t *)completed;
    test_tick=tick;
    for (unsigned y=0U;y<LCD_CAMERA_HEIGHT;y++)
        for (unsigned x=0U;x<LCD_CAMERA_WIDTH;x++)
            pixels[y*LCD_CAMERA_WIDTH+x]=
                x>=LCD_CAMERA_VIEW_X && x<LCD_CAMERA_VIEW_X+LCD_CAMERA_VIEW_WIDTH ?
                viewport_pattern(x,y):0U;
    assert(app_lcd_switch_bg_buffer());
    dma_target=app_lcd_get_bg_buffer();
    assert(dma_target!=completed);
    app_lcd_bg_capture_target_result(1U);
    app_lcd_present_bg_buffer();
    return completed;
}

static void expect_thumbnail_unavailable(uint16_t *pixels,unsigned width,unsigned height)
{
    app_camera_thumbnail_info_t info;
    memset(&info,0xFF,sizeof(info));
    assert(!app_lcd_copy_camera_thumbnail(pixels,(uint16_t)width,(uint16_t)height,&info));
    for (size_t i=0U;i<sizeof(info);i++) assert(((uint8_t *)&info)[i]==0U);
    assert(!irq_mask);
}

static void check_thumbnail_pattern(const uint16_t *pixels,unsigned side)
{
    for (unsigned y=0U;y<side;y++)
        for (unsigned x=0U;x<side;x++)
        {
            unsigned sx=LCD_CAMERA_VIEW_X+((x*2U+1U)*LCD_CAMERA_VIEW_WIDTH)/(side*2U);
            unsigned sy=LCD_CAMERA_VIEW_Y+((y*2U+1U)*LCD_CAMERA_VIEW_HEIGHT)/(side*2U);
            assert(pixels[y*side+x]==viewport_pattern(sx,sy));
        }
}

static void test_thumbnail_confirmation_and_staleness(void)
{
    static uint16_t picture[APP_CAMERA_THUMBNAIL_MAX_SIDE*APP_CAMERA_THUMBNAIL_MAX_SIDE];
    app_camera_thumbnail_info_t info;
    fixture();
    expect_thumbnail_unavailable(picture,128U,128U); /* Unknown initial scanout. */
    expect_thumbnail_unavailable(NULL,128U,128U);
    expect_thumbnail_unavailable(picture,0U,0U);
    expect_thumbnail_unavailable(picture,129U,129U);
    expect_thumbnail_unavailable(picture,128U,64U);
    assert(!app_lcd_copy_camera_thumbnail(picture,128U,128U,NULL));
    (void)capture_pattern(100U);
    expect_thumbnail_unavailable(picture,128U,128U); /* Shadow address is not front. */
    hw_vblank(0U);test_tick=120U;
    assert(app_lcd_copy_camera_thumbnail(picture,128U,128U,&info));
    assert(info.frame_id==1U && info.captured_ms==100U && !info.simulated &&
        info.source_x==160U && info.source_y==0U &&
        info.source_width==480U && info.source_height==480U);
    check_thumbnail_pattern(picture,128U);
    assert(thumbnail_cache_calls==128U && !sleeps && cache_calls==2U);
    test_tick=600U; /* Exactly 500 ms is available. */
    assert(app_lcd_copy_camera_thumbnail(picture,1U,1U,&info));
    check_thumbnail_pattern(picture,1U);
    test_tick=601U;expect_thumbnail_unavailable(picture,128U,128U);
    app_lcd_stop_bg_capture();test_tick=130U;
    expect_thumbnail_unavailable(picture,128U,128U);
    assert(app_lcd_restart_bg_capture());
    expect_thumbnail_unavailable(picture,128U,128U); /* No image from the previous run. */
    (void)capture_pattern(140U);hw_vblank(0U);
    assert(app_lcd_copy_camera_thumbnail(picture,128U,128U,&info));
    assert(info.frame_id==2U && info.captured_ms==140U);
    check_thumbnail_pattern(picture,128U);

    fixture();
    (void)capture_pattern(UINT32_MAX-20U);hw_vblank(0U);test_tick=5U;
    assert(app_lcd_copy_camera_thumbnail(picture,128U,128U,&info));
    assert(info.captured_ms==UINT32_MAX-20U);
    test_tick=600U;expect_thumbnail_unavailable(picture,128U,128U);
}

static void test_thumbnail_preemption_and_abort(void)
{
    static uint16_t picture[APP_CAMERA_THUMBNAIL_MAX_SIDE*APP_CAMERA_THUMBNAIL_MAX_SIDE];
    app_camera_thumbnail_info_t info;
    fixture();
    pinned_pixels=capture_pattern(10U);hw_vblank(0U);
    probe_preempt=1U;thumbnail_nested=1U;
    assert(app_lcd_copy_camera_thumbnail(picture,128U,128U,&info));
    assert(probe_preemption_ran && info.frame_id==1U && info.captured_ms==10U);
    check_thumbnail_pattern(picture,128U);
    assert(dma_target!=pinned_pixels && thumbnail_cache_calls==128U);
    app_lcd_display_diag_t d=diag();
    assert(d.background_frame_id==2U && d.background_dropped==1U);
    pinned_pixels=NULL;
    assert(capture_frame(0x4444U,1U)); /* Copy released the old front for reuse. */

    fixture();
    (void)capture_pattern(100U);hw_vblank(0U);
    thumbnail_stop=1U;
    expect_thumbnail_unavailable(picture,128U,128U);
    assert(app_lcd_restart_bg_capture());
    expect_thumbnail_unavailable(picture,128U,128U);
    (void)capture_pattern(110U);hw_vblank(0U);
    assert(app_lcd_copy_camera_thumbnail(picture,128U,128U,&info));
    thumbnail_age_advance=501U;
    expect_thumbnail_unavailable(picture,128U,128U); /* Became stale during copy. */
    (void)capture_pattern(700U);hw_vblank(0U);
    assert(app_lcd_copy_camera_thumbnail(picture,128U,128U,&info));
    check_thumbnail_pattern(picture,128U);

    fixture();
    (void)capture_pattern(10U); /* Recovery before this pending image's VBlank. */
    app_lcd_stop_bg_capture();assert(app_lcd_restart_bg_capture());hw_vblank(0U);
    expect_thumbnail_unavailable(picture,128U,128U);
    (void)capture_pattern(20U);hw_vblank(0U);
    assert(app_lcd_copy_camera_thumbnail(picture,128U,128U,&info));
    assert(info.frame_id==2U && info.captured_ms==20U);
}

int main(void)
{
    test_pending_and_layer_ack();
    test_failed_submissions(1U,0U);
    test_failed_submissions(0U,0U);
    test_failed_submissions(0U,1U);
    test_target_failure_and_unpublished_stop();
    test_probe_preemption();
    test_thumbnail_confirmation_and_staleness();
    test_thumbnail_preemption_and_abort();
    app_lcd_get_display_diag(NULL);
    puts("PASS: production LCD background VBlank handoff; HAL/target failure quarantine and recovery; 128x128 RGB565 thumbnail confirmed-front ownership, square crop, freshness/tick wrap, recovery generation, concurrent capture/reentrant refusal/abort; no masked cache/sleep.");
    return 0;
}
