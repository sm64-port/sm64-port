#pragma once

#include <stdbool.h>
#include <stdint.h>

#if defined(TARGET_PSP)
int psp_me_boot(void);
bool psp_me_init(void);
bool psp_me_is_active(void);
void psp_me_shutdown(void);
bool psp_me_generate_audio(int16_t *samples, uint32_t frames);
#else
static inline int psp_me_boot(void) {
    return -1;
}

static inline bool psp_me_init(void) {
    return false;
}

static inline bool psp_me_is_active(void) {
    return false;
}

static inline void psp_me_shutdown(void) {
}

static inline bool psp_me_generate_audio(int16_t *samples, uint32_t frames) {
    (void)samples;
    (void)frames;
    return false;
}
#endif
