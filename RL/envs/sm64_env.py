"""
Gymnasium environment for SM64 Bob-omb Battlefield (1-1).
Requires sm64-port compiled with RL bridge support (see c_bridge/).
"""

import subprocess
import time

import gymnasium
import numpy as np

from ..bridge.shm_bridge import SHMBridge, MarioState
from ..config import SM64_EXE, MAX_STEPS_PER_EPISODE
from ..envs.spaces import (
    make_observation_space, make_action_space,
    state_to_obs, action_to_input,
)
from ..rewards.bob_omb import compute_reward


class SM64Env(gymnasium.Env):
    """
    One episode = one attempt at Bob-omb Battlefield.
    Terminates on: Mario death, star collected, or MAX_STEPS_PER_EPISODE.
    """

    metadata = {"render_modes": ["human"]}

    def __init__(self, render_mode: str | None = None):
        super().__init__()
        self.observation_space = make_observation_space()
        self.action_space      = make_action_space()
        self.render_mode       = render_mode

        self._process:   subprocess.Popen | None = None
        self._bridge:    SHMBridge | None        = None
        self._prev_state: MarioState | None      = None
        self._step_count = 0

    # ------------------------------------------------------------------
    # Gymnasium API
    # ------------------------------------------------------------------

    def reset(self, seed=None, options=None):
        super().reset(seed=seed)

        if self._bridge is None:
            self._launch_game()
        else:
            self._bridge.request_reset()
            # Wait for C side to acknowledge reset (state_ready goes low then high again)
            time.sleep(0.3)

        self._step_count = 0
        state = self._bridge.wait_for_state()
        self._prev_state = state
        return state_to_obs(state), {}

    def step(self, action: int):
        assert self._bridge is not None, "Call reset() before step()"

        stick_x, stick_y, buttons = action_to_input(action)
        self._bridge.send_action(stick_x, stick_y, buttons)

        state = self._bridge.wait_for_state()

        obs        = state_to_obs(state)
        reward     = compute_reward(self._prev_state, state)
        terminated = state.episode_done
        truncated  = self._step_count >= MAX_STEPS_PER_EPISODE
        info       = {
            "pos":      (state.pos_x, state.pos_y, state.pos_z),
            "health":   state.health,
            "stars":    state.num_stars,
            "step":     self._step_count,
        }

        self._prev_state = state
        self._step_count += 1
        return obs, reward, terminated, truncated, info

    def render(self):
        pass  # the game window is the render; pass when headless

    def close(self):
        if self._bridge is not None:
            self._bridge.close()
            self._bridge = None
        if self._process is not None:
            self._process.terminate()
            self._process = None

    # ------------------------------------------------------------------

    def _launch_game(self):
        self._bridge = SHMBridge()
        cmd = [str(SM64_EXE)]
        if self.render_mode != "human":
            cmd.append("--rl-headless")  # skips SDL window creation
        self._process = subprocess.Popen(cmd)
        self._bridge.wait_for_ready(timeout=15.0)
