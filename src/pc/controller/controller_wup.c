#ifdef __linux__

#include <stdbool.h>
#include <pthread.h>
#include <stdio.h>

#include <ultra64.h>

#include "controller_api.h"

void *wup_start(void *a);
bool wup_get_controller_input(uint16_t *buttons, uint8_t axis[6], bool *do_saturate);

static int8_t saturate(int v) {
    // Is this supposed to map between -80 and 80?
    v = v * 3 / 2;
    return v < -128 ? -128 : v > 127 ? 127 : v;
}

static void controller_wup_init(void) {
    printf("Initializing WUP Controllers\n");
    pthread_t pid;
    pthread_create(&pid, NULL, wup_start, NULL);
}


static void controller_wup_read(OSContPad *pad) {
    uint16_t buttons;
    uint8_t axis[6];
    bool do_saturate;
    if (wup_get_controller_input(&buttons, axis, &do_saturate)) {
        int8_t stick_x;
        int8_t stick_y;
        if (buttons & 0x0001) pad->button |= START_BUTTON;
        if (buttons & 0x0008) pad->button |= Z_TRIG;
        if (buttons & 0x0004) pad->button |= R_TRIG;
        if (buttons & 0x0100) pad->button |= A_BUTTON;
        if (buttons & 0x0200) pad->button |= B_BUTTON;
        if (buttons & 0x1000) pad->button |= L_TRIG;
        if (axis[2] < 0x40) pad->button |= L_CBUTTONS;
        if (axis[2] > 0xC0) pad->button |= R_CBUTTONS;
        if (axis[3] < 0x40) pad->button |= D_CBUTTONS;
        if (axis[3] > 0xC0) pad->button |= U_CBUTTONS;
        if (do_saturate) {
            stick_x = saturate(axis[0] - 128 - 0);
            stick_y = saturate(axis[1] - 128 - 0);
        }
        else {
            // Already in the right range (128-80, 128+80) in this case.
            stick_x = axis[0] - 128;
            stick_y = axis[1] - 128;
        }
        if (stick_x != 0 || stick_y != 0) {
            pad->stick_x = stick_x;
            pad->stick_y = stick_y;
        }
    }
}

struct ControllerAPI controller_wup = {
    controller_wup_init,
    controller_wup_read
};

#endif
