# Solar Gradient Analysis (Math, Resolution Impact, and Visual Outcomes)

Date: 2026-04-12

## 1) Scope

This document analyzes how the watchface gradient is computed, how well it maps to real solar conditions, and how it presents across active color Pebble targets.

Active deployment scope (2026-04-12): basalt, chalk, emery, gabbro.
Legacy BW notes in this document are retained for historical context only.

Primary code paths reviewed:
- src/c/Sky.c:26-28 (gradient constants)
- src/c/Sky.c:81-88 (atmosphere anchor bands)
- src/c/Sky.c:202-236 (altitude-to-palette interpolation)
- src/c/Sky.c:238-241 (dither pattern)
- src/c/Sky.c:243-252 (color vs BW conversion)
- src/c/Sky.c:407-446 (gradient rasterization)
- src/pkjs/index.js:309-326 (SunCalc payload generation)

Supporting settings and messaging paths reviewed:
- src/pkjs/config.js (Clay options)
- src/pkjs/index.js:200-211 (normalized style settings sent to watch)
- src/c/Sky.c:588-636 (inbox handling on watch)

## 2) Current Gradient Pipeline

1. JS computes sun position using SunCalc for current time and location.
2. JS sends:
   - altitude (deg x100)
   - azimuth (deg x100)
   - gradient angle (deg x100)
3. Watch maps altitude to a two-color palette by interpolating between atmosphere anchor bands.
4. Watch draws a directional linear gradient by projecting each pixel block against a unit direction vector.
5. Watch applies tiny spatial dither to reduce visible banding.
6. Legacy note: on BW platforms, each RGB sample was thresholded to white/black only.

## 2.2) New Clay + Weather Layer (2026-04-12 Implementation)

The watchface now has a first-pass personalization and weather data layer:

1. Clay now exposes per-device time size controls for basalt/chalk/emery/gabbro.
2. Clay now exposes independent footer visibility toggles for city and altitude.
3. Clay now exposes weather controls (enable, Fahrenheit/Celsius, detail level).
4. pkjs now fetches Open-Meteo current conditions (temperature, weather code, cloud cover, precipitation, wind).
5. Weather is sent as a follow-up message and does not block core solar payload delivery.
6. Weather requests are cached/throttled and slowed relative to solar refresh cadence.

Implementation notes:
- Solar refresh cadence remains 10 minutes.
- Weather refresh cadence is currently throttled to 45 minutes unless startup/watch-request/settings-updated forces refresh.
- Battery save mode currently suppresses auto-refresh weather pulls while preserving solar updates.

## 2.1) Current Acceptance Focus

The current beauty pass must satisfy the strict high-daylight condition:

1. Altitude 40.0-55.0 deg on round displays (chalk/gabbro) must show at least two visibly distinct sky regions behind the time text.
2. Routine auto-refresh must not switch to full loading card.
3. Text readability must remain immediate without heavy fallback effects.
4. No regressions at twilight (-2 to +6 deg) and deep-night (< -18 deg) checkpoints.

## 3) Core Math and Behavior

### 3.1 Solar angle generation

From src/pkjs/index.js:309-326:
- azimuthDeg = normalized solar azimuth in compass degrees
- gradientAngleDeg = (azimuthDeg + 180) mod 360
- altitudeDeg = solar altitude in degrees

This means the gradient direction is always opposite the computed azimuth vector.

### 3.2 Altitude-to-palette interpolation

From src/c/Sky.c:81-88 and 202-236:
- Uses 6 anchor bands at altitudes: -18, -6, -2, 2, 10, 20 degrees.
- Between anchors: channel-wise linear interpolation.
- Above 20 degrees: additional contrast widening up to 45% over the base pair.

### 3.3 Gradient rasterization

From src/c/Sky.c:407-446:
- Block size defaults to 2x2 pixels.
- If area > 45,000 pixels: block size increases to 4x4.
- Factor for each block:
  factor = clamp(projected_position * scale + 0.5 + dither, 0, 1)
- dither amplitude is fixed at 0.03.

### 3.4 BW conversion limitation

From src/c/Sky.c:243-252:
- BW devices do not preserve grayscale.
- RGB is converted to luminance and thresholded at 127:
  - luminance > 127 => white
  - else black

