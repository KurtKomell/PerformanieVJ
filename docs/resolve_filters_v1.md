# Resolve-style filters (PerformanieVJ)

## Blur filters (Resolve 21 baseline)

Phase 1: seven blur effects registered:

| typeId | Display name | Shader `familyId` |
|--------|--------------|-------------------|
| `gaussian_blur` | Gaussian Blur | 0 |
| `box_blur` | Box Blur | 1 |
| `directional_blur` | Directional Blur | 2 |
| `radial_blur` | Radial Blur | 3 |
| `zoom_blur` | Zoom Blur | 4 |
| `mosaic_blur` | Mosaic Blur | 5 |
| `lens_blur` | Lens Blur | 6 |

GPU path: `RhiMixerWidget` filter chain → `effect_blur.frag` via `FilterUniformPacker`.

## Film Emulation (Resolve 21 Film Look Creator)

Resolve 21 exposes one **Film Look Creator** effect under **ResolveFX Film Emulation**. PerformanieVJ splits it into separate catalog effects (same parameter names as the Resolve manual sections). Film stock presets are **visual approximations**, not licensed Blackmagic profiles.

| typeId | Display name | Shader family | `familyId` | Resolve FLC section |
|--------|--------------|---------------|------------|---------------------|
| `film_look` | Film Look | Film | 0 | Film Look / Core Looks |
| `film_color` | Film Color | Film | 1 | Color Settings |
| `film_split_tone` | Film Split Tone | Film | 2 | Split Tone |
| `film_vignette` | Film Vignette | Film | 3 | Vignette |
| `film_halation` | Film Halation | Film | 4 | Halation |
| `film_bloom` | Film Bloom | Film | 5 | Bloom |
| `film_grain` | Film Grain | Film | 6 | Grain |
| `film_flicker` | Film Flicker | Film | 7 | Flicker |
| `film_gate_weave` | Film Gate Weave | Film | 8 | Gate Weave |
| `film_gate` | Film Gate | Film | 9 | Film Gate |

GPU path: `effect_film.frag` via `packFilmUniforms()` (80-byte UBO with `params3` for `film_color`).

### Parameter mapping (handbook → schema)

**film_look:** `film_look` (Default 65mm, Default 35mm, Cinematic, Nostalgic, Bleach Bypass, Rochester, Akasaka, Elated, Vintage, Aurora), `blend` (Film Look Blend)

**film_color:** `exposure`, `contrast`, `highlights_fade`, `fade_rolloff` (Resolve 21), `temperature`, `tint`, `subtractive_saturation`, `saturation`, `richness`, `blend` (Color Blend)

**film_split_tone:** `amount`, `hue_angle`, `balance`, `blend`

**film_vignette:** `amount`, `size`, `softness`, `roundness`, `blend`

**film_halation:** `amount`, `threshold`, `size`, `hue`, `blend`

**film_bloom:** `amount`, `threshold`, `size`, `blend`

**film_grain:** `size`, `strength`, `monochrome`, `blend`

**film_flicker:** `amount`, `speed`, `blend`

**film_gate_weave:** `amount_h`, `amount_v`, `speed`, `blend`

**film_gate:** `ratio` (4:3, 16:10, 16:9, 1.85, 2.39, Super 16, Super 8), `padding`, `softness` (Resolve 21), `blend`

Keying-only nodes (`chroma_key`, `luma_key`, `mask`) remain in `FilterEffectIds` for layer keying injection but are not listed in the filter picker.
