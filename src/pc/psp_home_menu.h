#ifndef SM64_PSP_HOME_MENU_H
#define SM64_PSP_HOME_MENU_H

#include <stdbool.h>
#include <stddef.h>

enum PspHomeMenuResult {
    PSP_HOME_MENU_RESULT_NONE,
    PSP_HOME_MENU_RESULT_EXIT_GAME,
};

void psp_home_menu_init(void);
void psp_home_menu_poll_home_button(void);
bool psp_home_menu_is_open(void);
enum PspHomeMenuResult psp_home_menu_run_frame(void);

int psp_home_menu_get_binding_count(void);
const char *psp_home_menu_get_binding_name(int index);
void psp_home_menu_get_binding_value_text(int index, char *buffer, size_t buffer_size);
int psp_home_menu_get_deadzone(void);

#endif