Result: the entire gradient can collapse to all-black or all-white if both endpoints lie on same side of threshold.

## 4) Altitude-to-Color Chart (Current Math)

Generated directly from current interpolation/widening logic.

| Alt (deg) | Top RGB | Bottom RGB | Luma Top | Luma Bottom | BW Endpoint Mapping |
|---|---|---|---:|---:|---|
| -18 | #08081a | #28366e | 10 | 56 | Black / Black |
| -12 | #11102e | #31306d | 20 | 55 | Black / Black |
| -6 | #1a1742 | #3a296c | 29 | 54 | Black / Black |
| -4 | #3d275c | #7f3877 | 52 | 84 | Black / Black |
| -2 | #603776 | #c44682 | 74 | 115 | Black / Black |
| 0 | #a75659 | #e25c7a | 111 | 135 | Black / White |
| 2 | #ee743c | #ff7172 | 146 | 156 | White / White |
| 6 | #f7994f | #ff8461 | 173 | 165 | White / White |
| 10 | #ffbd62 | #ff9750 | 198 | 174 | White / White |
| 15 | #bbbeb1 | #ddbda8 | 188 | 196 | White / White |
| 20 | #76beff | #bbe2ff | 176 | 218 | White / White |
| 25 | #74bdff | #bde3ff | 175 | 219 | White / White |
| 30 | #72bcff | #bfe4ff | 174 | 220 | White / White |
| 35 | #6fbbff | #c2e5ff | 172 | 221 | White / White |
| 45 | #6bb8ff | #c6e8ff | 169 | 224 | White / White |
| 55 | #66b6ff | #cbeaff | 166 | 227 | White / White |

Interpretation:
- Color devices can still show hue shifts when luma separation is small.
- BW devices will lose almost all gradient detail in many states because endpoint pairs are often both black or both white after thresholding.

## 5) Device Resolution and Step-Size Impact

Note: only color targets are currently in active support and acceptance gates.

