#if defined(TARGET_PSP)

#include "psp_home_menu.h"

#include <pspctrl.h>
#include <pspimpose_driver.h>
#include <pspkernel.h>
#include <stdint.h>
#include <stdio.h>

#include "configfile.h"
#include "gfx/gfx_pc.h"
#include "gfx/gfx_psp.h"

#define PSP_HOME_MENU_ITEM_RESUME_GAME 0
#define PSP_HOME_MENU_ITEM_CONTROLLER_MAPPING 1
#define PSP_HOME_MENU_ITEM_EXIT_GAME 2
#define PSP_HOME_MENU_ITEM_COUNT 3

#define PSP_HOME_MENU_SCREEN_MAIN 0
#define PSP_HOME_MENU_SCREEN_CONTROLLER_MAPPING 1

#define PSP_HOME_MENU_INPUT_LOCKOUT_US 500000
#define PSP_HOME_MENU_INPUT_DEBOUNCE_US 150000
#define PSP_HOME_MENU_STATUS_FRAMES 120
#define PSP_HOME_MENU_DEADZONE_MIN 0
#define PSP_HOME_MENU_DEADZONE_MAX 80
#define PSP_HOME_MENU_CONFIG_FILE "sm64config.txt"
#define PSP_HOME_MENU_BUTTON_MASK                                                         \
    (PSP_CTRL_CIRCLE | PSP_CTRL_CROSS | PSP_CTRL_START | PSP_CTRL_UP | PSP_CTRL_DOWN | \
     PSP_CTRL_LEFT | PSP_CTRL_RIGHT)

typedef struct PspHomeMenuBinding {
    const char *name;
    unsigned int *mask;
    unsigned int default_mask;
} PspHomeMenuBinding;

typedef struct PspHomeMenuPhysicalButton {
    const char *name;
    unsigned int mask;
} PspHomeMenuPhysicalButton;

static PspHomeMenuBinding sBindings[] = {
    { "A Button", &configKeyA, PSP_CTRL_CROSS },
    { "B Button", &configKeyB, PSP_CTRL_SQUARE },
    { "Start", &configKeyStart, PSP_CTRL_START },
    { "L Trigger", &configKeyL, PSP_CTRL_TRIANGLE },
    { "R Trigger", &configKeyR, PSP_CTRL_RTRIGGER },
    { "Z Trigger", &configKeyZ, PSP_CTRL_CIRCLE | PSP_CTRL_LTRIGGER },
    { "C Up", &configKeyCUp, PSP_CTRL_UP },
    { "C Down", &configKeyCDown, PSP_CTRL_DOWN },
    { "C Left", &configKeyCLeft, PSP_CTRL_LEFT },
    { "C Right", &configKeyCRight, PSP_CTRL_RIGHT },
};

static const PspHomeMenuPhysicalButton sPhysicalButtons[] = {
    { "None", 0 },
    { "Cross", PSP_CTRL_CROSS },
    { "Circle", PSP_CTRL_CIRCLE },
    { "Square", PSP_CTRL_SQUARE },
    { "Triangle", PSP_CTRL_TRIANGLE },
    { "L Trigger", PSP_CTRL_LTRIGGER },
    { "R Trigger", PSP_CTRL_RTRIGGER },
    { "Select", PSP_CTRL_SELECT },
    { "Start", PSP_CTRL_START },
    { "D-Pad Up", PSP_CTRL_UP },
    { "D-Pad Down", PSP_CTRL_DOWN },
    { "D-Pad Left", PSP_CTRL_LEFT },
    { "D-Pad Right", PSP_CTRL_RIGHT },
};

static volatile int sOpenRequested;
static bool sActive;
static int sSelectedIndex;
static int sScreen;
static int sControlSelectedIndex;
static int sStatusTimer;
static uint32_t sCurrentButtons;
static uint32_t sLastButtons;
static uint32_t sLastPolledButtons;
static uint32_t sMenuInputLockoutStartUsec;
static uint32_t sLastMenuInputUsec;
static char sStatusMessage[64];

