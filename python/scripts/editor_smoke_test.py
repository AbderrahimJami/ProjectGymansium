from __future__ import annotations

import argparse
from typing import Any

from gymnasium.vector.vector_env import AutoresetMode
from schola.core.protocols.protobuf.gRPC import gRPCProtocol
from schola.core.simulators.unreal.editor import UnrealEditor


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Smoke test the running UE editor via Schola.")
    parser.add_argument("--host", default="localhost", help="Schola host address.")
    parser.add_argument("--port", type=int, default=8000, help="Schola gRPC port.")
    parser.add_argument("--steps", type=int, default=25, help="Maximum random steps to run.")
    parser.add_argument("--seed", type=int, default=1337, help="Reset seed for the environment.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    protocol = gRPCProtocol(url=args.host, port=args.port, environment_start_timeout=45)
    simulator = UnrealEditor()

    protocol.start()
    simulator.start(protocol.properties)
    protocol.send_startup_msg(auto_reset_type=AutoresetMode.DISABLED)

    try:
        ids, agent_types, observation_spaces, action_spaces = protocol.get_definition()
        if not ids or not ids[0]:
            raise RuntimeError("No Schola environments or agents were discovered in the running editor.")

        env_id = 0
        agent_id = ids[env_id][0]
        observation_space = observation_spaces[env_id][agent_id]
        action_space = action_spaces[env_id][agent_id]

        print(f"Connected to env {env_id}, agent {agent_id}")
        print(f"Agent type: {agent_types[env_id][agent_id]}")
        print(f"Observation space: {observation_space}")
        print(f"Action space: {action_space}")

        observations, infos = protocol.send_reset_msg(seeds=[args.seed], options=[{}])
        observation = observations[env_id][agent_id]
        info = infos[env_id][agent_id]

        print("Reset observation:", _format_value(observation))
        print("Reset info:", info)

        for step_index in range(1, args.steps + 1):
            action = action_space.sample()
            observations, rewards, terminateds, truncateds, infos, _, _ = protocol.send_action_msg(
                {env_id: {agent_id: action}},
                {agent_id: action_space},
            )

            observation = observations[env_id][agent_id]
            reward = rewards[env_id][agent_id]
            terminated = terminateds[env_id][agent_id]
            truncated = truncateds[env_id][agent_id]
            info = infos[env_id][agent_id]

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
        protocol.close()
        simulator.stop()


def _format_value(value: Any) -> str:
    if hasattr(value, "tolist"):
        return str(value.tolist())
    return str(value)


if __name__ == "__main__":
    raise SystemExit(main())
