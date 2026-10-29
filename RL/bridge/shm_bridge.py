"""
Shared-memory bridge between sm64-port (C) and Python.

Layout mirrors SM64SharedState in c_bridge/rl_bridge.h — keep both in sync.
Sync protocol: spin-wait on uint8 flags (state_ready / action_ready).
  C sets state_ready=1 → Python reads and clears it.
  Python sets action_ready=1 → C reads and clears it.
"""

import ctypes
import mmap
import time
from dataclasses import dataclass

from ..config import SHM_NAME, SHM_SIZE


# ---------------------------------------------------------------------------
# Struct layout — must match SM64SharedState in c_bridge/rl_bridge.h exactly
# ---------------------------------------------------------------------------

class _SM64SharedState(ctypes.Structure):
    _fields_ = [
        # Game state (C writes, Python reads) --------------------------------
        ("pos_x",        ctypes.c_float),
        ("pos_y",        ctypes.c_float),
        ("pos_z",        ctypes.c_float),
        ("vel_x",        ctypes.c_float),
        ("vel_y",        ctypes.c_float),
        ("vel_z",        ctypes.c_float),
        ("forward_vel",  ctypes.c_float),
        ("action",       ctypes.c_uint32),   # SM64 action constant (u32)
        ("buttons_out",  ctypes.c_uint16),   # last buttons pressed (debug)
        ("face_yaw",     ctypes.c_int16),    # Mario's facing angle
        ("health",       ctypes.c_uint8),    # 0-8 (8 = full)
        ("num_stars",    ctypes.c_uint8),    # stars collected this level
        # Input (Python writes, C reads) -------------------------------------
        ("stick_x",      ctypes.c_int8),     # -128 to 127
        ("stick_y",      ctypes.c_int8),     # -128 to 127
        ("buttons",      ctypes.c_uint16),   # A=0x8000 B=0x4000 Z=0x2000 R=0x0010
        # Sync flags ---------------------------------------------------------
        ("state_ready",  ctypes.c_uint8),    # C→1 when new state written
        ("action_ready", ctypes.c_uint8),    # Python→1 when action written
        ("should_reset", ctypes.c_uint8),    # Python→1 to request level reset
        ("episode_done", ctypes.c_uint8),    # C→1 on death or star collect
        ("game_ready",   ctypes.c_uint8),    # C→1 once fully initialized
        ("_pad",         ctypes.c_uint8 * 3),
    ]


assert ctypes.sizeof(_SM64SharedState) <= SHM_SIZE, (
    f"SM64SharedState ({ctypes.sizeof(_SM64SharedState)} B) exceeds SHM_SIZE={SHM_SIZE}"
)


@dataclass
class MarioState:
    """Snapshot of game state returned to the Gymnasium env."""
    pos_x: float
    pos_y: float
    pos_z: float
    vel_x: float
    vel_y: float
    vel_z: float
    forward_vel: float
    face_yaw: int       # raw int16 (-32768..32767)
    health: int         # 0-8
    num_stars: int
    action: int
    episode_done: bool


# ---------------------------------------------------------------------------

SPIN_SLEEP = 0.0002   # 0.2 ms sleep between spin iterations


class SHMBridge:
    """Open (or create) the named shared memory and talk to sm64-port."""

    def __init__(self):
        # On Windows mmap.mmap(-1, size, tagname) uses CreateFileMapping
        # backed by the paging file → same as what the C code opens with
        # OpenFileMapping("SM64_RL_SHM").
        self._mm = mmap.mmap(-1, SHM_SIZE, tagname=SHM_NAME)
        self._state = _SM64SharedState.from_buffer(self._mm)

    # ------------------------------------------------------------------
    # Python → C
    # ------------------------------------------------------------------

    def send_action(self, stick_x: int, stick_y: int, buttons: int) -> None:
        self._state.stick_x      = max(-128, min(127, stick_x))
        self._state.stick_y      = max(-128, min(127, stick_y))
        self._state.buttons      = buttons
        self._state.action_ready = 1

    def request_reset(self) -> None:
        self._state.should_reset = 1

    # ------------------------------------------------------------------
    # C → Python
    # ------------------------------------------------------------------

    def wait_for_state(self, timeout: float = 5.0) -> MarioState:
        deadline = time.monotonic() + timeout
        while not self._state.state_ready:
            if time.monotonic() > deadline:
                raise TimeoutError("Timed out waiting for sm64 state")
            time.sleep(SPIN_SLEEP)
        snap = MarioState(
            pos_x       = self._state.pos_x,
            pos_y       = self._state.pos_y,
            pos_z       = self._state.pos_z,
            vel_x       = self._state.vel_x,
            vel_y       = self._state.vel_y,
            vel_z       = self._state.vel_z,
            forward_vel = self._state.forward_vel,
            face_yaw    = self._state.face_yaw,
            health      = self._state.health,
            num_stars   = self._state.num_stars,
            action      = self._state.action,
            episode_done= bool(self._state.episode_done),
        )
        self._state.state_ready  = 0
        self._state.episode_done = 0
        return snap

    def wait_for_ready(self, timeout: float = 15.0) -> None:
        deadline = time.monotonic() + timeout
        while not self._state.game_ready:
            if time.monotonic() > deadline:
                raise TimeoutError("sm64 did not signal game_ready in time")
            time.sleep(0.05)

    # ------------------------------------------------------------------

    def close(self) -> None:
        self._mm.close()
