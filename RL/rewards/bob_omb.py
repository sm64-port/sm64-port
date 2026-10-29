"""
Reward function for Bob-omb Battlefield (1-1).

Strategy: reward climbing toward the summit (y-axis is up in SM64),
big bonus for collecting a star, penalty for death, small time penalty
to discourage stalling.

The summit of Bob-omb Battlefield is at roughly y ≈ 1700 game units.
We use delta-height shaping so the agent is rewarded for each frame
it makes upward progress, not just for reaching the top.
"""

from ..bridge.shm_bridge import MarioState
from ..config import STAR_REWARD, DEATH_PENALTY, TIME_PENALTY, HEIGHT_SCALE


def compute_reward(prev: MarioState, curr: MarioState) -> float:
    reward = 0.0

    # --- height progress (main shaping signal) ---------------------------
    delta_y = curr.pos_y - prev.pos_y
    reward += delta_y * HEIGHT_SCALE

    # --- star collected --------------------------------------------------
    if curr.num_stars > prev.num_stars:
        reward += STAR_REWARD

    # --- death -----------------------------------------------------------
    # health==0 means dead; the C side also sets episode_done in this case
    if curr.health == 0 and prev.health > 0:
        reward += DEATH_PENALTY

    # --- per-step time penalty -------------------------------------------
    reward += TIME_PENALTY

    return float(reward)
