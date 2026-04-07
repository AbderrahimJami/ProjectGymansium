# Vision-Based RL: Issues & Next Steps

## Context

After adding an optional 84×84×3 RGB observation to `AGymansiumNavPawn` (commit `13c6536`), training with 50k timesteps caused the agent to fail during evaluation — mostly going in circles — despite the low-dimensional-only training working well at the same budget.

## Root Causes

### 1. Sample complexity exploded
- Low-dim state: 4 floats → 50k steps is sufficient
- 84×84×3 image: **21,168 features** → vision-based RL typically needs **500k–5M+ timesteps**
- The policy must simultaneously learn to extract visual features *and* navigate

### 2. Architecture mismatch (likely the main culprit)
SB3's default `MultiInputPolicy` for dict observation spaces uses **MLPs for all inputs**, including images. It simply flattens the image into a giant vector — no spatial structure, no weight sharing. This is both computationally wasteful and statistically terrible for pixel inputs.

### 3. Gradient dilution / policy collapse
The poorly-learned image branch adds noise to the gradient signal. The state branch (bearing, distance) was doing all the useful work before — those signals get drowned out, causing the policy to collapse into a degenerate behavior (spinning in place).

## Fix: Combined CNN + MLP Feature Extractor

For a dict obs space with both `state` (4 floats) and `image` (84×84×3), route each through an appropriate encoder and concatenate:

```python
import torch as th
import torch.nn as nn
from stable_baselines3.common.torch_layers import BaseFeaturesExtractor
from gymnasium import spaces

class CombinedExtractor(BaseFeaturesExtractor):
    def __init__(self, observation_space: spaces.Dict):
        super().__init__(observation_space, features_dim=1)  # placeholder

        # Nature CNN for image branch
        self.cnn = nn.Sequential(
            nn.Conv2d(3, 32, kernel_size=8, stride=4), nn.ReLU(),
            nn.Conv2d(32, 64, kernel_size=4, stride=2), nn.ReLU(),
            nn.Conv2d(64, 64, kernel_size=3, stride=1), nn.ReLU(),
            nn.Flatten(),
        )
        with th.no_grad():
            cnn_out = self.cnn(th.zeros(1, 3, 84, 84)).shape[1]

        # MLP for low-dim state branch
        self.state_mlp = nn.Sequential(nn.Linear(4, 64), nn.ReLU())

        self._features_dim = cnn_out + 64

    def forward(self, obs):
        img = obs["image"].permute(0, 3, 1, 2).float()  # NHWC → NCHW
        return th.cat([self.cnn(img), self.state_mlp(obs["state"])], dim=1)


# Usage in train_nav_ppo.py:
policy_kwargs = dict(features_extractor_class=CombinedExtractor)
model = PPO("MultiInputPolicy", env, policy_kwargs=policy_kwargs, verbose=1)
```

## Longer-Term Options

- **Increase timesteps**: Budget at least 1–2M when vision is involved.
- **Curriculum learning**: Train state-only first, then fine-tune with vision added.
- **Pre-trained CNN encoder**: Use a frozen visual encoder rather than learning from scratch — dramatically reduces sample requirements.
- **Reconsider vision**: The 4-float state vector may already be sufficient for this task. Vision might only add value if the environment has visual landmarks or obstacles not captured in the state.
