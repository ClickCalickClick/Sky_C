# Reddit Error Findings: Basalt Boot Crash

## Issue Summary
Users on Pebble Time (PT) and Pebble Time Steel (PTS) — both "Basalt" platforms — are experiencing watch reboots immediately upon launching the watchface. Basalt hardware runs on a slower Cortex-M3/M4 equivalent with tighter memory limits and no native hardware floating-point acceleration.

## Root Causes Identified

1. **Watchdog Timeout via CPU Exhaustion**
   - **Location:** `prv_draw_solar_gradient` inside the inner `for (int16_t x = 0; x < width; x += step)` loop.
   - **Problem:** When rendering a 144x168 screen, the watchface executes heavy per-pixel math (`prv_clampf`, multiplication/division of floats) up to ~24,000 times per frame. Without an FPU, this takes longer than the Pebble OS watchdog limit (~1000ms). When the main thread blocks for too long, the Pebble OS forces a reboot.

2. **Stack Overflow Risk**
   - **Location:** `prv_draw_face`
   - **Problem:** Over 340+ bytes of character buffers (`location_text[48]`, `weather_text[56]`, etc.) are stack-allocated in a single function call. Basalt has a tiny 4KB stack limit. These massive simultaneous allocations, layered with nested function calls and float math overhead, likely corrupt the stack.

3. **Missing Null/Memory Pressure Checks**
   - **Location:** `prv_window_load`
   - **Problem:** `layer_create(bounds)` is called without validating if it returns `NULL`. If the system is under memory pressure, subsequent calls to `layer_set_update_proc(NULL, ...)` will instantly crash the watchface.