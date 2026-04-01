# Python Quickstart

Starter scripts:

- `python/scripts/editor_smoke_test.py`
  Connects to a running UE editor session over Schola gRPC, resets the environment, samples random actions, and prints step results.
- `python/scripts/train_nav_ppo.py`
  Runs a minimal PPO loop against the running editor using Schola's SB3 vector environment.

Typical workflow:

1. Start Play In Editor in Unreal with `GymansiumNavigationEnvironment` and `GymansiumConnectorManager` placed in the map.
2. Run the smoke test first.
3. If that works, run the PPO script.
