# Optimization Plan

## Phase 1: Drastic Battery Savings (C Processor & Screen Drawing)
**Goal:** Reduce the exorbitant soft-float calculations and full-screen screen redrawing happening every second.
1. **Stop Unconditional Animation Timers:** Your `prv_start_animation_timer` runs continuously (every 550ms-2400ms) and marks the entire canvas layer dirty. *Plan:* Hook the timer to only run when `s_state.angle_transition_active` is `true`, or during a scheduled event transition (`refresh_badge_until_ms`/`event_moment_until_ms`). Once the animation completes, cancel the timer so the watch draws 60x less often each minute.
2. **Eliminate 80% of Noise Calculations:** In `prv_draw_solar_gradient` inside the X/Y loop, you are calling `prv_noise_2d` on every single pixel block (`step`). However, `coarse_x` only changes every 6 horizontal steps. *Plan:* Cache the `cloud_noise` and `detail_noise` values internally to the loop so that the heavy float arithmetic only runs once every 6 pixels.
3. **Hoist Y-Axis Math:** Float operations like `sample_y`, `dy_sq`, and `center_boost` are executed thousands of times within the inner `x` loop. *Plan:* Move these up to the `y` loop where they actually change.
4. **Use Loop Accumulators:** Inside the inner `x` loop, instead of running `projection = y_projection + (((float)x - cx) * vx);` every pixel, calculate an initial `projection` and a `projection_step` before the loop, and simply add `projection += projection_step` on each iteration.

## Phase 2: Eliminate GPS & Bluetooth Battery Drain (JS)
**Goal:** Stop waking up the phone's GPS hardware and sending unnecessarily gigantic AppMessages over Bluetooth.
1. **Fix the Core C Parsing Block:** In `prv_on_message_received`, your watchface hits an early return if `!lat || !lon` is missing from the payload. Because updating the City Name is placed *below* this check, PebbleKit JS is currently forced to send the entire massive solar payload *again* just to update the City Name. *Plan:* Move the city name processing above the `!lat || !lon` sanity check.
2. **Send Drip Updates (JS):** Once the C code accepts partial keys, update `resolveCityName` in `index.js` to only send `{ CityName: cityName }` instead of the 20+ key redundant solar dictionary.
3. **Throttle GPS Hardware:** In `src/pkjs/index.js`, your 10-minute `autoUpdateTimer` requests raw GPS data. *Plan:* Add `{ maximumAge: 3600000, timeout: 10000 }` to `navigator.geolocation.getCurrentPosition()`. The smartphone OS will return a cached cell-tower location rather than violently spinning up the hardware GPS.

## Phase 3: Memory Optimization & Duplicate Code
**Goal:** Regain compiled binary size and reclaim static heap allocations.
1. **Consolidate Gradient Calculations:** `prv_sample_gradient_color` and `prv_draw_solar_gradient` contain exactly 70-80 lines of identical trigonometry and projection math. *Plan:* Extract these variables into an intermediate struct (`AtmosphereParams`) returned by a single helper method.
2. **Reduce String Overhead:** In `prv_draw_face`, you are running `strftime`, `snprintf`, and the exceptionally heavy `graphics_text_layout_get_content_size()` over and over again *every frame*. *Plan:* Generate these layout bounds and strings globally inside `prv_on_minute_tick` (which only fires once a minute) instead of calculating text layouts 10+ times a minute.
3. **AppMessage Buffer Optimization:** `app_message_open(768, 256)` arbitrarily reserves 1,024 bytes of app memory. By slimming down the JS outbox calls (Phase 2), this can safely drop to ~256-512 bytes max, reclaiming strict Pebble RAM for background workers.

## Phase 4: Found Bugs / Missing Logic (No Crashes Found)
**Goal:** Fix user configuration logic.
1. **Fix Missing Config Slots:** In `prv_on_message_received`, there are parser hooks for `FooterSlot3`, `FooterSlot4`, `FooterSlot5`, and `FooterSlot6`, but **`FooterSlot1` and `FooterSlot2` are entirely missing.** If a user tries to change Slot 1 or 2 via the Clay Settings Page, it will silently fail on the watchface. *Plan:* Add Tuple parses for `FooterSlot1` and `FooterSlot2`.
