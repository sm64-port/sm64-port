"""
Evaluate a saved PPO model visually (render_mode='human').

Usage:
    python -m RL.eval --model models/ppo_bob_omb.zip --episodes 5
"""

import argparse
from pathlib import Path

import numpy as np
from stable_baselines3 import PPO

from .envs.sm64_env import SM64Env


def evaluate(model_path: str, n_episodes: int) -> None:
    env   = SM64Env(render_mode="human")
    model = PPO.load(model_path, env=env)

    for ep in range(n_episodes):
        obs, _ = env.reset()
        done   = False
        total_reward = 0.0
        steps  = 0

        while not done:
            action, _ = model.predict(obs, deterministic=True)
            obs, reward, terminated, truncated, info = env.step(int(action))
            total_reward += reward
            steps += 1
            done = terminated or truncated

        print(f"Episode {ep+1:3d} | reward={total_reward:8.1f} | "
              f"steps={steps:5d} | stars={info['stars']} | health={info['health']}")

    env.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--model",    type=str,  required=True)
    parser.add_argument("--episodes", type=int,  default=5)
    args = parser.parse_args()
    evaluate(args.model, args.episodes)
