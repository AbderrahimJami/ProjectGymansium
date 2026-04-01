# Python Quickstart

Starter scripts:

- `python/scripts/editor_smoke_test.py`
  Connects to a running UE editor session through the project-local Gymnasium wrapper, resets the environment, samples random actions, and prints step results.
- `python/scripts/train_nav_ppo.py`
  Runs a minimal PPO loop against the running editor using a real `gymnasium.Env`.
- `python/scripts/gym_env.py`
  Thin Gymnasium wrapper around the Schola protocol for the single-agent UE navigation slice.

Typical workflow:

1. Start Play In Editor in Unreal with `GymansiumNavigationEnvironment` and `GymansiumConnectorManager` placed in the map.
2. Run the smoke test first.
3. If that works, run the PPO script.
