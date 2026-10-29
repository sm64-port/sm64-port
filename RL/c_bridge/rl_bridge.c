/**
 * rl_bridge.c  –  sm64-port side of the shared-memory RL bridge.
 *
 * Copy to src/rl/rl_bridge.c inside sm64-port.
 * Add to Makefile:  SRC_DIRS += src/rl
 * Add to pc_main.c after audio/video init:  rl_bridge_init();
 * Hook post-frame call in src/game/game_loop.c after execute_game_iter().
 * Hook apply-input in src/pc/controller/controller_keyboard.c or equivalent.
 */

#include "rl_bridge.h"

#include <string.h>
#include <time.h>

/* sm64-port internal headers */
#include "game/mario.h"          /* gMarioState                    */
#include "game/save_file.h"      /* save_file_get_total_star_count */
#include "pc/platform.h"         /* for Windows/Linux detection    */

/* ------------------------------------------------------------------ */
/* Platform-specific named shared memory                               */
/* ------------------------------------------------------------------ */

#ifdef _WIN32
#  include <windows.h>
static HANDLE   s_hMap  = NULL;
#else
#  include <fcntl.h>
#  include <sys/mman.h>
#  include <unistd.h>
static int      s_fd    = -1;
#endif

static SM64SharedState *s_shm = NULL;

/* ------------------------------------------------------------------ */

static void spin_sleep_ns(long ns) {
    struct timespec ts = { .tv_sec = 0, .tv_nsec = ns };
    nanosleep(&ts, NULL);
}

void rl_bridge_init(void) {
#ifdef _WIN32
    s_hMap = CreateFileMappingA(
        INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, RL_SHM_SIZE, RL_SHM_NAME);
    if (!s_hMap) return;
    s_shm = (SM64SharedState *)MapViewOfFile(
        s_hMap, FILE_MAP_ALL_ACCESS, 0, 0, RL_SHM_SIZE);
#else
    s_fd = shm_open("/" RL_SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (s_fd < 0) return;
    ftruncate(s_fd, RL_SHM_SIZE);
    s_shm = (SM64SharedState *)mmap(
        NULL, RL_SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, s_fd, 0);
    if (s_shm == MAP_FAILED) { s_shm = NULL; return; }
#endif
    memset(s_shm, 0, RL_SHM_SIZE);
    s_shm->game_ready = 1;
}

void rl_bridge_post_frame(void) {
    if (!s_shm) return;

    /* Handle reset request from Python */
    if (s_shm->should_reset) {
        s_shm->should_reset = 0;
        /* Trigger level reload — simplest approach: set Mario's action to
         * ACT_DEATH_EXIT so the game reloads the level naturally.
         * Adjust to taste (e.g. call level_init_or_update). */
        extern struct MarioState *gMarioState;
        if (gMarioState) gMarioState->action = 0x00090001; /* ACT_DEATH_EXIT */
    }

    /* Write current state */
    extern struct MarioState *gMarioState;
    if (gMarioState) {
        s_shm->pos_x       = gMarioState->pos[0];
        s_shm->pos_y       = gMarioState->pos[1];
        s_shm->pos_z       = gMarioState->pos[2];
        s_shm->vel_x       = gMarioState->vel[0];
        s_shm->vel_y       = gMarioState->vel[1];
        s_shm->vel_z       = gMarioState->vel[2];
        s_shm->forward_vel = gMarioState->forwardVel;
        s_shm->action      = gMarioState->action;
        s_shm->face_yaw    = gMarioState->faceAngle[0];
        s_shm->health      = (uint8_t)(gMarioState->health >> 8);
        s_shm->num_stars   = (uint8_t)save_file_get_total_star_count(0, 0xFFFF);

        /* Episode termination: death or star collected */
        if (s_shm->health == 0 || s_shm->num_stars > 0)
            s_shm->episode_done = 1;
    }

    s_shm->state_ready = 1;

    /* Wait for Python to send back an action (spin up to ~200 ms) */
    for (int i = 0; i < 1000 && !s_shm->action_ready; i++)
        spin_sleep_ns(200000); /* 0.2 ms */
}

void rl_bridge_apply_input(int16_t *stick_x, int16_t *stick_y, uint16_t *buttons) {
    if (!s_shm || !s_shm->action_ready) return;
    *stick_x            = (int16_t)s_shm->stick_x;
    *stick_y            = (int16_t)s_shm->stick_y;
    *buttons            = s_shm->buttons;
    s_shm->buttons_out  = s_shm->buttons;
    s_shm->action_ready = 0;
}