typedef struct PspHomeMenuRenderArgs {
    int selected_index;
    int screen;
    int control_selected_index;
    const char *status_message;
} PspHomeMenuRenderArgs;

static uint32_t psp_home_menu_read_buttons(void) {
    SceCtrlData pad = { 0 };

    if (sceCtrlPeekBufferPositive(&pad, 1) > 0) {
        sCurrentButtons = pad.Buttons;
    }

    return sCurrentButtons;
}

static int psp_home_menu_deadzone_row(void) {
    return psp_home_menu_get_binding_count();
}

static int psp_home_menu_save_row(void) {
    return psp_home_menu_get_binding_count() + 1;
}

static int psp_home_menu_reset_row(void) {
    return psp_home_menu_get_binding_count() + 2;
}

static int psp_home_menu_back_row(void) {
    return psp_home_menu_get_binding_count() + 3;
}

static int psp_home_menu_control_row_count(void) {
    return psp_home_menu_get_binding_count() + 4;
}

static void psp_home_menu_set_status(const char *message) {
    snprintf(sStatusMessage, sizeof(sStatusMessage), "%s", message);
    sStatusTimer = PSP_HOME_MENU_STATUS_FRAMES;
}

static void psp_home_menu_open(void) {
    sActive = true;
    sSelectedIndex = PSP_HOME_MENU_ITEM_RESUME_GAME;
    sScreen = PSP_HOME_MENU_SCREEN_MAIN;
    sControlSelectedIndex = 0;
    sStatusTimer = 0;
    sStatusMessage[0] = '\0';
    sMenuInputLockoutStartUsec = sceKernelGetSystemTimeLow();
    sLastMenuInputUsec = sMenuInputLockoutStartUsec;
    sLastButtons = sCurrentButtons;

    gfx_scegu_set_home_menu_background_active(true);
    gfx_scegu_request_home_menu_background();
}

static void psp_home_menu_close(void) {
    sActive = false;
    sScreen = PSP_HOME_MENU_SCREEN_MAIN;
    sStatusTimer = 0;
    gfx_scegu_set_home_menu_background_active(false);
}

static void psp_home_menu_move_control_selection(int direction) {
    int row_count = psp_home_menu_control_row_count();

    sControlSelectedIndex += direction;
    if (sControlSelectedIndex < 0) {
        sControlSelectedIndex = row_count - 1;
    } else if (sControlSelectedIndex >= row_count) {
        sControlSelectedIndex = 0;
    }
}

static void psp_home_menu_cycle_binding(int index, int direction) {
    int current = 0;
    int count = (int)(sizeof(sPhysicalButtons) / sizeof(sPhysicalButtons[0]));
    int i;

    if ((index < 0) || (index >= psp_home_menu_get_binding_count())) {
        return;
    }

    for (i = 0; i < count; i++) {
        if (*sBindings[index].mask == sPhysicalButtons[i].mask) {
            current = i;
            break;
        }
    }

    current += (direction >= 0) ? 1 : -1;
    if (current < 0) {
        current = count - 1;
    } else if (current >= count) {
        current = 0;
    }
    *sBindings[index].mask = sPhysicalButtons[current].mask;
}

static void psp_home_menu_adjust_control_row(int direction) {
    if (sControlSelectedIndex < psp_home_menu_get_binding_count()) {
        psp_home_menu_cycle_binding(sControlSelectedIndex, direction);
    } else if (sControlSelectedIndex == psp_home_menu_deadzone_row()) {
        int deadzone = (int)configDeadzone + (direction * 2);

        if (deadzone < PSP_HOME_MENU_DEADZONE_MIN) {
            deadzone = PSP_HOME_MENU_DEADZONE_MIN;
        } else if (deadzone > PSP_HOME_MENU_DEADZONE_MAX) {
            deadzone = PSP_HOME_MENU_DEADZONE_MAX;
        }
        configDeadzone = (unsigned int)deadzone;
    }
}

