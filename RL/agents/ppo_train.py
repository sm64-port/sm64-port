"""
Train a PPO agent on SM64 Bob-omb Battlefield.

Usage:
    python -m RL.agents.ppo_train
    python -m RL.agents.ppo_train --timesteps 5000000 --run-name experiment_01
"""

import argparse
from pathlib import Path

from stable_baselines3 import PPO
from stable_baselines3.common.env_checker import check_env
from stable_baselines3.common.monitor import Monitor

from ..envs.sm64_env import SM64Env
from ..config import (
    TOTAL_TIMESTEPS, LEARNING_RATE, N_STEPS,
    BATCH_SIZE, N_EPOCHS, GAMMA, GAE_LAMBDA,
)

MODELS_DIR = Path(__file__).parent.parent.parent / "models"
LOGS_DIR   = Path(__file__).parent.parent.parent / "logs"


def train(timesteps: int, run_name: str, check: bool = False) -> None:
    MODELS_DIR.mkdir(exist_ok=True)
    LOGS_DIR.mkdir(exist_ok=True)

    env = Monitor(SM64Env(), filename=str(LOGS_DIR / run_name))

    if check:
        print("Running environment sanity check...")
        check_env(env, warn=True)
        print("Environment OK.")

    model = PPO(
        policy         = "MlpPolicy",
        env            = env,
        learning_rate  = LEARNING_RATE,
        n_steps        = N_STEPS,
        batch_size     = BATCH_SIZE,
        n_epochs       = N_EPOCHS,
        gamma          = GAMMA,
        gae_lambda     = GAE_LAMBDA,
        verbose        = 1,
        tensorboard_log= str(LOGS_DIR),
    )

    print(f"Training for {timesteps:,} timesteps — run: {run_name}")
    model.learn(total_timesteps=timesteps, tb_log_name=run_name)

    save_path = MODELS_DIR / f"{run_name}.zip"
    model.save(save_path)
    print(f"Model saved → {save_path}")
    env.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--timesteps", type=int, default=TOTAL_TIMESTEPS)
    parser.add_argument("--run-name",  type=str, default="ppo_bob_omb")
    parser.add_argument("--check",     action="store_true")
    args = parser.parse_args()
    train(args.timesteps, args.run_name, args.check)
