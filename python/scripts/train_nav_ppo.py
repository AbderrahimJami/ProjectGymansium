from __future__ import annotations

import argparse
from pathlib import Path

from schola.core.protocols.protobuf.gRPC import gRPCProtocol
from schola.core.simulators.unreal.editor import UnrealEditor
from schola.sb3.env import VecEnv
from stable_baselines3 import PPO


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Train the minimal UE navigation environment with PPO.")
    parser.add_argument("--host", default="localhost", help="Schola host address.")
    parser.add_argument("--port", type=int, default=8000, help="Schola gRPC port.")
    parser.add_argument("--timesteps", type=int, default=10000, help="Total PPO training timesteps.")
    parser.add_argument(
        "--checkpoint",
        type=Path,
        default=Path("python/checkpoints/nav_ppo.zip"),
        help="Where to save the PPO checkpoint.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    args.checkpoint.parent.mkdir(parents=True, exist_ok=True)

    protocol = gRPCProtocol(url=args.host, port=args.port, environment_start_timeout=45)
    simulator = UnrealEditor()
    env = VecEnv(simulator=simulator, protocol=protocol)

    try:
        model = PPO(
            "MlpPolicy",
            env,
            verbose=1,
            n_steps=256,
            batch_size=64,
            learning_rate=3e-4,
            gamma=0.99,
        )
        model.learn(total_timesteps=args.timesteps)
        model.save(args.checkpoint)
        print(f"Saved checkpoint to {args.checkpoint}")
        return 0
    finally:
        env.close()


if __name__ == "__main__":
    raise SystemExit(main())
