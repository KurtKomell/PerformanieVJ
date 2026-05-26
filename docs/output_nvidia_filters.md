# NVIDIA output filters (post-mix chain)

NVIDIA Maxine filters are configured **only** on the **Output** inspector tab as a project-wide filter chain. They run on the **fully composited mixer image** (all layers and feedback already blended), immediately **before** preview B and fullscreen present.

## Pipeline order

1. Per-layer cell filter chains (shader / non-NVIDIA)
2. Layer mixer composite → `m_sceneRt`
3. Feedback history capture (unchanged; **not** passed through the output chain)
4. Output NVIDIA chain (Output tab)
5. Present to preview B / fullscreen

## Allowed filters

Only catalog entries in the **NVIDIA** category that use the Maxine backend are valid in `Settings.output.filterChain`, for example:

- `nvidia_artifact_reduction`
- `nvidia_super_resolution`
- `nvidia_upscale`
- `noise_reduction` (Video Denoise)

NVIDIA filters **do not** appear in the per-cell filter editor. Loading older projects strips Maxine/NVIDIA nodes from cell chains automatically.

## Runtime

Maxine filters require the NVIDIA Video Effects runtime (`NVVideoEffects.dll` and models) or a build with `MAXINE_SDK_ROOT`. When unavailable, the output chain falls back to passthrough copy for those nodes.

Integration follows the NVIDIA VFX SDK sample pattern: `NvVFX_CreateEffect` → model directory (except Upscale) → CUDA stream → `NvCVImage` GPU buffers → `NvVFX_Load` → `NvVFX_Run`. Denoise uses `NvVFX_AllocateState` for temporal state. Upscale uses RGBA U8 interleaved GPU buffers; AR/SR/Denoise use BGR F32 planar.

## Persistence

The chain is stored under `<Settings><Output><FilterChain>…</Output></Settings>` in `.pvj` files.
