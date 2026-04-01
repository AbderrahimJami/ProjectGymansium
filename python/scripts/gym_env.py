from __future__ import annotations

from typing import Any

import gymnasium as gym
from gymnasium.vector.vector_env import AutoresetMode
from schola.core.protocols.protobuf.gRPC import gRPCProtocol
from schola.core.simulators.unreal.editor import UnrealEditor


class UnrealScholaGymEnv(gym.Env):
    metadata = {"render_modes": []}

    def __init__(self, host: str = "localhost", port: int = 8000, timeout_seconds: int = 45):
        self.protocol = gRPCProtocol(url=host, port=port, environment_start_timeout=timeout_seconds)
        self.simulator = UnrealEditor()

        self.protocol.start()
        self.simulator.start(self.protocol.properties)
        self.protocol.send_startup_msg(auto_reset_type=AutoresetMode.DISABLED)

        ids, agent_types, observation_spaces, action_spaces = self.protocol.get_definition()
        if not ids or not ids[0]:
            self.close()
            raise RuntimeError("No Schola environments or agents were discovered in the running editor.")

        if len(ids) != 1 or len(ids[0]) != 1:
            self.close()
            raise RuntimeError("UnrealScholaGymEnv only supports one environment with one agent.")

        self.env_id = 0
        self.agent_id = ids[0][0]
        self.agent_type = agent_types[self.env_id][self.agent_id]
        self.observation_space = observation_spaces[self.env_id][self.agent_id]
        self.action_space = action_spaces[self.env_id][self.agent_id]

    def reset(self, *, seed: int | None = None, options: dict[str, str] | None = None) -> tuple[Any, dict[str, str]]:
        super().reset(seed=seed)
        observations, infos = self.protocol.send_reset_msg(
            seeds=[seed] if seed is not None else None,
            options=[options or {}],
        )
        return observations[self.env_id][self.agent_id], infos[self.env_id][self.agent_id]

    def step(self, action: Any) -> tuple[Any, float, bool, bool, dict[str, str]]:
        observations, rewards, terminateds, truncateds, infos, _, _ = self.protocol.send_action_msg(
            {self.env_id: {self.agent_id: action}},
            {self.agent_id: self.action_space},
        )

        return (
            observations[self.env_id][self.agent_id],
            float(rewards[self.env_id][self.agent_id]),
            bool(terminateds[self.env_id][self.agent_id]),
            bool(truncateds[self.env_id][self.agent_id]),
            infos[self.env_id][self.agent_id],
        )

    def close(self) -> None:
        if hasattr(self, "protocol") and self.protocol is not None:
            self.protocol.close()
        if hasattr(self, "simulator") and self.simulator is not None:
            self.simulator.stop()
