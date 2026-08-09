#if defined(TARGET_PSP)
#include "gfx_window_manager_api.h"
#include "gfx_screen_config.h"
#include "macros.h"

#include <stdio.h>
#include <string.h>
#include <pspsdk.h>
#include <pspkernel.h>
#include <pspctrl.h>
#include <psppower.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>
#include <stdint.h>

#define GFX_API_NAME "PSP - sceGU"
#define SCR_WIDTH (480)
#define SCR_HEIGHT (272)

#define PSP_VI_RATE_HZ 60U
#define PSP_VI_FRAME_BASE_USEC 16666U
#define PSP_VI_FRAME_REMAINDER 40U
#define PSP_GAME_UPDATE_RATE 2U

static uint32_t sNextFrameCompletionUsec;
static uint32_t sFramePacingRemainder;
static bool sFramePacingInitialized;

static inline int32_t gfx_psp_time_diff(uint32_t a, uint32_t b) {
    return (int32_t)(a - b);
}

/* Mirror OOT's VI remainder accumulation. At updateRate 2 this produces
 * 33333, 33333, 33334 microsecond intervals, averaging exactly 30 Hz. */
static uint32_t gfx_psp_get_frame_usec(void) {
    uint32_t frame_usec = PSP_VI_FRAME_BASE_USEC * PSP_GAME_UPDATE_RATE;

    sFramePacingRemainder += PSP_VI_FRAME_REMAINDER * PSP_GAME_UPDATE_RATE;
    if (sFramePacingRemainder >= PSP_VI_RATE_HZ) {
        uint32_t extra_usec = sFramePacingRemainder / PSP_VI_RATE_HZ;

        frame_usec += extra_usec;
        sFramePacingRemainder -= extra_usec * PSP_VI_RATE_HZ;
    }

    return frame_usec;
}

static void gfx_psp_pace_presented_frame(void) {
    uint32_t now = sceKernelGetSystemTimeLow();
    uint32_t frame_usec = gfx_psp_get_frame_usec();
    int32_t wait_usec;

    if (!sFramePacingInitialized) {
        sNextFrameCompletionUsec = now;
        sFramePacingInitialized = true;
    }

    wait_usec = gfx_psp_time_diff(sNextFrameCompletionUsec, now);
    if (wait_usec > 0) {
        sceKernelDelayThread((uint32_t)wait_usec);
    } else if (wait_usec < 0) {
        /* Do not run catch-up frames after a missed deadline. */
        sNextFrameCompletionUsec = now;
    }

    sNextFrameCompletionUsec += frame_usec;
}

/* I forgot why we need this */
void __assert_func(UNUSED const char *file, UNUSED int line, UNUSED const char *method, UNUSED const char *expression) {
}

/* Minimalist PSP SDK 0.15.0 for Windows whines about missing this, linux built toolchain doesn't care */
char *stpcpy(char *__restrict__ dest, const char *__restrict__ src) {
    while ((*dest++ = *src++) != '\0')
        /* nothing */;
    return --dest;
}

int isspace(int _c) {
    char c = (char) _c;
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

static void gfx_psp_init(UNUSED const char *game_name, UNUSED bool start_in_fullscreen) {
    sNextFrameCompletionUsec = 0;
    sFramePacingRemainder = 0;
    sFramePacingInitialized = false;

    scePowerSetClockFrequency(333, 333, 166);
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
}

static void gfx_psp_set_fullscreen_changed_callback(UNUSED void (*on_fullscreen_changed)(bool is_now_fullscreen)) {
}

static void gfx_psp_set_fullscreen(UNUSED bool enable) {
}

static void gfx_psp_set_keyboard_callbacks(UNUSED bool (*on_key_down)(int scancode),
                                           UNUSED bool (*on_key_up)(int scancode),
                                           UNUSED void (*on_all_keys_up)(void)) {
}

static void gfx_psp_main_loop(void (*run_one_game_iter)(void)) {
    while (1) {
        run_one_game_iter();
    }
}

static void gfx_psp_get_dimensions(uint32_t *width, uint32_t *height) {
    *width = SCR_WIDTH;
    *height = SCR_HEIGHT;
}

/* What events should we be handling? */
static void gfx_psp_handle_events(void) {
    /* Lets us yield to other threads*/
    sceKernelDelayThread(100);
}

static bool gfx_psp_start_frame(void) {
    return true;
}

static void gfx_psp_swap_buffers_begin(void) {
    gfx_psp_pace_presented_frame();
}

static void gfx_psp_swap_buffers_end(void) {
    /* Lets us yield to other threads*/
    sceKernelDelayThread(100);
}

/* Idk what this is for? */
static double gfx_psp_get_time(void) {
    return 0.0;
}

struct GfxWindowManagerAPI gfx_psp = {
    gfx_psp_init,
    gfx_psp_set_keyboard_callbacks,
    gfx_psp_set_fullscreen_changed_callback,
    gfx_psp_set_fullscreen,
    gfx_psp_main_loop,
    gfx_psp_get_dimensions,
    gfx_psp_handle_events,
    gfx_psp_start_frame,
    gfx_psp_swap_buffers_begin,
    gfx_psp_swap_buffers_end,
    gfx_psp_get_time
};
#endif // TARGET_PSP