Verified from build/c4che/*_cache.py (line 21 for each platform):
- aplite: BW, 144x168
- basalt: color, 144x168
- chalk: color, 180x180 round
- diorite: BW, 144x168
- emery: color, 200x228
- flint: BW, 144x168
- gabbro: color, 260x260 round

Current draw block sizing from src/c/Sky.c:411-414:
- area <= 45,000: step 2
- area > 45,000: step 4

| Platform | Resolution | Color Class | Gradient Step | Approx Fill Blocks/Frame |
|---|---:|---|---:|---:|
| aplite | 144x168 | BW | 2 | 6,048 |
| basalt | 144x168 | color | 2 | 6,048 |
| diorite | 144x168 | BW | 2 | 6,048 |
| flint | 144x168 | BW | 2 | 6,048 |
| chalk | 180x180 | color | 2 | 8,100 |
| emery | 200x228 | color | 4 | 2,850 |
| gabbro | 260x260 | color | 4 | 4,225 |

Implications:
- Large color targets (emery/gabbro) trade smoothness for speed due to 4x4 blocks; visible chunking can reduce dramatic feel.
- BW targets are limited mostly by threshold collapse, not by step size.

## 6) Real Capture Evidence

Captured files:
- analysis/screens/emery_ready.png
- analysis/screens/flint_ready.png

Observed at capture time (altitude approx -27.1 deg from watch text):
- emery: clear diagonal split, strong color separation.
- flint: mostly black with little/no perceivable gradient.

This aligns with the math table above for negative altitudes where both endpoints map below BW threshold.

## 7) Why Some Displays Look Barely Present

1. BW threshold collapse (major)
- The BW path reduces full RGB to binary black/white, often eliminating intra-gradient contrast entirely.

2. Palette luma pinch at specific solar phases (moderate)
- Around low positive altitudes (for example 2-6 deg, and also around 15 deg), endpoint luma delta is low, making gradients feel flat even on color displays.

3. Coarse step on large displays (moderate)
- 4x4 block rendering on emery/gabbro can look less rich than expected.

4. Direction-versus-sun aesthetic (minor but notable)
- Gradient direction is explicitly opposite solar azimuth, which can be visually striking but not always physically intuitive.

## 8) Reality Fidelity Assessment

What is physically grounded:
- Solar azimuth and altitude are real-time and location-based (SunCalc).

What is stylized:
- Sky color model is hand-authored via six anchor bands.
- No cloud, turbidity, humidity, aerosol, or twilight scattering model.
- No per-device perceptual compensation.

Conclusion:
- The direction and timing of color transitions are reality-informed.
- The exact sky chroma/contrast is intentionally artistic and currently under-optimized for BW and some color phases.

## 9) Testing Gap and Dev Mode Recommendation

Current Clay/testing mismatch:
- JS sends normalized settings including CustomLocationEnabled, CustomLatitudeE6, CustomLongitudeE6, DebugBenchmark (src/pkjs/index.js:200-211).
- Watch inbox currently reads TextOverrideMode but does not read custom-location/debug tuples directly (src/c/Sky.c:588-636).

Recommendation for a dedicated test mode (phase 2, after approval):

1. Add "Solar Dev Mode" section in Clay:
- Toggle: enable dev solar override
- Inputs: test latitude, longitude
- Inputs: test date/time (or hour slider)
- Toggle: force BW simulation on color watches (optional)

2. Compute simulated payload in JS:
- If dev mode on, compute solar for chosen datetime/location and send to watch immediately.
- Keep production mode unchanged.

3. Add optional watch-side stress tools:
- Sweep mode to animate altitude from -18 to +55 and cycle azimuth.
- On-screen tiny debug overlay with altitude/azimuth in dev mode only.

4. Build a screenshot harness:
- Script captures all target emulators for standardized solar checkpoints (civil dusk, sunrise, noon, golden hour, deep night).

This would create an objective "pretty and dramatic" acceptance process per platform.

## 10) Suggested Tuning Targets (No Code Yet)

1. BW-specific palette remap
- Create a dedicated BW gradient strategy using ordered dithering in luminance space.

2. Contrast floor by solar zone
- Enforce minimum endpoint luminance delta, especially near 2-6 deg and ~15 deg.

3. Step-size refinement on high-res devices
- Consider adaptive 2/3/4 based on frame budget rather than a hard 45k threshold.

4. Optional hue emphasis near sunrise/sunset
- Gentle saturation boost in twilight bands to improve drama while preserving realism cues.

---

Prepared outcome: documented analysis complete. Awaiting permission to implement Solar Dev Mode and any rendering refinements.

## 11) Davenport Timing Record (America/Chicago)

Location: Davenport, IA
Coordinates used: 41.5236, -90.5776
Timezone: CDT

### 2026-04-12

| Altitude band | Visual expected | Time range |
|---|---|---|
| Altitude <= -18 | deep night navy to dark indigo | 00:00-04:49, 21:18-24:00 |
| -18 to -6 | pre-dawn indigo/violet | 04:49-05:59, 20:08-21:18 |
| -6 to -2 | twilight purple/magenta | 05:59-06:21, 19:45-20:08 |
| -2 to +2 | sunrise/sunset coral-orange-pink transition | 06:21-06:43, 19:24-19:45 |
| +2 to +10 | warm golden-hour orange/peach | 06:43-07:26, 18:41-19:24 |
| +10 to +20 | daytime sky blues | 07:26-08:19, 17:47-18:41 |
| +20 | brighter blue/cyan daytime | 08:19-17:47 |
| +35 | extra daylight enhancement | 09:42-16:24 |

### 2026-04-13

| Altitude band | Visual expected | Time range |
|---|---|---|
| Altitude <= -18 | deep night navy to dark indigo | 00:00-04:47, 21:19-24:00 |
| -18 to -6 | pre-dawn indigo/violet | 04:47-05:57, 20:09-21:19 |
| -6 to -2 | twilight purple/magenta | 05:57-06:20, 19:47-20:09 |
| -2 to +2 | sunrise/sunset coral-orange-pink transition | 06:20-06:41, 19:25-19:47 |
| +2 to +10 | warm golden-hour orange/peach | 06:41-07:24, 18:42-19:25 |
| +10 to +20 | daytime sky blues | 07:24-08:18, 17:48-18:42 |
| +20 | brighter blue/cyan daytime | 08:18-17:48 |
| +35 | extra daylight enhancement | 09:40-16:25 |
