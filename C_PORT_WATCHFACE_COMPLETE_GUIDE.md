# Sky Watchface: Complete Architecture and C Port Guide

Date: 2026-04-11
Project: Sky (Pebble watchface)
Goal: Document every major behavior in the current app and provide a complete, implementation-ready path to build an equivalent C watchface.

## 1. What The App Is

Sky is a color Pebble watchface that renders a sky-like gradient based on current sun position, then overlays time and location/altitude text.

Active support scope (2026-04-12): basalt, chalk, emery, gabbro.

The system is split into two cooperating runtimes:

- Watch runtime (currently Moddable JS on device): renders UI and handles watch-side interaction states.
- Phone runtime (pkjs JavaScript): fetches location, computes sun angles, reverse-geocodes city, and sends data to watch.

At runtime, the watchface shows:

- Animated loading card with stage labels and progress percentages (cold start/reconnect only).
- Solar gradient background with directional angle based on azimuth.
- Time text centered.
- Footer line with city and altitude.
- Style override modes for text contrast.

## 2. Current Project Architecture

## 2.1 Core Files

- package metadata and message keys: package.json
- generated app metadata and numeric key mapping: build/appinfo.json
- native entry bootstrap for Moddable machine: src/c/mdbl.c
- watch rendering logic (current implementation): src/embeddedjs/main.js
- watch-side embedded runtime config: src/embeddedjs/manifest.json
- phone-side logic (location/sun/geocode/messaging): src/pkjs/index.js
- Clay config UI schema: src/pkjs/config.js
- build orchestration: wscript

## 2.2 Build Characteristics

- projectType is currently moddable.
- Targets: basalt, chalk, emery, gabbro.
- enableMultiJS is true.
- Watchface flag is true.
- No static media assets are currently bundled.

## 2.3 Message Key Contract

Message keys are generated from package.json at build time.

Core groups now include:

- loading and refresh control: StatusCode, ProgressPercent, ReloadFaceToken, RefreshRequest
- solar payload: SourceCode, LatitudeE6, LongitudeE6, AzimuthDegX100, AltitudeDegX100, GradientAngleDegX100, ComputedAtEpoch, CityName
- text and motion settings: TextOverrideMode, MotionMode, BatterySaveMode
- per-device time sizing: TimeSizeBasalt, TimeSizeChalk, TimeSizeEmery, TimeSizeGabbro
- footer visibility: ShowLocation, ShowAltitude
- weather settings and payload: WeatherEnabled, WeatherUnitFahrenheit, WeatherDetailLevel, WeatherStatus, WeatherTempX10, WeatherCloudCover, WeatherCode, WeatherWindX10, WeatherPrecipX100, WeatherUpdatedEpoch
- location/dev controls: CustomLocationEnabled, CustomLatitudeE6, CustomLongitudeE6, DevModeEnabled, DevLatitudeE6, DevLongitudeE6, DevReferenceEpoch, DevSweepEnabled, DevSweepCycleSeconds, DevShowDebugOverlay, DebugBenchmark

## 3. End-To-End Runtime Flow

## 3.1 Startup

1. Watch app boots and draws loading card immediately.
2. pkjs receives ready event.
3. pkjs sends style/settings tuple(s).
4. pkjs starts solar update pipeline:
   - Status: starting -> grabbing location -> calculating sun -> resolving city -> sending payload -> ready.
5. pkjs sends complete payload (lat/lon/azimuth/altitude/gradient angle/city/computed timestamp).
6. Watch applies payload, reaches 100%, briefly holds loading completion, then transitions to watchface.

## 3.2 Ten-Minute Refresh

- pkjs schedules updates every 10 minutes and pushes fresh payload.
- Routine refreshes update the face in place (no full loading-card takeover).
- Full loading-card UX is reserved for cold start and reconnect recovery.

## 3.3 Retry Behavior

- If phone geolocation fails, pkjs falls back to Chicago and schedules retry in 5 minutes.

## 3.4 Reconnect Flow

- Watch detects app reconnection and sends ReloadFaceToken to request fresh data.
- pkjs interprets request and runs requestAndSendSolar("watch-request").

## 4. Watch-Side Features (Current Behavior)

All in src/embeddedjs/main.js.

## 4.1 Solar Gradient Renderer

The watch computes a palette from solar altitude and then draws a directional gradient.

Pipeline:

