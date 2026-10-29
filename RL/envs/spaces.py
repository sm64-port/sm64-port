"""
Observation and action space definitions.

Observation (11 floats, all normalized to roughly [-1, 1]):
  pos_x, pos_y, pos_z   – divided by 8000 (level scale)
  vel_x, vel_y, vel_z   – divided by 80   (max speed ~75 u/f)
  forward_vel           – divided by 80
  yaw_sin, yaw_cos      – sin/cos of face_yaw (avoids angle wraparound)
  health                – divided by 8     (0=dead, 1=full)
  num_stars             – raw (0-7 for 1-1)

Action space: Discrete(9) — pre-defined input combos tuned for 1-1.
"""

import math
import numpy as np
import gymnasium
from gymnasium import spaces

from ..bridge.shm_bridge import MarioState

# N64 button bitmasks
A_BTN = 0x8000
B_BTN = 0x4000
Z_BTN = 0x2000
R_BTN = 0x0010

OBS_DIM = 11
LEVEL_SCALE   = 8000.0
VEL_SCALE     = 80.0
STICK_MAX     = 80

# (stick_x, stick_y, buttons)
DISCRETE_ACTIONS: list[tuple[int, int, int]] = [
    (0,          0,          0),               # 0: no-op
    (0,          STICK_MAX,  0),               # 1: forward
    (0,          STICK_MAX,  A_BTN),           # 2: forward + jump
    (-STICK_MAX, 0,          0),               # 3: strafe left
    (STICK_MAX,  0,          0),               # 4: strafe right
    (0,         -STICK_MAX,  0),               # 5: back
    (0,          STICK_MAX,  Z_BTN | A_BTN),   # 6: long jump
    (0,          0,          A_BTN),           # 7: jump in place
    (0,          STICK_MAX,  B_BTN),           # 8: forward + punch/kick
]

N_ACTIONS = len(DISCRETE_ACTIONS)


def make_observation_space() -> spaces.Box:
    return spaces.Box(
        low  = np.full(OBS_DIM, -2.0, dtype=np.float32),
        high = np.full(OBS_DIM,  2.0, dtype=np.float32),
        dtype = np.float32,
    )


def make_action_space() -> spaces.Discrete:
    return spaces.Discrete(N_ACTIONS)


def state_to_obs(s: MarioState) -> np.ndarray:
    yaw_rad = s.face_yaw * (math.pi / 32768.0)
    return np.array([
        s.pos_x       / LEVEL_SCALE,
        s.pos_y       / LEVEL_SCALE,
        s.pos_z       / LEVEL_SCALE,
        s.vel_x       / VEL_SCALE,
        s.vel_y       / VEL_SCALE,
        s.vel_z       / VEL_SCALE,
        s.forward_vel / VEL_SCALE,
        math.sin(yaw_rad),
        math.cos(yaw_rad),
        s.health      / 8.0,
        float(s.num_stars),
    ], dtype=np.float32)


def action_to_input(action_idx: int) -> tuple[int, int, int]:
    return DISCRETE_ACTIONS[action_idx]
