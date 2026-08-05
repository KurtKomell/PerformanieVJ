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

## Resolve FX Temporal (Resolve 21 handbook)

GPU path: `effect_temporal.frag` via `packTemporalUniforms()`. Per-layer **frame-history ring** (16 slots) in `RhiMixerWidget`; shader binding `u_history` = previous frame. **No optical flow** — Smear and Motion Blur use frame-difference approximations.

| typeId | Display name | `familyId` |
|--------|--------------|------------|
| `motion_trails` | Motion Trails | 0 |
| `smear` | Smear | 1 |
| `stop_motion` | Stop Motion | 2 |
| `motion_blur` | Motion Blur | 3 |

### Parameter mapping (handbook → schema)

**motion_trails:** `trail_length`, `dropoff`, `composite_gamma` (Timeline / Rec.709 / Linear / Custom), `composite_gamma_custom`, `pan`, `pan_angle`, `zoom`, `rotate`, `reuse_current_frame`, `border_type` (Black / Soften / Replicate / Reflect / Wrap-Around), `input_alpha`, `use_alpha`, `blend`

**smear:** `frames_either_side`, `luma_threshold`, `chroma_threshold`, `input_alpha`, `use_alpha`, `blend`

**stop_motion:** `frame_hold`, `input_alpha`, `use_alpha`, `blend` (`frame_hold` = output frames per source frame; ring updates only on hold boundaries)

**motion_blur:** `motion_est_type` (Better / Faster), `motion_range`, `motion_blur`, `blur_direction` (Both Directions / From Previous Frame / Towards Next Frame), `granularity`, `blend`

## Resolve FX Texture (Resolve 21 handbook)

GPU path: `effect_texture.frag` via `packTextureUniforms()`. Studio-only effects in Resolve; implementations are procedural GPU approximations tuned for live VJ use.

| typeId | Display name | `familyId` |
|--------|--------------|------------|
| `jpeg_damage` | JPEG Damage | 0 |
| `texture_pop` | Texture Pop | 1 |
| `film_damage` | Film Damage | 2 |
| `analog_damage` | Analog Damage | 3 |

### Parameter mapping (handbook → schema)

**jpeg_damage:** `quality`, `resolution`, `block_aspect_ratio`, `frequency_scale`, `scale_component` (All / X / Y Frequencies), `blend`

**texture_pop:** `mode` (Simple / Advanced), `details` (Simple, −1…1), `rough`…`tiny` (Advanced, −1…1), `strength`, `shadows` / `midtones` / `highlights` (Tonal Range), `blend`

**film_damage:** `film_blur`, `temp_shift`, `tint_shift`; vignetting `focal_factor`, `geometry_factor`, `tilt_amount`, `tilt_angle`; dirt `dirt_color`, `changing_dirt`, `dirt_density`, `dirt_size`, `dirt_blur`, `dirt_seed`; five scratch groups `scratchN_*` (color, position, width, strength, blur, moving, moving_amplitude, moving_speed, moving_randomness, flickering_speed); `blend`

**analog_damage:** `preset` (Custom, VHS, Bad Reception, Old TV, Damaged Tape, Security Cam); Telecine `vignetting`, `vignette_aspect`, `shutter_weave`; Broadcast `noise_scale`, `signal_noise`, `chroma_noise`, `detail_loss`, `chroma_detail_loss`, `ghosting`, `ghost_offset`, `chroma_misalignment`; Color Dials `brightness`, `contrast`, `color`, `tint`; Scan `image_aspect`, `h_shift`, `v_shift`, `v_hold`, `v_hold_latch`, `overscan`, `v_scale`, `vertical_blanking`; Scan Lines `line_sharpness`, `line_frequency`, `colored_lines`; TV `phosphor_brightness`, `phosphor_tint`, `defocus`, `screen_curvature`, `edge_mask`, `edges_transparent`, `mask_curvature`, `mask_aspect`; VHS `restless_foot_height`, `restless_foot_offset`, `restless_foot_jitter`; `blend`

### Resolve 21 QA checklist (manual compare)

1. Import a still with skin texture + fine mechanical detail (Resolve Texture Pop handbook examples).
2. Per effect: default, one extreme, one preset (`analog_damage` presets 1–5).
3. Compare macroblocking (`jpeg_damage`), midtone detail bands (`texture_pop`), dirt/scratches (`film_damage`), scan/VHS (`analog_damage`).
4. Document residual differences under “Known approximations” only if algorithmically blocked (no Resolve binary).