1. Determine top/bottom palette for current altitude.
2. For altitudes above max daytime band, widen contrast.
3. Compute projected factor for each 2x2 pixel block using gradient angle.
4. Apply small deterministic dither offset per block.
5. Fill block color by lerping RGB channels from top to bottom.

Important constants:

- step size: 2 pixels (performance + style balance)
- dither strength: 0.03

Atmosphere bands:

- -18 deg: deep night
- -6 deg: twilight
- -2 deg: dawn/dusk transition
- 2 deg: sunrise/sunset warm tones
- 10 deg: stronger daylight warmth
- 20 deg: bright daytime sky

## 4.2 Text and Contrast Modes

Text modes:

- 0 auto contrast
- 1 force white
- 2 force black
- 3 black with glow

Auto mode uses weighted luminance:

brightness = (R * 299 + G * 587 + B * 114) / 1000

Threshold > 145 chooses dark text.

The implementation samples separate gradient points for:

- time text (mid-screen sample)
- info/footer text (lower sample)

This prevents poor readability when top and bottom backgrounds diverge.

## 4.3 Time and Footer

- Time is centered, 12-hour format, minute precision.
- Footer text: city plus altitude in degrees.
- Footer uses dynamic font size based on display height.

## 4.4 Loading Card UX

Loading card includes:

- title
- status text
- progress bar
- percentage label

Progress model:

- stage floor and stage ceiling ranges.
- target progress from phone updates.
- idle ramp while waiting in stage.
- stale messaging:
  - after ~3.5s in location stage: "Waiting for phone GPS"
  - after ~6s overall: "Still loading"
  - after ~8s timeout: show fallback face even without payload

## 4.5 Event Handling

- minutechange: refresh time.
- connected: if app connected and payload existed, trigger reconnect refresh.
- resize: redraw.
- interval timer (125ms): animate loading progress/status.

## 4.6 Data Model State

Core watch variables:

- hasPayload
- launchDone
- loadingStatusCode
- loadingProgress
- loadingProgressTarget
- loadingStatusText
- solar struct values (lat/lon/azimuth/altitude/angle/timestamp/city/source)

## 5. Phone-Side Features (Current Behavior)

All in src/pkjs/index.js.

## 5.1 Settings and Persistence

Uses localStorage keys:

- solar-gradient-settings-v1
- solar-gradient-solar-cache-v1
- solar-gradient-reload-token-v1

Settings include:

- TextOverrideMode
- MotionMode
- BatterySaveMode
- TimeSizeBasalt
- TimeSizeChalk
- TimeSizeEmery
- TimeSizeGabbro
- ShowLocation
- ShowAltitude
- WeatherEnabled
- WeatherUnitFahrenheit
- WeatherDetailLevel
- ForceChicagoForTesting
- CustomLocationEnabled
- CustomLatitude
- CustomLongitude
- DevModeEnabled
- DevLatitude
- DevLongitude
- DevDateTime
- DevSweepEnabled
- DevSweepCycleSeconds
- DevShowDebugOverlay
- DebugBenchmark

## 5.2 Location Source Selection

Priority:

1. ForceChicagoForTesting
2. Custom location
3. phone geolocation
4. Chicago fallback on failure/unavailable

## 5.3 Solar Computation

Uses SunCalc with current time and lat/lon.

Computed values:

- azimuth in degrees, normalized to [0, 360)
- gradient angle = (azimuth + 180) mod 360
- altitude in degrees
- all transported as fixed-point integers (x100 or E6)

## 5.4 Reverse Geocoding Strategy

Provider chain:

1. Nominatim
2. maps.co
3. BigDataCloud

For each provider:

- call JSON endpoint with timeout.
- parse city/town/village/etc fields.
- reject generic source labels.

If all providers fail:

- fallback to nearest city from an internal known-city list.

## 5.5 Status Messaging

pkjs emits status updates with progress percentages throughout pipeline.

Nominal sequence:

- starting (5)
- grabbing location (14 -> 34)
- calculating sun (48)
- resolving city (58 -> 76 dynamic)
- sending payload (88)
- complete (100)

## 5.6 Payload Caching

- Last successful solar payload is cached.
- Cached payload can be re-sent with source=cached in failure/lag conditions.

## 5.7 Clay Configuration

Config UI sections:

- Time readability mode select.
- Location controls (force Chicago, custom lat/lon).
- Debug benchmark toggle.

