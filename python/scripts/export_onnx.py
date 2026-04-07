"""Export a trained SB3 PPO policy to ONNX for deployment in Unreal Engine via NNE.

Usage:
    .\\python\\.condaenv\\python.exe .\\python\\scripts\\export_onnx.py \\
        --checkpoint python/checkpoints/nav_ppo_seed1337_<timestamp>.zip \\
        --output python/checkpoints/policy.onnx

The exported model takes a flat float32 observation vector and outputs a
float32 action vector (throttle, turn), matching the inference done by
AGymansiumNNEController in UE.

Once exported, import the .onnx file into the UE Content Browser — the engine
will create a UNNEModelData asset. Assign that asset to AGymansiumNNEController
in the level and set ObservationDim to match your training obs size.
"""

from __future__ import annotations

import argparse
from pathlib import Path

import torch
import torch.nn as nn
from stable_baselines3 import PPO


class DeterministicActorExporter(nn.Module):
    """Wraps an SB3 MlpPolicy actor for ONNX export.

    Mirrors the deterministic forward pass used during evaluation:
      observation -> features -> mlp_extractor (actor head) -> action_net -> clamp
    """

    def __init__(self, policy) -> None:
        super().__init__()
        self.features_extractor = policy.pi_features_extractor
        self.mlp_extractor_actor = policy.mlp_extractor
        self.action_net = policy.action_net

    def forward(self, obs: torch.Tensor) -> torch.Tensor:
        features = self.features_extractor(obs)
        latent_pi, _ = self.mlp_extractor_actor(features)
        actions = self.action_net(latent_pi)
        # Clamp to [-1, 1] — SB3 PPO default (no tanh squashing)
        return torch.clamp(actions, -1.0, 1.0)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Export SB3 PPO policy to ONNX.")
    parser.add_argument("--checkpoint", type=Path, required=True, help="Path to .zip SB3 checkpoint.")
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("python/checkpoints/policy.onnx"),
        help="Output path for the ONNX file.",
    )
    parser.add_argument(
        "--obs-dim",
        type=int,
        default=4,
        help="Observation vector size (must match training). Default 4 (state-only).",
    )
    parser.add_argument(
        "--opset",
        type=int,
        default=17,
        help="ONNX opset version. UE NNEOnnxRuntime supports opset 9-17.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    if not args.checkpoint.exists():
        print(f"Error: checkpoint not found at {args.checkpoint}")
        return 1

    args.output.parent.mkdir(parents=True, exist_ok=True)

    print(f"Loading checkpoint: {args.checkpoint}")
    model = PPO.load(str(args.checkpoint))
    model.policy.eval()

    exporter = DeterministicActorExporter(model.policy)
    exporter.eval()

    dummy_obs = torch.zeros(1, args.obs_dim, dtype=torch.float32)

    # Verify forward pass before exporting
    with torch.no_grad():
        test_output = exporter(dummy_obs)
    action_dim = test_output.shape[-1]
    print(f"Forward pass ok — obs_dim={args.obs_dim}, action_dim={action_dim}")

    torch.onnx.export(
        exporter,
        dummy_obs,
        str(args.output),
        input_names=["observation"],
        output_names=["action"],
        dynamic_axes={
            "observation": {0: "batch_size"},
            "action": {0: "batch_size"},
        },
        opset_version=args.opset,
        do_constant_folding=True,
    )

    print(f"Exported ONNX model to: {args.output}")
    print(f"  Input:  observation [{args.obs_dim}] float32")
    print(f"  Output: action      [{action_dim}] float32  (throttle, turn)")
    print()
    print("Next steps:")
    print("  1. Import the .onnx file into the UE Content Browser")
    print("  2. Place AGymansiumNNEController in your level")
    print("  3. Assign the resulting UNNEModelData asset to PolicyModelData")
    print(f"  4. Set ObservationDim = {args.obs_dim} on the controller")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