static void psp_home_menu_reset_defaults(void) {
    int i;

    for (i = 0; i < psp_home_menu_get_binding_count(); i++) {
        *sBindings[i].mask = sBindings[i].default_mask;
    }
    configDeadzone = 0x20;
}

static void psp_home_menu_activate_control_row(void) {
    if (sControlSelectedIndex < psp_home_menu_get_binding_count()) {
        psp_home_menu_cycle_binding(sControlSelectedIndex, 1);
    } else if (sControlSelectedIndex == psp_home_menu_deadzone_row()) {
        psp_home_menu_adjust_control_row(1);
    } else if (sControlSelectedIndex == psp_home_menu_save_row()) {
        configfile_save(PSP_HOME_MENU_CONFIG_FILE);
        psp_home_menu_set_status("Saved sm64config.txt");
    } else if (sControlSelectedIndex == psp_home_menu_reset_row()) {
        psp_home_menu_reset_defaults();
        configfile_save(PSP_HOME_MENU_CONFIG_FILE);
        psp_home_menu_set_status("Defaults saved");
    } else if (sControlSelectedIndex == psp_home_menu_back_row()) {
        sScreen = PSP_HOME_MENU_SCREEN_MAIN;
    }
}

static void psp_home_menu_draw(void *arg) {
    const PspHomeMenuRenderArgs *menu = (const PspHomeMenuRenderArgs *)arg;

    /* Mario's cap and shirt use full red as their primary light color. */
    gfx_scegu_render_home_menu(menu->selected_index, menu->screen,
                               menu->control_selected_index, menu->status_message,
                               255, 0, 0);
}

static void psp_home_menu_render(void) {
    const PspHomeMenuRenderArgs args = {
        sSelectedIndex,
        sScreen,
        sControlSelectedIndex,
        (sStatusTimer > 0) ? sStatusMessage : NULL,
    };

    gfx_render_callback_frame(psp_home_menu_draw, (void *)&args);
}

void psp_home_menu_init(void) {
    sOpenRequested = false;
    sActive = false;
    sSelectedIndex = PSP_HOME_MENU_ITEM_RESUME_GAME;
    sScreen = PSP_HOME_MENU_SCREEN_MAIN;
    sControlSelectedIndex = 0;
    sStatusTimer = 0;
    sCurrentButtons = 0;
    sLastButtons = 0;
    sLastPolledButtons = psp_home_menu_read_buttons();
    sMenuInputLockoutStartUsec = 0;
    sLastMenuInputUsec = 0;
    sStatusMessage[0] = '\0';

    /* Suppress the firmware HOME overlay; this port renders its own menu. */
    sceImposeSetHomePopup(0);
}

void psp_home_menu_poll_home_button(void) {
    uint32_t buttons = psp_home_menu_read_buttons();
    uint32_t pressed = buttons & ~sLastPolledButtons;

    sLastPolledButtons = buttons;
    if ((pressed & PSP_CTRL_HOME) && !sActive) {
        psp_home_menu_open();
    }
}

bool psp_home_menu_is_open(void) {
    return sActive || sOpenRequested;
}

enum PspHomeMenuResult psp_home_menu_run_frame(void) {
    uint32_t buttons;
    uint32_t pressed;
    uint32_t now;

    if (sOpenRequested) {
        sOpenRequested = false;
        if (!sActive) {
            psp_home_menu_open();
        }
    }

    if (!sActive) {
        return PSP_HOME_MENU_RESULT_NONE;
    }

    buttons = sCurrentButtons;
    pressed = buttons & ~sLastButtons;
    sLastButtons = buttons;
    sLastPolledButtons = buttons;

    if (sStatusTimer > 0) {
        sStatusTimer--;
    }