Flow:

- showConfiguration opens Clay URL.
- webviewclosed parses Clay response.
- settings are saved and pushed to watch.
- fresh solar update is requested.

## 6. Reliability, Failure Modes, and Existing Mitigations

## 6.1 Crash Class: Undefined Function Call

Observed prior failure:

- watch crashed with fxAbort unhandled exception: call: not a function
- cause: removed helper function still referenced by draw loop

Mitigation pattern:

- when removing code for memory, run symbol search for all call sites before build.
- prefer replacing with inline primitive checks over helper removal without references cleanup.

## 6.2 Memory Pressure

History indicates tight constraints in embedded runtime.

Mitigations already used:

- removed disconnected icon feature to free headroom.
- kept rendering and state model compact.
- reduced feature scope on watch when necessary.

## 6.3 Network and Geocode Fragility

Mitigations:

- multi-provider geocoding fallback.
- nearest-known-city fallback.
- progress updates so user sees activity.

## 6.4 Startup UX Risks

Mitigations:

- loading timeout escapes to fallback view.
- stale state hints reduce user confusion.

## 7. How To Build A C Version As Similar As Possible

This section is the direct implementation blueprint.

Recommendation: Keep phone-side pkjs logic mostly intact and replace only watch-side Moddable renderer with native C watchface.

Why this is best:

- preserves proven geolocation and geocoding logic.
- avoids re-implementing network stacks on watch (not practical on Pebble).
- retains message contract and Clay settings with minimal changes.

## 7.1 Target Architecture For C Port

Watch side in C:

- window + canvas layer
- gradient renderer in update proc
- text rendering and contrast logic
- loading card state machine
- appmessage inbox parser
- timers and connection handlers

Phone side stays in JS:

- src/pkjs/index.js continues to compute and send payload
- src/pkjs/config.js continues Clay UI

## 7.2 File Plan For C Watchface

Create:

- src/c/main.c
- src/c/model.h
- src/c/model.c
- src/c/render_gradient.h
- src/c/render_gradient.c
- src/c/render_loading.h
- src/c/render_loading.c
- src/c/messages.h
- src/c/messages.c
- src/c/state.h

Retire:

- src/c/mdbl.c (or keep out of build)
- src/embeddedjs/main.js (watch path only)
- Moddable-specific prebuild steps when fully migrated

## 7.3 Data Structures In C

Use fixed-point integers for parity with payload format.

Example state model:

- int32_t latitude_e6
- int32_t longitude_e6
- int32_t azimuth_x100
- int32_t altitude_x100
- int32_t gradient_angle_x100
- int32_t computed_at_epoch
- int32_t source_code
- char city_name[48]
- bool has_payload
- bool launch_done
- uint8_t text_mode
- uint8_t loading_status
- uint8_t loading_progress
- uint8_t loading_target

## 7.4 Message Parsing In C

Use AppMessage inbox callback:

1. DictionaryIterator over tuples.
2. switch by key.
3. parse int/string values.
4. apply state updates.
5. if full solar payload present, mark has_payload true and launch_done true after brief delay.

For reconnect request:

- send outbox tuple ReloadFaceToken (or RefreshRequest) on connection event.
- pkjs will compute and resend data.

## 7.5 Rendering In C

## 7.5.1 Gradient

In Layer update proc:

1. Compute palette top/bottom from altitude bands.
2. Convert angle_x100 to float degrees once per frame.
3. Iterate y then x in step=2 blocks.
4. Compute projected factor and optional tiny dither.
5. Build GColor8 from lerped RGB.
6. graphics_context_set_fill_color + graphics_fill_rect.

Performance tips:

- only redraw when needed (minute tick, loading tick, message update).
- keep step at 2 for near-identical look and acceptable draw cost.
- avoid per-pixel floating point where possible; precompute sin/cos or use lookup table.

## 7.5.2 Text

- time: FONT_KEY_BITHAM_42_BOLD
- footer: FONT_KEY_GOTHIC_18_BOLD or 14 based on bounds height
- choose text color via sampled luminance threshold (145)
- glow mode: draw white offsets around black text

## 7.5.3 Loading Card

Draw full-screen black, centered panel, title, status label, progress track/fill, percent.

Mirror status ramps:

- stage floor/ceiling functions
- idle ramp increase every timer tick
- stale messaging thresholds 3500ms and 6000ms
- timeout to fallback at 8000ms

