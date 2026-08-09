#if defined(TARGET_PSP)

#include <stdbool.h>
#include <pspctrl.h>
#include <ultra64.h>

#include "controller_api.h"
#include "../configfile.h"

static void controller_psp_init(void) {
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    // consider using sceCtrlSetIdleCancelThreshold
}

// n64 controls https://strategywiki.org/wiki/Super_Mario_64/Controls
// PSP -> N64 control mapping
//  SQUARE   -> B
//  CROSS    -> A
//  TRIANGLE -> L Trigger /* Literally unused in the game */
//  CIRCLE   -> Z Trigger
//  L Trig   -> Z Trigger
//  R Trig   -> R Trigger
//  Analog   -> Move
//  Start    -> Start
//  DPad     -> Camera buttons

static void controller_psp_read(OSContPad *pad) {
    static SceCtrlData data;

    if (!sceCtrlPeekBufferPositive(&data, 1))
        return;

    const char stickH = data.Lx+0x80;
    const char stickV = 0xff-(data.Ly+0x80);
    uint32_t magnitude_sq = (uint32_t)(stickH * stickH) + (uint32_t)(stickV * stickV);

    if (magnitude_sq > (uint32_t)(configDeadzone * configDeadzone)) {
        pad->stick_x = stickH;
        pad->stick_y = stickV;
    }

    if (data.Buttons & configKeyStart)
        pad->button |= START_BUTTON;
    if (data.Buttons & configKeyB)
        pad->button |= B_BUTTON;
    if (data.Buttons & configKeyA)
        pad->button |= A_BUTTON;
    if (data.Buttons & configKeyL)
        pad->button |= L_TRIG;
    if (data.Buttons & configKeyZ)
        pad->button |= Z_TRIG;
    if (data.Buttons & configKeyR)
        pad->button |= R_TRIG;
    if (data.Buttons & configKeyCUp)
        pad->button |= U_CBUTTONS;
    if (data.Buttons & configKeyCDown)
        pad->button |= D_CBUTTONS;
    if (data.Buttons & configKeyCLeft)
        pad->button |= L_CBUTTONS;
    if (data.Buttons & configKeyCRight)
        pad->button |= R_CBUTTONS;

}

struct ControllerAPI controller_psp = {
    controller_psp_init,
    controller_psp_read
};

#endif
