from __future__ import annotations

import argparse
import time
from pathlib import Path

from stable_baselines3 import PPO

from gym_env import UnrealScholaGymEnv


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Evaluate a trained PPO navigation policy against the running UE editor.")
    parser.add_argument("--host", default="localhost", help="Schola host address.")
    parser.add_argument("--port", type=int, default=8000, help="Schola gRPC port.")
    parser.add_argument(
        "--checkpoint",
        type=Path,
        default=Path("python/checkpoints/nav_ppo.zip"),
        help="Path to the PPO checkpoint.",
    )
    parser.add_argument("--episodes", type=int, default=5, help="Number of episodes to run.")
    parser.add_argument("--deterministic", action="store_true", help="Use deterministic policy actions.")
    parser.add_argument("--sleep", type=float, default=0.0, help="Optional delay in seconds after each environment step.")
    parser.add_argument("--verbose", action="store_true", help="Print step-by-step inference data.")
    parser.add_argument("--seed", type=int, default=1337, help="Base reset seed.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if not args.checkpoint.is_file():
        raise FileNotFoundError(f"Checkpoint not found: {args.checkpoint}")

    env = UnrealScholaGymEnv(host=args.host, port=args.port, timeout_seconds=45)

    try:
        model = PPO.load(args.checkpoint, env=env)

        print(f"Connected to env 0, agent {env.agent_id}")
        print(f"Observation space: {env.observation_space}")
        print(f"Action space: {env.action_space}")

        successes = 0
        total_reward = 0.0
        total_steps = 0

        for episode_index in range(1, args.episodes + 1):
            observation, reset_info = env.reset(seed=args.seed + episode_index - 1)
            if args.verbose:
                print(f"episode={episode_index} reset obs={_format_value(observation)} info={reset_info}")

            episode_reward = 0.0
            episode_length = 0

            while True:
                action, _ = model.predict(observation, deterministic=args.deterministic)
                observation, reward, terminated, truncated, info = env.step(action)

                episode_reward += reward
                episode_length += 1

                if args.verbose:
                    print(
                        f"step={episode_length} action={_format_value(action)} reward={reward:.4f} "
                        f"terminated={terminated} truncated={truncated} info={info} obs={_format_value(observation)}"
                    )

                if args.sleep > 0.0:
                    time.sleep(args.sleep)

                if not (terminated or truncated):
                    continue

                success = terminated and not truncated
                successes += int(success)
                total_reward += episode_reward
                total_steps += episode_length

                outcome = "success" if success else "timeout"
                distance = info.get("distance", "n/a")
                print(
                    f"episode={episode_index} outcome={outcome} "
                    f"reward={episode_reward:.3f} steps={episode_length} final_distance={distance}"
                )
                break

        average_reward = total_reward / max(args.episodes, 1)
        average_steps = total_steps / max(args.episodes, 1)
        success_rate = successes / max(args.episodes, 1)
        print(
            f"summary successes={successes}/{args.episodes} "
            f"success_rate={success_rate:.2%} avg_reward={average_reward:.3f} avg_steps={average_steps:.1f}"
        )
        return 0
    finally:
        env.close()


def _format_value(value: object) -> str:
    if hasattr(value, "tolist"):
        return str(value.tolist())
    return str(value)


if __name__ == "__main__":
    raise SystemExit(main())