## 7.6 Time and Scheduling In C

Services:

- TickTimerService for minute updates.
- AppTimer every 125ms while in loading state.
- ConnectionService to detect app/phone reconnection.

On minute tick:

- update cached time struct
- mark layer dirty

On loading timer tick:

- advance progress toward target
- apply stale label transitions
- if timeout exceeded, set launch_done true
- mark layer dirty

## 7.7 Connection and Refresh In C

Implement connection callback that:

- checks if watch app connection is active and has_payload is true
- enters loading mode for reconnect
- sends ReloadFaceToken incremented counter via outbox

This preserves current reconnect semantics.

## 7.8 Settings Parity

Keep using pkjs and Clay to send settings tuples to watch.

C watch should consume:

- TextOverrideMode
- CustomLocationEnabled (optional display/debug use)
- CustomLatitudeE6 (optional)
- CustomLongitudeE6 (optional)
- DebugBenchmark (optional)

Only TextOverrideMode is mandatory for visual parity.

## 7.9 Build System Changes For Full C Migration

When removing Moddable watch runtime:

1. Update package.json projectType to native.
2. Remove Moddable prebuild steps from scripts/tooling.
3. Ensure wscript C source glob includes new files.
4. Keep pkjs bundle settings for phone JS.
5. Run clean build to regenerate message headers:
   - pebble clean
   - pebble build

## 8. Mathematical Parity Notes

To match visuals exactly, preserve these equations.

Azimuth normalization:

azimuth_deg = ((raw_azimuth_deg + 180) mod 360 + 360) mod 360

Gradient direction:

gradient_angle_deg = (azimuth_deg + 180) mod 360

Projected factor at pixel block center:

- vx = sin(angle_rad)
- vy = -cos(angle_rad)
- projection = ((x-cx)*vx + (y-cy)*vy)
- max_projection = abs(width*vx)/2 + abs(height*vy)/2
- factor = clamp((projection/max_projection)*0.5 + 0.5, 0, 1)

Brightness for text mode auto:

brightness = (R*299 + G*587 + B*114) / 1000

Use threshold 145.

## 9. Visual Parity Checklist

To claim parity, verify all of these:

- loading card appears immediately at boot
- status labels and percentages progress similarly
- gradient orientation shifts through day
- time centered and readable in all backgrounds
- footer city and altitude render correctly
- reconnect triggers loading then fresh payload
- routine 10-minute refresh updates city/altitude/gradient without full loading card
- fallback still shows usable face if loading times out

## 10. Test Plan For C Port

## 10.1 Unit-ish Logic Checks (host mindset)

- altitude band interpolation boundaries
- text contrast threshold behavior
- progress floor/ceiling logic
- message parse handling for partial payload

## 10.2 Emulator Functional Tests

- clean install and cold boot
- no phone location available
- geocode provider failures
- manual location enabled
- force Chicago enabled
- reconnect path
- long loading path (simulate delayed phone)

## 10.3 Regression Guards

- log fatal callback errors in inbox/outbox
- assert message key coverage in parser switch
- run grep before removing helper functions to avoid stale call sites

## 11. Implementation Sequence You Can Hand Back For Coding

Phase 1:

- Create C app skeleton with window/canvas/time text.

Phase 2:

- Add gradient renderer with fixed test values.

Phase 3:

- Add appmessage parser and payload model.

Phase 4:

- Add loading card and stage/progress animation logic.

Phase 5:

- Add reconnect token sender and connection handlers.

Phase 6:

- Wire text mode setting and glow mode.

Phase 7:

- Remove Moddable watch path and switch project metadata/build pipeline fully to native C.

Phase 8:

- Run parity test matrix and tune visuals/perf.

## 12. C Port Acceptance Criteria (Definition of Done)

A C version is acceptable when:

- watchface boots reliably on emery and gabbro
- no crash loops or unhandled appmessage errors
- all primary visual features match existing behavior
- phone settings update behavior is preserved
- reconnect refresh and routine 10-minute updates are operational
- memory footprint remains stable with safety headroom

## 13. Practical Notes For The Next Build Request

When you are ready for implementation, provide this guide and ask for:

- exact file creation and edits for C migration
- strict parity-first implementation (no design changes)
- emulator verification with screenshot evidence
- stepwise commits by phase

That request will allow direct execution against this specification without missing feature details.
