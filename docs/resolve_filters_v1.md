# DaVinci Resolve Effects (V1 parameter mapping)

This document describes how Resolve FX / Fusion filters are exposed in PerformanieVJ V1.

## Scope

- **Included:** Resolve FX (missing entries only), Fusion Effects, Fusion Generators
- **Excluded:** Titles, Text+, Transitions, Adjustment Clips (separate features)

Machine-readable inventory: [`tools/resolve_effects_inventory.json`](../tools/resolve_effects_inventory.json)

## Parameter convention (4-param limit)

Every new Resolve catalog entry uses:

| Index | Name | Kind | Role |
|-------|------|------|------|
| 0 | `blend` | Percent (0–1, default 1.0) | Resolve-style mix between source and effect |
| 1 | `strength` | Percent | Primary effect intensity |
| 2 | `detail` | Float 0–1 | Secondary control (frequency, edge weight, etc.) |
| 3 | `size` | Float 0–1 | Tertiary control (radius, tile size, patch offset, …) |

Full Resolve parameter sets are not replicated in V1 due to the `CellFilterNode` four-parameter cap.

### Exceptions

- **`noise_reduction`:** Uses Maxine backend (NVIDIA Video Effects SDK denoising). Params: `blend`, `strength` (Weak/Strong), `amount`.
- **`dctl`:** Utility stub — slight exposure/gamma approximation only (no DCTL script execution).

## Shader families

| Resolve group | Shader family | Notes |
|---------------|---------------|-------|
| Blur / Sharpen | `effect_blur.frag` | familyId 11–14 |
| Color | `effect_color.frag` | familyId 25–35 |
| Generate / Fusion Generators | `effect_generate.frag` | familyId 18–24 |
| Grid | `effect_pattern.frag` | familyId 5 |
| Key | `effect_key.frag` | familyId 2–4 |
| Light | `effect_light.frag` | new family |
| Revival / Refine | `effect_revival.frag` | new family |
| Temporal | `effect_temporal.frag` | new family |
| Texture / Stylize / Fusion overlays | `effect_stylize.frag` | familyId 24–41 |
| Transform | `effect_transform.frag` | familyId 13–14 |
| Warp | `effect_distort.frag` | familyId 17–22 |

All GLSL paths apply `mix(original, effect, blend)` (or equivalent alpha blend for keys).

## Duplicates

Existing Resolume-style filters (151 entries) are unchanged. Resolve names that overlap visually (e.g. `god_rays` vs `light_rays`) remain separate `typeId`s with independent schemas.
