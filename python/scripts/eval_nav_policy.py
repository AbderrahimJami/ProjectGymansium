from __future__ import annotations

import argparse
import time
from pathlib import Path
from typing import Any

from gymnasium.vector.vector_env import AutoresetMode
from schola.core.protocols.protobuf.gRPC import gRPCProtocol
from schola.core.simulators.unreal.editor import UnrealEditor
from stable_baselines3 import PPO


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

    model = PPO.load(args.checkpoint)

    protocol = gRPCProtocol(url=args.host, port=args.port, environment_start_timeout=45)
    simulator = UnrealEditor()

    protocol.start()
    simulator.start(protocol.properties)
    protocol.send_startup_msg(auto_reset_type=AutoresetMode.DISABLED)

    try:
        ids, _, observation_spaces, action_spaces = protocol.get_definition()
        if not ids or not ids[0]:
            raise RuntimeError("No Schola environments or agents were discovered in the running editor.")

        env_id = 0
        agent_id = ids[env_id][0]
        action_space = action_spaces[env_id][agent_id]
        observation_space = observation_spaces[env_id][agent_id]

        print(f"Connected to env {env_id}, agent {agent_id}")
        print(f"Observation space: {observation_space}")
        print(f"Action space: {action_space}")

        successes = 0
        total_reward = 0.0
        total_steps = 0

        for episode_index in range(1, args.episodes + 1):
            observations, infos = protocol.send_reset_msg(seeds=[args.seed + episode_index - 1], options=[{}])
            observation = observations[env_id][agent_id]
            reset_info = infos[env_id][agent_id]
            if args.verbose:
                print(f"episode={episode_index} reset obs={_format_value(observation)} info={reset_info}")

            episode_reward = 0.0
            episode_length = 0

            while True:
                action, _ = model.predict(observation, deterministic=args.deterministic)
                observations, rewards, terminateds, truncateds, infos, _, _ = protocol.send_action_msg(
                    {env_id: {agent_id: action}},
                    {agent_id: action_space},
                )

                observation = observations[env_id][agent_id]
                reward = float(rewards[env_id][agent_id])
                terminated = bool(terminateds[env_id][agent_id])
                truncated = bool(truncateds[env_id][agent_id])
                info = infos[env_id][agent_id]

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
        protocol.close()
        simulator.stop()


def _format_value(value: Any) -> str:
    if hasattr(value, "tolist"):
        return str(value.tolist())
    return str(value)


if __name__ == "__main__":
    raise SystemExit(main())
