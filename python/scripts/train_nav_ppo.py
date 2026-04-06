from __future__ import annotations

import argparse
from datetime import datetime, timezone
from pathlib import Path

import gymnasium.spaces as spaces
from stable_baselines3 import PPO

from gym_env import UnrealScholaGymEnv


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Train the minimal UE navigation environment with PPO.")
    parser.add_argument("--host", default="localhost", help="Schola host address.")
    parser.add_argument("--port", type=int, default=8000, help="Schola gRPC port.")
    parser.add_argument("--timesteps", type=int, default=50000, help="Total PPO training timesteps.")
    parser.add_argument("--seed", type=int, default=1337, help="Random seed for PPO and environment resets.")
    parser.add_argument(
        "--checkpoint",
        type=Path,
        default=None,
        help="Optional exact path for the PPO checkpoint.",
    )
    parser.add_argument(
        "--checkpoint-dir",
        type=Path,
        default=Path("python/checkpoints"),
        help="Directory to use when --checkpoint is not provided.",
    )
    return parser.parse_args()


def resolve_checkpoint_path(args: argparse.Namespace) -> Path:
    if args.checkpoint is not None:
        return args.checkpoint

    timestamp = datetime.now(timezone.utc).strftime("%Y%m%d_%H%M%S")
    return args.checkpoint_dir / f"nav_ppo_seed{args.seed}_{timestamp}.zip"


def main() -> int:
    args = parse_args()
    checkpoint_path = resolve_checkpoint_path(args)
    checkpoint_path.parent.mkdir(parents=True, exist_ok=True)

    env = UnrealScholaGymEnv(host=args.host, port=args.port, timeout_seconds=45)

    try:
        env.reset(seed=args.seed)
        uses_dict_observation = isinstance(env.observation_space, spaces.Dict)
        policy_name = "MultiInputPolicy" if uses_dict_observation else "MlpPolicy"
        policy_kwargs = {"normalize_images": False} if uses_dict_observation else None
        model = PPO(
            policy_name,
            env,
            verbose=1,
            n_steps=256,
            batch_size=64,
            learning_rate=3e-4,
            gamma=0.99,
            seed=args.seed,
            policy_kwargs=policy_kwargs,
        )
        print(
            f"Training with policy={policy_name} seed={args.seed} for {args.timesteps} timesteps "
            f"dict_obs={uses_dict_observation}"
        )
        model.learn(total_timesteps=args.timesteps)
        model.save(checkpoint_path)
        print(f"Saved checkpoint to {checkpoint_path}")
        return 0
    finally:
        env.close()


if __name__ == "__main__":
    raise SystemExit(main())
