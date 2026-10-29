/**
 * rl_bridge.h  –  Shared-memory contract between sm64-port and Python.
 *
 * Copy this file (and rl_bridge.c) into sm64-port under src/rl/.
 * The Python side mirrors this struct in RL/bridge/shm_bridge.py.
 *
 * Sync protocol (spin-wait on uint8 flags):
 *   C sets   state_ready=1  after writing a new frame's state.
 *   Python clears state_ready=0  after copying the snapshot.
 *   Python sets   action_ready=1 after writing stick + buttons.
 *   C clears action_ready=0  after applying input to controller.
 */

#pragma once
#include <stdint.h>

#define RL_SHM_NAME  "SM64_RL_SHM"
#define RL_SHM_SIZE  128

/* SM64 button bitmasks (same as controller.h) */
#define RL_BTN_A     0x8000u
#define RL_BTN_B     0x4000u
#define RL_BTN_Z     0x2000u
#define RL_BTN_START 0x1000u
#define RL_BTN_R     0x0010u
#define RL_BTN_L     0x0020u

typedef struct {
    /* ---- Game state: C writes, Python reads ---- */
    float    pos_x, pos_y, pos_z;   /* gMarioState->pos          */
    float    vel_x, vel_y, vel_z;   /* gMarioState->vel          */
    float    forward_vel;            /* gMarioState->forwardVel   */
    uint32_t action;                 /* gMarioState->action       */
    uint16_t buttons_out;            /* last buttons (debug only) */
    int16_t  face_yaw;               /* gMarioState->faceAngle[0] */
    uint8_t  health;                 /* gMarioState->health >> 8  (0-8) */
    uint8_t  num_stars;              /* save_file_get_total_star_count */

    /* ---- Input: Python writes, C reads ---- */
    int8_t   stick_x;               /* -128..127 */
    int8_t   stick_y;               /* -128..127 */
    uint16_t buttons;               /* bitmask (BTN_A etc.) */

    /* ---- Sync flags ---- */
    uint8_t  state_ready;    /* C→1 new state available;  Python clears */
    uint8_t  action_ready;   /* Python→1 action available; C clears     */
    uint8_t  should_reset;   /* Python→1 request reset;    C clears     */
    uint8_t  episode_done;   /* C→1 on death or star;      Python clears */
    uint8_t  game_ready;     /* C→1 once fully initialized               */
    uint8_t  _pad[3];
} SM64SharedState;           /* sizeof == 48, aligned to 4 bytes         */

/* Call once during game init (before the main loop). */
void rl_bridge_init(void);

/* Call once per game frame, AFTER execute_game_iter(). */
void rl_bridge_post_frame(void);

/* Call to apply pending input before reading the controller. */
void rl_bridge_apply_input(int16_t *stick_x, int16_t *stick_y, uint16_t *buttons);
