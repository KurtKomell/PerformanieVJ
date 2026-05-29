# Filter-Parameter-Review

Automatisch generiert aus `FilterCatalog.cpp`, `FilterParamSchema.cpp`, `FilterEffectIds.cpp`, `FilterUniformPacker.cpp`.

Regenerieren: `python tools/generate_filter_param_review.py`

**Gesamt:** 221 Katalog-Einträge | **221** mit GPU/Maxine-Metadaten | **0** nur Katalog (kein Shader) | **70** Resolve/Fusion-Stub-Schema (blend/strength/detail/size)

## Legende

| Spalte | Bedeutung |
|--------|-----------|
| Key | Parameter-Name in Projekt/MIDI |
| Label | Anzeige im Inspector |
| Typ | Float, Percent (0–1), Angle (°), Color, Bool, EnumIndex |
| Bereich | Min–Max oder Enum-Liste |
| Default | Standardwert |

---


## Blur & sharpen

### 1/221 — Blur (`blur`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.75 |
| `radius` | Radius | Float | 0.0–128.0 | 12.0 |

**Shader** — `effect_blur.frag`; `familyId=0`; 2 interne Pässe; **Spezial** —  amount/radius/angle → UV-Radius, 2-Pass separable (params2[1] axis)

---

### 2/221 — Fast Blur (`fast_blur`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.75 |
| `radius` | Radius | Float | 0.0–128.0 | 12.0 |

**Shader** — `effect_blur.frag`; `familyId=1`; **Spezial** —  amount/radius/angle → UV-Radius, 2-Pass separable (params2[1] axis)

---

### 3/221 — Gaussian Blur (`gaussian_blur`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.75 |
| `radius` | Radius | Float | 0.0–128.0 | 12.0 |

**Shader** — `effect_blur.frag`; `familyId=2`; 2 interne Pässe; **Spezial** —  amount/radius/angle → UV-Radius, 2-Pass separable (params2[1] axis)

---

### 4/221 — Box Blur (`box_blur`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.75 |
| `radius` | Radius | Float | 0.0–128.0 | 12.0 |

**Shader** — `effect_blur.frag`; `familyId=3`; 2 interne Pässe; **Spezial** —  amount/radius/angle → UV-Radius, 2-Pass separable (params2[1] axis)

---

### 5/221 — Radial Blur (`radial_blur`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.75 |
| `radius` | Radius | Float | 0.0–128.0 | 10.0 |
| `angle` | Angle | Angle | -180°–180° | 0.0 |

**Shader** — `effect_blur.frag`; `familyId=4`; **Spezial** —  amount/radius/angle → UV-Radius, 2-Pass separable (params2[1] axis)

---

### 6/221 — Directional Blur (`directional_blur`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.75 |
| `radius` | Radius | Float | 0.0–128.0 | 10.0 |
| `angle` | Angle | Angle | -180°–180° | 0.0 |

**Shader** — `effect_blur.frag`; `familyId=5`; **Spezial** —  amount/radius/angle → UV-Radius, 2-Pass separable (params2[1] axis)

---

### 7/221 — Motion Blur (`motion_blur`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.75 |
| `radius` | Radius | Float | 0.0–128.0 | 10.0 |
| `angle` | Angle | Angle | -180°–180° | 0.0 |

**Shader** — `effect_blur.frag`; `familyId=6`; **Spezial** —  amount/radius/angle → UV-Radius, 2-Pass separable (params2[1] axis)

---

### 8/221 — Zoom Blur (`zoom_blur`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.75 |
| `radius` | Radius | Float | 0.0–128.0 | 10.0 |
| `angle` | Angle | Angle | -180°–180° | 0.0 |

**Shader** — `effect_blur.frag`; `familyId=7`; **Spezial** —  amount/radius/angle → UV-Radius, 2-Pass separable (params2[1] axis)

---

### 9/221 — Temporal Blur (`temporal_blur`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.75 |
| `radius` | Radius | Float | 0.0–128.0 | 12.0 |

**Shader** — `effect_blur.frag`; `familyId=8`; **Spezial** —  amount/radius/angle → UV-Radius, 2-Pass separable (params2[1] axis)

---

### 10/221 — Sharpen (`sharpen`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `sharpen_amount` | Sharpen Amount | Float | 0.0–5.0 | 1.8 |
| `fine_detail_size` | Fine Detail Size | Float | 0.0–1.0 | 0.05 |
| `fine_detail` | Fine Detail | Float | 0.0–2.0 | 1.0 |
| `medium_details` | Medium Details | Float | 0.0–2.0 | 1.0 |
| `large_details` | Large Details | Float | 0.0–2.0 | 1.0 |
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |

**Shader** — `effect_blur.frag`; `familyId=7`; **Spezial** — multi-scale unsharp (fine/medium/large bands), `packBlurUniforms`

---

### 11/221 — Denoise (`denoise`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.3 |

**Shader** — `effect_blur.frag`; `familyId=10`; **Spezial** —  amount/radius/angle → UV-Radius, 2-Pass separable (params2[1] axis)

---


## Color & levels

### 12/221 — Color (`color`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 1.0 |
| `brightness` | Brightness | Float | -1.0–1.0 | 0.0 |
| `contrast` | Contrast | Float | 0.0–2.0 | 1.0 |
| `saturation` | Saturation | Float | 0.0–2.0 | 1.0 |