    now = sceKernelGetSystemTimeLow();
    if ((now - sMenuInputLockoutStartUsec) < PSP_HOME_MENU_INPUT_LOCKOUT_US) {
        psp_home_menu_render();
        return PSP_HOME_MENU_RESULT_NONE;
    }

    pressed &= PSP_HOME_MENU_BUTTON_MASK;
    if (pressed != 0) {
        if ((now - sLastMenuInputUsec) < PSP_HOME_MENU_INPUT_DEBOUNCE_US) {
            pressed = 0;
        } else {
            sLastMenuInputUsec = now;
        }
    }

    if (sScreen == PSP_HOME_MENU_SCREEN_CONTROLLER_MAPPING) {
        if (pressed & PSP_CTRL_CIRCLE) {
            sScreen = PSP_HOME_MENU_SCREEN_MAIN;
        } else if (pressed & PSP_CTRL_UP) {
            psp_home_menu_move_control_selection(-1);
        } else if (pressed & PSP_CTRL_DOWN) {
            psp_home_menu_move_control_selection(1);
        } else if (pressed & PSP_CTRL_LEFT) {
            psp_home_menu_adjust_control_row(-1);
        } else if (pressed & PSP_CTRL_RIGHT) {
            psp_home_menu_adjust_control_row(1);
        } else if (pressed & (PSP_CTRL_CROSS | PSP_CTRL_START)) {
            psp_home_menu_activate_control_row();
        }
    } else {
        if (pressed & PSP_CTRL_UP) {
            sSelectedIndex = (sSelectedIndex + PSP_HOME_MENU_ITEM_COUNT - 1) % PSP_HOME_MENU_ITEM_COUNT;
        } else if (pressed & PSP_CTRL_DOWN) {
            sSelectedIndex = (sSelectedIndex + 1) % PSP_HOME_MENU_ITEM_COUNT;
        }

        if (pressed & (PSP_CTRL_CROSS | PSP_CTRL_START)) {
            if (sSelectedIndex == PSP_HOME_MENU_ITEM_RESUME_GAME) {
                psp_home_menu_close();
                return PSP_HOME_MENU_RESULT_NONE;
            }
            if (sSelectedIndex == PSP_HOME_MENU_ITEM_EXIT_GAME) {
                psp_home_menu_close();
                return PSP_HOME_MENU_RESULT_EXIT_GAME;
            }
            sScreen = PSP_HOME_MENU_SCREEN_CONTROLLER_MAPPING;
        }
    }

    psp_home_menu_render();
    return PSP_HOME_MENU_RESULT_NONE;
}

int psp_home_menu_get_binding_count(void) {
    return (int)(sizeof(sBindings) / sizeof(sBindings[0]));
}

const char *psp_home_menu_get_binding_name(int index) {
    if ((index < 0) || (index >= psp_home_menu_get_binding_count())) {
        return "";
    }
    return sBindings[index].name;
}

void psp_home_menu_get_binding_value_text(int index, char *buffer, size_t buffer_size) {
    unsigned int mask;
    size_t used = 0;
    int i;

    if (buffer_size == 0) {
        return;
    }
    buffer[0] = '\0';
    if ((index < 0) || (index >= psp_home_menu_get_binding_count())) {
        return;
    }

    mask = *sBindings[index].mask;
    if (mask == 0) {
        snprintf(buffer, buffer_size, "None");
        return;
    }

    for (i = 1; i < (int)(sizeof(sPhysicalButtons) / sizeof(sPhysicalButtons[0])); i++) {
        if (mask & sPhysicalButtons[i].mask) {
            int written = snprintf(buffer + used, buffer_size - used, "%s%s", used ? "+" : "",
                                   sPhysicalButtons[i].name);

            if ((written < 0) || ((size_t)written >= buffer_size - used)) {
                buffer[buffer_size - 1] = '\0';
                return;
            }
            used += (size_t)written;
        }
    }
}

int psp_home_menu_get_deadzone(void) {
    return (int)configDeadzone;
}

#endif
