from pathlib import Path

ROOT = Path(__file__).parent.parent  # sm64-RL repo root

SM64_EXE = ROOT / "build" / "us_pc" / "sm64.us.f3dex2e.exe"

# Shared memory — must match SM64SharedState in c_bridge/rl_bridge.h
SHM_NAME = "SM64_RL_SHM"
SHM_SIZE = 128  # bytes (struct is 48, rest is headroom)

# Episode
MAX_STEPS_PER_EPISODE = 6000  # ~100 s at 60 fps

# Reward weights
STAR_REWARD       = 1000.0
DEATH_PENALTY     = -100.0
TIME_PENALTY      = -0.01
HEIGHT_SCALE      = 0.002   # reward per unit of y gained toward summit

# PPO hyperparameters (Stable-Baselines3 defaults are usually fine to start)
TOTAL_TIMESTEPS = 2_000_000
LEARNING_RATE   = 3e-4
N_STEPS         = 2048
BATCH_SIZE      = 64
N_EPOCHS        = 10
GAMMA           = 0.99
GAE_LAMBDA      = 0.95