**Shader** — `effect_color.frag`; `familyId=0`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 13/221 — Color Correction (`color_correction`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 1.0 |
| `brightness` | Brightness | Float | -1.0–1.0 | 0.0 |
| `contrast` | Contrast | Float | 0.0–2.0 | 1.0 |
| `saturation` | Saturation | Float | 0.0–2.0 | 1.0 |

**Shader** — `effect_color.frag`; `familyId=1`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 14/221 — Color Intensity (`color_intensity`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `intensity` | Intensity | Float | 0.0–2.0 | 1.0 |

**Shader** — `effect_color.frag`; `familyId=2`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 15/221 — Color Balance (`color_balance`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `red` | Red | Color |  | 1.0 |
| `green` | Green | Color |  | 1.0 |
| `blue` | Blue | Color |  | 1.0 |

**Shader** — `effect_color.frag`; `familyId=3`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 16/221 — Brightness (`brightness`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `value` | Brightness | Float | -1.0–1.0 | 0.0 |

**Shader** — `effect_color.frag`; `familyId=4`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 17/221 — Contrast (`contrast`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `value` | Contrast | Float | 0.0–2.0 | 1.0 |

**Shader** — `effect_color.frag`; `familyId=5`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 18/221 — Gamma (`gamma`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `value` | Gamma | Float | 0.1–4.0 | 1.0 |

**Shader** — `effect_color.frag`; `familyId=6`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 19/221 — Exposure (`exposure`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `value` | Exposure | Float | -4.0–4.0 | 0.0 |

**Shader** — `effect_color.frag`; `familyId=7`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 20/221 — Saturation (`saturation`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `value` | Saturation | Float | 0.0–2.0 | 1.0 |

**Shader** — `effect_color.frag`; `familyId=8`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 21/221 — Hue (`hue`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `angle` | Hue | Angle | -180°–180° | 0.0 |

**Shader** — `effect_color.frag`; `familyId=9`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 22/221 — HSL Adjust (`hsl`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 1.0 |
| `brightness` | Brightness | Float | -1.0–1.0 | 0.0 |
| `contrast` | Contrast | Float | 0.0–2.0 | 1.0 |
| `saturation` | Saturation | Float | 0.0–2.0 | 1.0 |

**Shader** — `effect_color.frag`; `familyId=10`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 23/221 — Curves (`curves`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 1.0 |
| `brightness` | Brightness | Float | -1.0–1.0 | 0.0 |
| `contrast` | Contrast | Float | 0.0–2.0 | 1.0 |
| `saturation` | Saturation | Float | 0.0–2.0 | 1.0 |

**Shader** — `effect_color.frag`; `familyId=11`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 24/221 — Levels (`levels`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 1.0 |
| `brightness` | Brightness | Float | -1.0–1.0 | 0.0 |
| `contrast` | Contrast | Float | 0.0–2.0 | 1.0 |
| `saturation` | Saturation | Float | 0.0–2.0 | 1.0 |

**Shader** — `effect_color.frag`; `familyId=12`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 25/221 — Invert (`invert`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `enabled` | Enabled | Bool | 0–1 | true |

**Shader** — `effect_color.frag`; `familyId=13`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 26/221 — Solarize (`solarize`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `threshold` | Threshold | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_color.frag`; `familyId=14`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 27/221 — Tint (`tint`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `hue` | Hue | Angle | -180°–180° | 0.0 |
| `strength` | Strength | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_color.frag`; `familyId=15`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 28/221 — Black & White (`black_white`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `mix` | Mix | Float | 0.0–1.0 | 1.0 |

**Shader** — `effect_color.frag`; `familyId=16`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 29/221 — Posterize (`posterize`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `levels` | Levels | Float | 2.0–64.0 | 8.0 |

**Shader** — `effect_color.frag`; `familyId=17`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 30/221 — Threshold (`threshold`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `value` | Threshold | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_color.frag`; `familyId=18`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 31/221 — Colorize (`colorize`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `hue` | Hue | Angle | -180°–180° | 0.0 |
| `brightness` | Brightness | Float | -1.0–1.0 | 0.0 |
| `saturation` | Saturation | Float | 0.0–1.0 | 1.0 |

**Shader** — `effect_color.frag`; `familyId=19`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 32/221 — Chromatic Aberration (`chromatic_aberration`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `distance` | Distance | Float | 0.0–100.0 | 2.0 |
| `angle` | Angle | Angle | -180°–180° | 0.0 |

**Shader** — `effect_color.frag`; `familyId=20`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---


## Distort & transform

### 33/221 — Transform (`transform`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `scale` | Scale | Float | 0.0–4.0 | 1.0 |
| `rotation` | Rotation | Angle | -180°–180° | 0.0 |
| `offset_x` | Offset X | Float | -1.0–1.0 | 0.0 |
| `offset_y` | Offset Y | Float | -1.0–1.0 | 0.0 |

**Shader** — `effect_transform.frag`; `familyId=0`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 34/221 — Scale (`scale`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `value` | Scale | Float | 0.0–4.0 | 1.0 |

**Shader** — `effect_transform.frag`; `familyId=1`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 35/221 — Rotate (`rotate`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `angle` | Angle | Angle | -180°–180° | 0.0 |

**Shader** — `effect_transform.frag`; `familyId=2`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 36/221 — Flip (`flip`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `mode` | Mode | EnumIndex | 0=Horizontal; 1=Vertical; 2=Both | 0 |

**Shader** — `effect_transform.frag`; `familyId=3`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 37/221 — Flip Horizontal (`flip_horizontal`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `enabled` | Enabled | Bool | 0–1 | true |

**Shader** — `effect_transform.frag`; `familyId=4`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 38/221 — Flip Vertical (`flip_vertical`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `enabled` | Enabled | Bool | 0–1 | true |

**Shader** — `effect_transform.frag`; `familyId=5`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 39/221 — Mirror (`mirror`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `divisions` | Divisions | Float | 1.0–32.0 | 4.0 |
| `angle` | Angle | Angle | -180°–180° | 0.0 |
| `mix` | Mix | Percent | 0.0–1.0 | 1.0 |

**Shader** — `effect_kaleido.frag`; `familyId=0`; **Spezial** —  divisions/angle/mix (überschreibt generisch)

---

### 40/221 — Mirror Quad (`mirror_quad`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `divisions` | Divisions | Float | 1.0–32.0 | 4.0 |
| `angle` | Angle | Angle | -180°–180° | 0.0 |
| `mix` | Mix | Percent | 0.0–1.0 | 1.0 |

**Shader** — `effect_kaleido.frag`; `familyId=1`; **Spezial** —  divisions/angle/mix (überschreibt generisch)

---

### 41/221 — Mirror Stripes (`mirror_stripes`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `divisions` | Divisions | Float | 1.0–32.0 | 4.0 |
| `angle` | Angle | Angle | -180°–180° | 0.0 |
| `mix` | Mix | Percent | 0.0–1.0 | 1.0 |

**Shader** — `effect_kaleido.frag`; `familyId=2`; **Spezial** —  divisions/angle/mix (überschreibt generisch)

---

### 42/221 — Multi Mirror (`multi_mirror`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `divisions` | Divisions | Float | 1.0–32.0 | 4.0 |
| `angle` | Angle | Angle | -180°–180° | 0.0 |
| `mix` | Mix | Percent | 0.0–1.0 | 1.0 |

**Shader** — `effect_kaleido.frag`; `familyId=3`; **Spezial** —  divisions/angle/mix (überschreibt generisch)

---

### 43/221 — Tile (`tile`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `repeat_x` | Repeat X | Float | 1.0–32.0 | 2.0 |
| `repeat_y` | Repeat Y | Float | 1.0–32.0 | 2.0 |
| `mirror` | Mirror | Percent | 0.0–1.0 | 0.0 |

**Shader** — `effect_transform.frag`; `familyId=6`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 44/221 — Kaleido (`kaleido`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `divisions` | Divisions | Float | 1.0–32.0 | 4.0 |
| `angle` | Angle | Angle | -180°–180° | 0.0 |
| `mix` | Mix | Percent | 0.0–1.0 | 1.0 |

**Shader** — `effect_kaleido.frag`; `familyId=4`; **Spezial** —  divisions/angle/mix (überschreibt generisch)

---

### 45/221 — Kaleidoscope (`kaleidoscope`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `divisions` | Divisions | Float | 1.0–32.0 | 4.0 |
| `angle` | Angle | Angle | -180°–180° | 0.0 |
| `mix` | Mix | Percent | 0.0–1.0 | 1.0 |

**Shader** — `effect_kaleido.frag`; `familyId=5`; **Spezial** —  divisions/angle/mix (überschreibt generisch)

---

### 46/221 — Ripple (`ripple`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.5 |
| `frequency` | Frequency | Float | 0.0–32.0 | 4.0 |
| `speed` | Speed | Float | -10.0–10.0 | 0.0 |

**Shader** — `effect_distort.frag`; `familyId=0`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 47/221 — Wave (`wave`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.5 |
| `frequency` | Frequency | Float | 0.0–32.0 | 4.0 |
| `speed` | Speed | Float | -10.0–10.0 | 0.0 |

**Shader** — `effect_distort.frag`; `familyId=1`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 48/221 — Twirl (`twirl`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.5 |
| `frequency` | Frequency | Float | 0.0–32.0 | 4.0 |
| `speed` | Speed | Float | -10.0–10.0 | 0.0 |

**Shader** — `effect_distort.frag`; `familyId=2`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 49/221 — Bulge (`bulge`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.5 |
| `frequency` | Frequency | Float | 0.0–32.0 | 4.0 |
| `speed` | Speed | Float | -10.0–10.0 | 0.0 |

**Shader** — `effect_distort.frag`; `familyId=3`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 50/221 — Fisheye (`fisheye`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.5 |
| `frequency` | Frequency | Float | 0.0–32.0 | 4.0 |
| `speed` | Speed | Float | -10.0–10.0 | 0.0 |

**Shader** — `effect_distort.frag`; `familyId=4`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 51/221 — Distortion (`distortion`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `distort` | Distort | Percent | 0.0–1.0 | 0.5 |
| `radius` | Radius | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_distort.frag`; `familyId=5`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 52/221 — Bend (`bend`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.5 |
| `frequency` | Frequency | Float | 0.0–32.0 | 4.0 |
| `speed` | Speed | Float | -10.0–10.0 | 0.0 |

**Shader** — `effect_distort.frag`; `familyId=6`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 53/221 — Warp (`warp`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.5 |
| `frequency` | Frequency | Float | 0.0–32.0 | 4.0 |
| `speed` | Speed | Float | -10.0–10.0 | 0.0 |

**Shader** — `effect_distort.frag`; `familyId=7`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 54/221 — Zoom (`zoom`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Float | 0.0–4.0 | 1.0 |

**Shader** — `effect_transform.frag`; `familyId=9`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 55/221 — Polar (`polar`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 1.0 |

**Shader** — `effect_transform.frag`; `familyId=10`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 56/221 — Polarizer (`polarizer`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `angle` | Angle | Angle | -180°–180° | 0.0 |
| `amount` | Amount | Percent | 0.0–1.0 | 1.0 |

**Shader** — `effect_transform.frag`; `familyId=11`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 57/221 — Displacement (`displacement`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `horizontal` | Horizontal | Float | -1.0–1.0 | 0.0 |
| `vertical` | Vertical | Float | -1.0–1.0 | 0.0 |
| `amount` | Amount | Percent | 0.0–1.0 | 0.5 |

**Shader** — `effect_distort.frag`; `familyId=8`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 58/221 — Pixelate (`pixelate`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `size` | Size | Float | 1.0–256.0 | 8.0 |

**Shader** — `effect_transform.frag`; `familyId=12`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 59/221 — Smooth Transform (`smooth_transform`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `smoothness` | Smoothness | Percent | 0.0–1.0 | 0.5 |

**Shader** — `effect_transform.frag`; `familyId=8`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 60/221 — Screen Shake (`screen_shake`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.3 |
| `frequency` | Frequency | Float | 0.1–30.0 | 8.0 |

**Shader** — `effect_distort.frag`; `familyId=9`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 61/221 — Space Warper (`space_warper`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.6 |
| `frequency` | Frequency | Float | 0.0–16.0 | 2.0 |

**Shader** — `effect_distort.frag`; `familyId=10`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 62/221 — Shifty (`shifty`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.5 |
| `direction` | Direction | Angle | -180°–180° | 0.0 |

**Shader** — `effect_distort.frag`; `familyId=11`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---


## Generate & blend

### 63/221 — Glow (`glow`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.6 |
| `threshold` | Threshold | Float | 0.0–1.0 | 0.5 |
| `radius` | Radius | Float | 0.0–128.0 | 16.0 |

**Shader** — `effect_generate.frag`; `familyId=0`; **Spezial** —  familyId==1 (bloom) Pass1: params[0]*2

---

### 64/221 — Bloom (`bloom`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.6 |
| `threshold` | Threshold | Float | 0.0–1.0 | 0.5 |
| `radius` | Radius | Float | 0.0–128.0 | 16.0 |

**Shader** — `effect_generate.frag`; `familyId=1`; 2 interne Pässe; **Spezial** —  familyId==1 (bloom) Pass1: params[0]*2

---

### 65/221 — God Rays (`god_rays`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.6 |
| `threshold` | Threshold | Float | 0.0–1.0 | 0.5 |
| `radius` | Radius | Float | 0.0–128.0 | 16.0 |

**Shader** — `effect_generate.frag`; `familyId=2`; **Spezial** —  familyId==1 (bloom) Pass1: params[0]*2

---

### 66/221 — Strobe (`strobe`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `rate` | Rate | Float | 0.0–20.0 | 8.0 |
| `duty` | Duty | Percent | 0.0–1.0 | 0.5 |

**Shader** — `effect_generate.frag`; `familyId=3`; **Spezial** —  familyId==1 (bloom) Pass1: params[0]*2

---

### 67/221 — Trails (`trails`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `strength` | Strength | Percent | 0.0–1.0 | 0.75 |
| `decay` | Decay | Percent | 0.0–1.0 | 0.2 |

**Shader** — `effect_generate.frag`; `familyId=4`; **Spezial** —  familyId==1 (bloom) Pass1: params[0]*2

---

### 68/221 — Light Leak (`light_leak`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.6 |
| `angle` | Angle | Angle | -180°–180° | 0.0 |

**Shader** — `effect_generate.frag`; `familyId=5`; **Spezial** —  familyId==1 (bloom) Pass1: params[0]*2

---

### 69/221 — Noise (`noise`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.2 |
| `speed` | Speed | Float | 0.0–10.0 | 1.0 |

**Shader** — `effect_generate.frag`; `familyId=6`; **Spezial** —  familyId==1 (bloom) Pass1: params[0]*2

---

### 70/221 — RGB Noise (`rgb_noise`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.2 |
| `speed` | Speed | Float | 0.0–10.0 | 1.0 |

**Shader** — `effect_generate.frag`; `familyId=7`; **Spezial** —  familyId==1 (bloom) Pass1: params[0]*2

---

### 71/221 — Video Noise (`video_noise`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.2 |
| `speed` | Speed | Float | 0.0–10.0 | 1.0 |

**Shader** — `effect_generate.frag`; `familyId=8`; **Spezial** —  familyId==1 (bloom) Pass1: params[0]*2

---

### 72/221 — Halftone (`halftone`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `size` | Size | Float | 1.0–128.0 | 8.0 |
| `angle` | Angle | Angle | -180°–180° | 0.0 |

**Shader** — `effect_generate.frag`; `familyId=9`; **Spezial** —  familyId==1 (bloom) Pass1: params[0]*2

---

### 73/221 — Vignette (`vignette`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.5 |
| `softness` | Softness | Percent | 0.0–1.0 | 0.5 |

**Shader** — `effect_generate.frag`; `familyId=10`; **Spezial** —  familyId==1 (bloom) Pass1: params[0]*2

---

### 74/221 — Spotlight (`spotlight`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.6 |
| `size` | Size | Float | 0.0–2.0 | 0.5 |
| `falloff` | Falloff | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_generate.frag`; `familyId=11`; **Spezial** —  familyId==1 (bloom) Pass1: params[0]*2

---

### 75/221 — Drop Shadow (`drop_shadow`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `opacity` | Opacity | Percent | 0.0–1.0 | 0.5 |
| `distance` | Distance | Float | 0.0–128.0 | 8.0 |
| `angle` | Angle | Angle | -180°–180° | 45.0 |

**Shader** — `effect_generate.frag`; `familyId=12`; **Spezial** —  familyId==1 (bloom) Pass1: params[0]*2

---

### 76/221 — Rainbow (`rainbow`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 1.0 |
| `speed` | Speed | Float | -4.0–4.0 | 0.5 |

**Shader** — `effect_generate.frag`; `familyId=13`; **Spezial** —  familyId==1 (bloom) Pass1: params[0]*2

---

### 77/221 — Prismatic (`prismatic`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.5 |
| `samples` | Samples | Float | 1.0–16.0 | 4.0 |

**Shader** — `effect_generate.frag`; `familyId=14`; **Spezial** —  familyId==1 (bloom) Pass1: params[0]*2

---

### 78/221 — Replicate (`replicate`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `count` | Count | Float | 1.0–32.0 | 4.0 |
| `spread` | Spread | Percent | 0.0–1.0 | 0.25 |

**Shader** — `effect_generate.frag`; `familyId=15`; **Spezial** —  familyId==1 (bloom) Pass1: params[0]*2

---

### 79/221 — Echo (`echo`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.5 |
| `delay` | Delay | Float | 0.0–2.0 | 0.2 |

**Shader** — `effect_generate.frag`; `familyId=16`; **Spezial** —  familyId==1 (bloom) Pass1: params[0]*2

---

### 80/221 — Slit Scanner (`slit_scanner`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `speed` | Speed | Float | -4.0–4.0 | 0.5 |
| `axis` | Axis | EnumIndex | 0=Horizontal; 1=Vertical | 0 |

**Shader** — `effect_generate.frag`; `familyId=17`; **Spezial** —  familyId==1 (bloom) Pass1: params[0]*2

---


## Stylize & film

### 81/221 — Edges (`edges`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.6 |
| `detail` | Detail | Float | 0.0–4.0 | 1.0 |

**Shader** — `effect_stylize.frag`; `familyId=0`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 82/221 — Emboss (`emboss`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.6 |
| `detail` | Detail | Float | 0.0–4.0 | 1.0 |

**Shader** — `effect_stylize.frag`; `familyId=1`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 83/221 — Find Edges (`find_edges`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.6 |
| `detail` | Detail | Float | 0.0–4.0 | 1.0 |

**Shader** — `effect_stylize.frag`; `familyId=2`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 84/221 — Glow Edges (`glow_edges`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.6 |
| `detail` | Detail | Float | 0.0–4.0 | 1.0 |

**Shader** — `effect_stylize.frag`; `familyId=3`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 85/221 — CRT (`crt`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.7 |

**Shader** — `effect_stylize.frag`; `familyId=4`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 86/221 — VHS (`vhs`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.7 |

**Shader** — `effect_stylize.frag`; `familyId=5`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 87/221 — VHSifyer (`vhsifyer`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.7 |

**Shader** — `effect_stylize.frag`; `familyId=6`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 88/221 — Film Grain (`film_grain`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.2 |
| `speed` | Speed | Float | 0.0–10.0 | 1.0 |

**Shader** — `effect_stylize.frag`; `familyId=7`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 89/221 — Scanlines (`scanlines`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.7 |

**Shader** — `effect_stylize.frag`; `familyId=8`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 90/221 — Broadcast (`broadcast`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.7 |

**Shader** — `effect_stylize.frag`; `familyId=9`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 91/221 — Reducto (`reducto`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.7 |

**Shader** — `effect_stylize.frag`; `familyId=10`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 92/221 — Total Visual Annihilation (`total_visual_annihilation`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.7 |

**Shader** — `effect_stylize.frag`; `familyId=11`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---


## Key & mask

### 93/221 — Chroma Key (`chroma_key`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `mode` | Key mode | EnumIndex | 0=Chroma (hue); 1=Chroma (hue) inverted; 2=RGB distance; 3=RGB distance inverted; 4=RGB max delta; 5=RGB max delta inverted | 0 |
| `hue` | Key hue | Color |  | 0.33 |
| `threshold` | Threshold | Percent | 0.0–1.0 | 0.2 |
| `softness` | Softness | Percent | 0.0–1.0 | 0.2 |

**Shader** — `effect_key.frag`; `familyId=0`; **Spezial** —  keyChannelRgb + mode; softness min 0.04

---

### 94/221 — Luma Key (`luma_key`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `mode` | Key source | EnumIndex | 0=Weighted RGB (inspector); 1=Weighted RGB inverted; 2=Luma BT.709; 3=Luma BT.709 inverted; 4=Red channel; 5=Green channel; 6=Blue channel; 7=Max RGB; 8=Min RGB; 9=Max RGB inverted | 0 |
| `brightness` | Level / center | Percent | 0.0–1.0 | 0.5 |
| `threshold` | Tolerance | Percent | 0.0–1.0 | 0.25 |
| `softness` | Feather | Percent | 0.0–1.0 | 0.12 |

**Shader** — `effect_key.frag`; `familyId=1`; **Spezial** —  keyChannelRgb + mode; softness min 0.04

---

### 95/221 — Linear Mask (`linear_mask`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `angle` | Angle | Angle | -180°–180° | 0.0 |
| `softness` | Softness | Percent | 0.0–1.0 | 0.2 |

**Shader** — `effect_mask.frag`; `familyId=0`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 96/221 — Mask (`mask`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `mode` | Mask mode | EnumIndex | 0=None; 1=Rectangle; 2=Circle; 3=Soft edge; 4=Ellipse; 5=Custom | 0 |
| `sizeX` | Size X | Percent | 0.0–1.0 | 1.0 |
| `sizeY` | Size Y | Percent | 0.0–1.0 | 1.0 |
| `feather` | Feather | Percent | 0.0–1.0 | 0.1 |

**Shader** — `effect_mask.frag`; `familyId=1`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 97/221 — Crop (`crop`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `left` | Left | Percent | 0.0–1.0 | 0.0 |
| `top` | Top | Percent | 0.0–1.0 | 0.0 |
| `right` | Right | Percent | 0.0–1.0 | 1.0 |
| `bottom` | Bottom | Percent | 0.0–1.0 | 1.0 |

**Shader** — `effect_mask.frag`; `familyId=2`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 98/221 — Crop Rectangle (`crop_rectangle`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `x` | X | Percent | 0.0–1.0 | 0.0 |
| `y` | Y | Percent | 0.0–1.0 | 0.0 |
| `width` | Width | Percent | 0.0–1.0 | 1.0 |
| `height` | Height | Percent | 0.0–1.0 | 1.0 |

**Shader** — `effect_mask.frag`; `familyId=3`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---


## Mix & utility

### 99/221 — Add Subtract (`add_subtract`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `mix` | Mix | Percent | 0.0–1.0 | 0.5 |
| `mode` | Mode | EnumIndex | 0=Add; 1=Subtract | 0 |

**Shader** — `effect_utility.frag`; `familyId=0`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 100/221 — Mix (`mix`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 1.0 |

**Shader** — `effect_utility.frag`; `familyId=1`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 101/221 — Fade (`fade`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 1.0 |

**Shader** — `effect_utility.frag`; `familyId=2`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 102/221 — RGB Shift (`rgb_shift`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `distance` | Distance | Float | 0.0–100.0 | 2.0 |
| `angle` | Angle | Angle | -180°–180° | 0.0 |

**Shader** — `effect_utility.frag`; `familyId=3`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 103/221 — Shift (`shift`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `x` | X | Float | -1.0–1.0 | 0.0 |
| `y` | Y | Float | -1.0–1.0 | 0.0 |

**Shader** — `effect_utility.frag`; `familyId=4`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 104/221 — Tilt Shift (`tilt_shift`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.5 |
| `angle` | Angle | Angle | -180°–180° | 0.0 |

**Shader** — `effect_utility.frag`; `familyId=5`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---


## Patterns

### 105/221 — Radar (`radar`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 1.0 |
| `scale` | Scale | Float | 0.01–10.0 | 1.0 |
| `speed` | Speed | Float | -10.0–10.0 | 0.0 |

**Shader** — `effect_pattern.frag`; `familyId=0`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 106/221 — PolkaDot (`polka_dot`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 1.0 |
| `scale` | Scale | Float | 0.01–10.0 | 1.0 |
| `speed` | Speed | Float | -10.0–10.0 | 0.0 |

**Shader** — `effect_pattern.frag`; `familyId=1`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 107/221 — Stripes (`stripes`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 1.0 |
| `scale` | Scale | Float | 0.01–10.0 | 1.0 |
| `speed` | Speed | Float | -10.0–10.0 | 0.0 |

**Shader** — `effect_pattern.frag`; `familyId=2`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 108/221 — Checkerboard (`checkerboard`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 1.0 |
| `scale` | Scale | Float | 0.01–10.0 | 1.0 |
| `speed` | Speed | Float | -10.0–10.0 | 0.0 |

**Shader** — `effect_pattern.frag`; `familyId=3`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 109/221 — Dots (`dots`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 1.0 |
| `scale` | Scale | Float | 0.01–10.0 | 1.0 |
| `speed` | Speed | Float | -10.0–10.0 | 0.0 |

**Shader** — `effect_pattern.frag`; `familyId=4`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---


## Blend modes

### 110/221 — Normal (`blend_normal`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `mode` | Blend Mode | EnumIndex | 0=Normal; 1=Add; 2=Subtract; 3=Multiply; 4=Screen; 5=Overlay; 6=Soft Light; 7=Hard Light; 8=Difference; 9=Exclusion | 0 |
| `mix` | Opacity | Percent | 0.0–1.0 | 1.0 |

**Shader** — `effect_blend.frag`; `familyId=0`; Blend-Override=0; **Spezial** —  blendModeOverride → params[0] wenn gesetzt

---

### 111/221 — Add (`blend_add`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `mode` | Blend Mode | EnumIndex | 0=Normal; 1=Add; 2=Subtract; 3=Multiply; 4=Screen; 5=Overlay; 6=Soft Light; 7=Hard Light; 8=Difference; 9=Exclusion | 0 |
| `mix` | Opacity | Percent | 0.0–1.0 | 1.0 |

**Shader** — `effect_blend.frag`; `familyId=1`; Blend-Override=1; **Spezial** —  blendModeOverride → params[0] wenn gesetzt

---

### 112/221 — Subtract (`blend_subtract`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `mode` | Blend Mode | EnumIndex | 0=Normal; 1=Add; 2=Subtract; 3=Multiply; 4=Screen; 5=Overlay; 6=Soft Light; 7=Hard Light; 8=Difference; 9=Exclusion | 0 |
| `mix` | Opacity | Percent | 0.0–1.0 | 1.0 |

**Shader** — `effect_blend.frag`; `familyId=2`; Blend-Override=2; **Spezial** —  blendModeOverride → params[0] wenn gesetzt

---

### 113/221 — Multiply Blend (`blend_multiply`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `mode` | Blend Mode | EnumIndex | 0=Normal; 1=Add; 2=Subtract; 3=Multiply; 4=Screen; 5=Overlay; 6=Soft Light; 7=Hard Light; 8=Difference; 9=Exclusion | 0 |
| `mix` | Opacity | Percent | 0.0–1.0 | 1.0 |

**Shader** — `effect_blend.frag`; `familyId=3`; Blend-Override=3; **Spezial** —  blendModeOverride → params[0] wenn gesetzt

---

### 114/221 — Screen (`blend_screen`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `mode` | Blend Mode | EnumIndex | 0=Normal; 1=Add; 2=Subtract; 3=Multiply; 4=Screen; 5=Overlay; 6=Soft Light; 7=Hard Light; 8=Difference; 9=Exclusion | 0 |
| `mix` | Opacity | Percent | 0.0–1.0 | 1.0 |

**Shader** — `effect_blend.frag`; `familyId=4`; Blend-Override=4; **Spezial** —  blendModeOverride → params[0] wenn gesetzt

---

### 115/221 — Overlay (`blend_overlay`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `mode` | Blend Mode | EnumIndex | 0=Normal; 1=Add; 2=Subtract; 3=Multiply; 4=Screen; 5=Overlay; 6=Soft Light; 7=Hard Light; 8=Difference; 9=Exclusion | 0 |
| `mix` | Opacity | Percent | 0.0–1.0 | 1.0 |

**Shader** — `effect_blend.frag`; `familyId=5`; Blend-Override=5; **Spezial** —  blendModeOverride → params[0] wenn gesetzt

---

### 116/221 — Soft Light (`blend_soft_light`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `mode` | Blend Mode | EnumIndex | 0=Normal; 1=Add; 2=Subtract; 3=Multiply; 4=Screen; 5=Overlay; 6=Soft Light; 7=Hard Light; 8=Difference; 9=Exclusion | 0 |
| `mix` | Opacity | Percent | 0.0–1.0 | 1.0 |

**Shader** — `effect_blend.frag`; `familyId=6`; Blend-Override=6; **Spezial** —  blendModeOverride → params[0] wenn gesetzt

---

### 117/221 — Hard Light (`blend_hard_light`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `mode` | Blend Mode | EnumIndex | 0=Normal; 1=Add; 2=Subtract; 3=Multiply; 4=Screen; 5=Overlay; 6=Soft Light; 7=Hard Light; 8=Difference; 9=Exclusion | 0 |
| `mix` | Opacity | Percent | 0.0–1.0 | 1.0 |

**Shader** — `effect_blend.frag`; `familyId=7`; Blend-Override=7; **Spezial** —  blendModeOverride → params[0] wenn gesetzt

---

### 118/221 — Color Dodge (`blend_color_dodge`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `mode` | Blend Mode | EnumIndex | 0=Normal; 1=Add; 2=Subtract; 3=Multiply; 4=Screen; 5=Overlay; 6=Soft Light; 7=Hard Light; 8=Difference; 9=Exclusion | 0 |
| `mix` | Opacity | Percent | 0.0–1.0 | 1.0 |

**Shader** — `effect_blend.frag`; `familyId=8`; Blend-Override=8; **Spezial** —  blendModeOverride → params[0] wenn gesetzt

---

### 119/221 — Color Burn (`blend_color_burn`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `mode` | Blend Mode | EnumIndex | 0=Normal; 1=Add; 2=Subtract; 3=Multiply; 4=Screen; 5=Overlay; 6=Soft Light; 7=Hard Light; 8=Difference; 9=Exclusion | 0 |
| `mix` | Opacity | Percent | 0.0–1.0 | 1.0 |

**Shader** — `effect_blend.frag`; `familyId=9`; Blend-Override=9; **Spezial** —  blendModeOverride → params[0] wenn gesetzt

---

### 120/221 — Darken (`blend_darken`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `mode` | Blend Mode | EnumIndex | 0=Normal; 1=Add; 2=Subtract; 3=Multiply; 4=Screen; 5=Overlay; 6=Soft Light; 7=Hard Light; 8=Difference; 9=Exclusion | 0 |
| `mix` | Opacity | Percent | 0.0–1.0 | 1.0 |

**Shader** — `effect_blend.frag`; `familyId=10`; Blend-Override=10; **Spezial** —  blendModeOverride → params[0] wenn gesetzt

---

### 121/221 — Lighten (`blend_lighten`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `mode` | Blend Mode | EnumIndex | 0=Normal; 1=Add; 2=Subtract; 3=Multiply; 4=Screen; 5=Overlay; 6=Soft Light; 7=Hard Light; 8=Difference; 9=Exclusion | 0 |
| `mix` | Opacity | Percent | 0.0–1.0 | 1.0 |

**Shader** — `effect_blend.frag`; `familyId=11`; Blend-Override=11; **Spezial** —  blendModeOverride → params[0] wenn gesetzt

---

### 122/221 — Difference (`blend_difference`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `mode` | Blend Mode | EnumIndex | 0=Normal; 1=Add; 2=Subtract; 3=Multiply; 4=Screen; 5=Overlay; 6=Soft Light; 7=Hard Light; 8=Difference; 9=Exclusion | 0 |
| `mix` | Opacity | Percent | 0.0–1.0 | 1.0 |

**Shader** — `effect_blend.frag`; `familyId=12`; Blend-Override=12; **Spezial** —  blendModeOverride → params[0] wenn gesetzt

---

### 123/221 — Exclusion (`blend_exclusion`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `mode` | Blend Mode | EnumIndex | 0=Normal; 1=Add; 2=Subtract; 3=Multiply; 4=Screen; 5=Overlay; 6=Soft Light; 7=Hard Light; 8=Difference; 9=Exclusion | 0 |
| `mix` | Opacity | Percent | 0.0–1.0 | 1.0 |

**Shader** — `effect_blend.frag`; `familyId=13`; Blend-Override=13; **Spezial** —  blendModeOverride → params[0] wenn gesetzt

---


## Stylize & film

### 124/221 — Cartoon (`cartoon`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.6 |
| `detail` | Detail | Float | 0.0–4.0 | 1.0 |

**Shader** — `effect_stylize.frag`; `familyId=12`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 125/221 — Watercolor (`watercolor`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.6 |
| `detail` | Detail | Float | 0.0–4.0 | 1.0 |

**Shader** — `effect_stylize.frag`; `familyId=13`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 126/221 — Oil Paint (`oil_paint`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.6 |
| `detail` | Detail | Float | 0.0–4.0 | 1.0 |

**Shader** — `effect_stylize.frag`; `familyId=14`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 127/221 — Night Vision (`night_vision`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.7 |

**Shader** — `effect_stylize.frag`; `familyId=15`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 128/221 — Thermal (`thermal`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.7 |

**Shader** — `effect_stylize.frag`; `familyId=16`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 129/221 — X-Ray (`x_ray`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.7 |

**Shader** — `effect_stylize.frag`; `familyId=17`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 130/221 — Duotone (`duotone`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.7 |

**Shader** — `effect_stylize.frag`; `familyId=18`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 131/221 — Tritone (`tritone`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.7 |

**Shader** — `effect_stylize.frag`; `familyId=19`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 132/221 — Gradient Map (`gradient_map`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.7 |

**Shader** — `effect_stylize.frag`; `familyId=20`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 133/221 — Stroke (`stroke`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.7 |

**Shader** — `effect_stylize.frag`; `familyId=21`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 134/221 — Erode (`erode`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.7 |

**Shader** — `effect_stylize.frag`; `familyId=22`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 135/221 — Dilate (`dilate`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.7 |

**Shader** — `effect_stylize.frag`; `familyId=23`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---


## Color & levels

### 136/221 — Hue Rotate (`hue_rotate`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `angle` | Hue Rotate | Angle | -180°–180° | 0.0 |

**Shader** — `effect_color.frag`; `familyId=21`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 137/221 — Selective Color (`selective_color`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 1.0 |
| `brightness` | Brightness | Float | -1.0–1.0 | 0.0 |
| `contrast` | Contrast | Float | 0.0–2.0 | 1.0 |
| `saturation` | Saturation | Float | 0.0–2.0 | 1.0 |

**Shader** — `effect_color.frag`; `familyId=22`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 138/221 — Channel Mixer (`channel_mixer`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 1.0 |
| `brightness` | Brightness | Float | -1.0–1.0 | 0.0 |
| `contrast` | Contrast | Float | 0.0–2.0 | 1.0 |
| `saturation` | Saturation | Float | 0.0–2.0 | 1.0 |

**Shader** — `effect_color.frag`; `familyId=23`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 139/221 — Color Lookup (`color_lookup`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 1.0 |
| `brightness` | Brightness | Float | -1.0–1.0 | 0.0 |
| `contrast` | Contrast | Float | 0.0–2.0 | 1.0 |
| `saturation` | Saturation | Float | 0.0–2.0 | 1.0 |

**Shader** — `effect_color.frag`; `familyId=24`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---


## Distort & transform

### 140/221 — Mesh Warp (`mesh_warp`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.5 |
| `frequency` | Frequency | Float | 0.0–32.0 | 4.0 |
| `speed` | Speed | Float | -10.0–10.0 | 0.0 |

**Shader** — `effect_distort.frag`; `familyId=12`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 141/221 — Liquify (`liquify`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.5 |
| `frequency` | Frequency | Float | 0.0–32.0 | 4.0 |
| `speed` | Speed | Float | -10.0–10.0 | 0.0 |

**Shader** — `effect_distort.frag`; `familyId=13`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 142/221 — Motion Tile (`motion_tile`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `repeat_x` | Repeat X | Float | 1.0–32.0 | 2.0 |
| `repeat_y` | Repeat Y | Float | 1.0–32.0 | 2.0 |
| `mirror` | Mirror | Percent | 0.0–1.0 | 0.0 |

**Shader** — `effect_transform.frag`; `familyId=7`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 143/221 — Spherize (`spherize`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.5 |
| `frequency` | Frequency | Float | 0.0–32.0 | 4.0 |
| `speed` | Speed | Float | -10.0–10.0 | 0.0 |

**Shader** — `effect_distort.frag`; `familyId=14`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 144/221 — Cylinder (`cylinder`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.5 |
| `frequency` | Frequency | Float | 0.0–32.0 | 4.0 |
| `speed` | Speed | Float | -10.0–10.0 | 0.0 |

**Shader** — `effect_distort.frag`; `familyId=15`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 145/221 — Cube (`cube`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 0.5 |
| `frequency` | Frequency | Float | 0.0–32.0 | 4.0 |
| `speed` | Speed | Float | -10.0–10.0 | 0.0 |

**Shader** — `effect_distort.frag`; `familyId=16`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---


## NVIDIA

### 146/221 — Encoder Artifact Reduction (`nvidia_artifact_reduction`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `strength` | Strength | EnumIndex | 0=Weak; 1=Strong | 0 |
| `amount` | Opacity | Percent | 0.0–1.0 | 1.0 |

**Maxine SDK** — kein GLSL-Family-Shader; Output-Tab erlaubt

---

### 147/221 — Super Resolution (`nvidia_super_resolution`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `strength` | Mode | EnumIndex | 0=Lossy; 1=Lossless | 0 |
| `scale` | Scale | Float | 1.0–2.0 | 1.5 |

**Maxine SDK** — kein GLSL-Family-Shader; Output-Tab erlaubt

---

### 148/221 — Upscale (`nvidia_upscale`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `scale` | Scale | Float | 1.0–4.0 | 2.0 |
| `amount` | Strength | Percent | 0.0–1.0 | 0.5 |

**Maxine SDK** — kein GLSL-Family-Shader; Output-Tab erlaubt

---

### 149/221 — Video Denoise (`noise_reduction`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | EnumIndex | 0=Weak; 1=Strong | 1 |
| `amount` | Amount | Percent | 0.0–1.0 | 1.0 |

**Maxine SDK** — kein GLSL-Family-Shader; Output-Tab erlaubt

---


## Resolve FX Blur

### 150/221 — Mosaic Blur (`mosaic_blur`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_blur.frag`; `familyId=11`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 151/221 — Lens Blur (`lens_blur`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_blur.frag`; `familyId=12`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---


## Resolve FX Sharpen

### 152/221 — Sharpen Edges (`sharpen_edges`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `sharpen_amount` | Sharpen Amount | Float | 0.0–2.0 | 0.5 |
| `sharpen_radius` | Sharpen Radius | Percent | 0.0–1.0 | 0.5 |
| `display_edges` | Display Edges | Bool | 0/1 | 0 |
| `pre_denoise` | Pre Denoise | Percent | 0.0–1.0 | 0.0 |
| `edge_detect_threshold` | Edge Detect Threshold | Percent | 0.0–1.0 | 0.2 |
| `edge_mask_strength` | Edge Mask Strength | Percent | 0.0–1.0 | 0.5 |
| `edge_blur` | Edge Blur | Percent | 0.0–1.0 | 0.5 |
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |

**Shader** — `effect_blur.frag`; `familyId=8`; **Spezial** — edge-keyed unsharp mask, `packBlurUniforms` (params3 for pre_denoise/display_edges)

---

### 153/221 — Soften & Sharpen (`soften_sharpen`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `small_texture` | Small Texture | Float | -1.0–1.0 | 0.0 |
| `medium_texture` | Medium Texture | Float | -1.0–1.0 | -0.8 |
| `large_texture` | Large Texture | Float | -1.0–1.0 | -0.3 |
| `small_texture_size` | Small Texture Size | Float | 0.0–1.0 | 0.5 |
| `coring_softness` | Coring Softness | Percent | 0.0–1.0 | 0.0 |
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |

**Shader** — `effect_blur.frag`; `familyId=9`; **Spezial** — per-band soften/sharpen (negative/positive texture), `packBlurUniforms`

---


## Resolve FX Color

### 154/221 — ACES Transform (`aces_transform`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_color.frag`; `familyId=25`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 155/221 — Chromatic Adaptation (`chromatic_adaptation`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_color.frag`; `familyId=26`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 156/221 — Color Compressor (`color_compressor`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_color.frag`; `familyId=27`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 157/221 — Color Space Transform (`color_space_transform`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_color.frag`; `familyId=28`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 158/221 — Color Stabilizer (`color_stabilizer`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_color.frag`; `familyId=29`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 159/221 — Contrast Pop (`contrast_pop`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_color.frag`; `familyId=30`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 160/221 — Dehaze (`dehaze`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_color.frag`; `familyId=31`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 161/221 — False Color (`false_color`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_color.frag`; `familyId=32`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 162/221 — Flicker Addition (`flicker_addition`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_color.frag`; `familyId=33`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 163/221 — Gamut Limiter (`gamut_limiter`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_color.frag`; `familyId=34`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 164/221 — Gamut Mapping (`gamut_mapping`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_color.frag`; `familyId=35`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 165/221 — DCTL (`dctl`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_utility.frag`; `familyId=8`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---


## Resolve FX Generate

### 166/221 — Color Generator (`color_generator`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_generate.frag`; `familyId=18`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 167/221 — Color Palette (`color_palette`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_generate.frag`; `familyId=19`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 168/221 — Grid (`grid`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_pattern.frag`; `familyId=5`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---


## Resolve FX Key

### 169/221 — 3D Keyer (`key_3d`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_key.frag`; `familyId=2`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 170/221 — HSL Keyer (`hsl_keyer`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_key.frag`; `familyId=3`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 171/221 — Alpha Matte Shrink and Glow (`alpha_matte_shrink_glow`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_key.frag`; `familyId=4`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---


## Resolve FX Light

### 172/221 — Aperture Diffraction (`aperture_diffraction`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_light.frag`; `familyId=0`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 173/221 — Halation (`halation`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_light.frag`; `familyId=1`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 174/221 — Lens Flare (`lens_flare`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_light.frag`; `familyId=2`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 175/221 — Lens Reflections (`lens_reflections`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_light.frag`; `familyId=3`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 176/221 — Light Rays (`light_rays`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_light.frag`; `familyId=4`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---


## Resolve FX Refine

### 177/221 — Beauty (`beauty`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_revival.frag`; `familyId=6`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---


## Resolve FX Revival

### 178/221 — Automatic Dirt Removal (`automatic_dirt_removal`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_revival.frag`; `familyId=0`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 179/221 — Chromatic Aberration Removal (`chromatic_aberration_removal`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_revival.frag`; `familyId=1`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 180/221 — Dead Pixel Fixer (`dead_pixel_fixer`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_revival.frag`; `familyId=2`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 181/221 — Deband (`deband`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_revival.frag`; `familyId=3`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 182/221 — Deflicker (`deflicker`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_revival.frag`; `familyId=4`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 183/221 — Frame Replacer (`frame_replacer`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_revival.frag`; `familyId=5`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 184/221 — Patch Replacer (`patch_replacer`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_revival.frag`; `familyId=7`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---


## Resolve FX Stylize

### 185/221 — Abstraction (`abstraction`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_stylize.frag`; `familyId=24`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 186/221 — Blanking Fill (`blanking_fill`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_stylize.frag`; `familyId=25`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 187/221 — Pencil Sketch (`pencil_sketch`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_stylize.frag`; `familyId=26`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 188/221 — Prism Blur (`prism_blur`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_stylize.frag`; `familyId=27`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 189/221 — Stylize (`stylize`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_stylize.frag`; `familyId=28`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---


## Resolve FX Temporal

### 190/221 — Motion Trails (`motion_trails`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_temporal.frag`; `familyId=0`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 191/221 — Smear (`smear`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_temporal.frag`; `familyId=1`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 192/221 — Stop Motion (`stop_motion`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_temporal.frag`; `familyId=2`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---


## Resolve FX Texture

### 193/221 — Analog Damage (`analog_damage`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_stylize.frag`; `familyId=29`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 194/221 — Film Damage (`film_damage`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_stylize.frag`; `familyId=30`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 195/221 — JPEG Damage (`jpeg_damage`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_stylize.frag`; `familyId=31`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 196/221 — Texture Pop (`texture_pop`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_stylize.frag`; `familyId=32`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---


## Resolve FX Transform

### 197/221 — Camera Shake (`camera_shake`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_transform.frag`; `familyId=13`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 198/221 — Video Collage (`video_collage`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_transform.frag`; `familyId=14`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---


## Resolve FX Warp

### 199/221 — Dent (`dent`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_distort.frag`; `familyId=17`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 200/221 — Lens Distortion (`lens_distortion`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_distort.frag`; `familyId=18`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 201/221 — Ripples (`ripples`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_distort.frag`; `familyId=19`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 202/221 — Vortex (`vortex`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_distort.frag`; `familyId=20`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 203/221 — Warper (`warper`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_distort.frag`; `familyId=21`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 204/221 — Waviness (`waviness`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_distort.frag`; `familyId=22`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---


## Fusion Effects

### 205/221 — Binoculars (`binoculars`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_stylize.frag`; `familyId=33`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 206/221 — CCTV (`cctv`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_stylize.frag`; `familyId=34`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 207/221 — Colored Border (`colored_border`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_stylize.frag`; `familyId=35`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 208/221 — Digital Glitch (`digital_glitch`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_stylize.frag`; `familyId=36`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 209/221 — Drone Overlay (`drone_overlay`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_stylize.frag`; `familyId=37`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 210/221 — DSLR (`dslr`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_stylize.frag`; `familyId=38`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 211/221 — DVE (`dve`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_stylize.frag`; `familyId=39`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 212/221 — Video Call (`video_call`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_stylize.frag`; `familyId=40`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 213/221 — Video Camera (`video_camera`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_stylize.frag`; `familyId=41`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---


## Fusion Generators

### 214/221 — Background (`background`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_generate.frag`; `familyId=20`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 215/221 — Fast Noise (`fast_noise`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_generate.frag`; `familyId=21`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 216/221 — Plasma (`plasma`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_generate.frag`; `familyId=22`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 217/221 — Mandelbrot (`mandelbrot`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_generate.frag`; `familyId=23`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---

### 218/221 — Day Sky (`day_sky`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `blend` | Blend | Percent | 0.0–1.0 | 1.0 |
| `strength` | Strength | Percent | 0.0–1.0 | 0.5 |
| `detail` | Detail | Float | 0.0–1.0 | 0.5 |
| `size` | Size | Float | 0.0–1.0 | 0.5 |

**Shader** — `effect_generate.frag`; `familyId=24`; **Resolve-Stub** — `packGenericParams` (blend/strength/detail/size), früher Return

---


## Utility

### 219/221 — Opacity (`opacity`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `amount` | Amount | Percent | 0.0–1.0 | 1.0 |

**Shader** — `effect_utility.frag`; `familyId=6`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---

### 220/221 — Blend Mode (`blend_mode`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `mode` | Blend Mode | EnumIndex | 0=Normal; 1=Add; 2=Subtract; 3=Multiply; 4=Screen; 5=Overlay; 6=Soft Light; 7=Hard Light; 8=Difference; 9=Exclusion | 0 |
| `mix` | Opacity | Percent | 0.0–1.0 | 1.0 |

**Shader** — `effect_blend.frag`; `familyId=14`; **Spezial** —  blendModeOverride → params[0] wenn gesetzt

---

### 221/221 — 3D Transform (`transform_3d`)

| Key | Label | Typ | Bereich | Default |
|-----|-------|-----|---------|---------|
| `rotate_x` | Rotate X | Angle | -180°–180° | 0.0 |
| `rotate_y` | Rotate Y | Angle | -180°–180° | 0.0 |
| `rotate_z` | Rotate Z | Angle | -180°–180° | 0.0 |
| `depth` | Depth | Float | 0.0–2.0 | 0.5 |

**Shader** — `effect_utility.frag`; `familyId=7`; **Generisch** — `packGenericParams` → `ubo.params[0..3]` per `familyId` im Shader

---
