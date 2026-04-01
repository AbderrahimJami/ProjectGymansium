# ProjectGymansium

Developed with Unreal Engine 5

## Intended Structure

- `Plugins/Schola/` for the Schola Unreal plugin source
- `python/` for external Gymnasium training scripts and Python dependencies
- `Source/` for project C++
- `Content/` for maps, Blueprints, and assets

## Current Prototype Slice

The first Schola environment slice lives in `Source/ProjectGymansium/Training/`:

- `AGymansiumNavigationEnvironment` implements `ISingleAgentScholaEnvironment`
- `AGymansiumNavPawn` is a simple move-and-turn pawn
- `AGymansiumGoalActor` is the target actor
- `AGymansiumConnectorManager` wraps Schola's `AGymConnectorManager` with a default `URPCGymConnector`

The current environment exposes a 4-float observation:

- normalized distance to goal
- signed bearing to goal
- normalized speed
- collision flag

And a 2-float continuous action:

- throttle in `[-1, 1]`
- turn in `[-1, 1]`

On the Python side, this vertical slice now uses a direct Gymnasium wrapper in `python/scripts/gym_env.py`.
That wrapper exposes the UE environment as a real `gymnasium.Env`, and the PPO training/evaluation
scripts use that wrapper rather than Schola's SB3 vector environment.

## First Editor Smoke Test

1. Create or open a simple test map with a floor so the pawn has something to collide against.
2. Place one `GymansiumNavigationEnvironment` actor in the level.
3. Place one `GymansiumConnectorManager` actor in the level.
4. On the connector manager, leave the default `RPCConnector` in place.
5. Set the connector port if needed. The default Schola port is `8000`.
6. Press Play in Editor.
7. In another shell, activate the project Python environment and run:

```powershell
.\python\.condaenv\python.exe .\python\scripts\editor_smoke_test.py --port 8000
```

That should connect to the running editor, request the Schola environment definition, reset the episode, sample random actions, and print reward/termination data.

## Notes

- Schola `2.0.1` builds cleanly here against UE `5.7.4`.
- `StructUtils` is currently listed because Schola depends on it, although Unreal marks that plugin deprecated in newer engine versions.
- Schola's built-in `schola.gym.env` currently imports `ray` at module import time in this setup.
  To avoid that dependency leak, this project uses its own thin `gymnasium.Env` wrapper over the working Schola protocol layer.
