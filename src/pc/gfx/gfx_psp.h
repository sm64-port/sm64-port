#ifndef GFX_PSP_H
#define GFX_PSP_H

#include <stdbool.h>
#include <stdint.h>

#include "gfx_window_manager_api.h"

extern struct GfxWindowManagerAPI gfx_psp;

void gfx_scegu_request_home_menu_background(void);
void gfx_scegu_set_home_menu_background_active(bool active);
void gfx_scegu_render_home_menu(int selected_index, int screen, int control_selected_index,
                                const char *status_message, uint8_t highlight_red,
                                uint8_t highlight_green, uint8_t highlight_blue);

#endif
