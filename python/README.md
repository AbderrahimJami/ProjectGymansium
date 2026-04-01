# Python Workspace

This directory is for the external training side of the project.

Expected responsibilities:

- launch or connect to the Unreal environment through Schola
- define training scripts and experiment configs
- hold Python-only dependencies and virtual environment metadata
- keep logs, checkpoints, and notebooks outside the Unreal project structure

Suggested layout:

- `python/requirements.txt` or `python/pyproject.toml`
- `python/scripts/` for entry points such as training and evaluation
- `python/configs/` for experiment settings
- `python/checkpoints/` for local model outputs if you do not use an external tracker

The Unreal side should stay in:

- `Source/` for C++
- `Content/` for maps, Blueprints, and assets
- `Plugins/Schola/` for the Schola plugin source
