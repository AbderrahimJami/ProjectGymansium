from __future__ import annotations

import argparse
from typing import Any

from gym_env import UnrealScholaGymEnv


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Smoke test the running UE editor via Schola.")
    parser.add_argument("--host", default="localhost", help="Schola host address.")
    parser.add_argument("--port", type=int, default=8000, help="Schola gRPC port.")
    parser.add_argument("--steps", type=int, default=25, help="Maximum random steps to run.")
    parser.add_argument("--seed", type=int, default=1337, help="Reset seed for the environment.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    env = UnrealScholaGymEnv(host=args.host, port=args.port, timeout_seconds=45)

    try:
        print(f"Connected to env 0, agent {env.agent_id}")
        print(f"Agent type: {env.agent_type}")
        print(f"Observation space: {env.observation_space}")
        print(f"Action space: {env.action_space}")

        observation, info = env.reset(seed=args.seed)

        print("Reset observation:", _format_value(observation))
        print("Reset info:", info)

        for step_index in range(1, args.steps + 1):
            action = env.action_space.sample()
            observation, reward, terminated, truncated, info = env.step(action)

            print(
                f"step={step_index} action={_format_value(action)} "
                f"reward={reward:.4f} terminated={terminated} truncated={truncated} "
                f"obs={_format_value(observation)} info={info}"
            )

            if terminated or truncated:
                print("Episode finished.")
                return 0

        return 0
    finally:
        env.close()


def _format_value(value: Any) -> str:
    if hasattr(value, "tolist"):
        return str(value.tolist())
    return str(value)


if __name__ == "__main__":
    raise SystemExit(main())
