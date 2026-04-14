import re

with open('src/c/Sky.c', 'r') as f:
    text = f.read()

OLD_TEXT = """  int16_t width = bounds.size.w;
  int16_t height = bounds.size.h;
  float altitude_deg = (float)s_state.altitude_deg_x100 / 100.0f;
  float event_moment = prv_event_moment_strength(now_ms);
  float angle = prv_effective_gradient_angle_deg(now_ms, profile);
  int16_t step = GRADIENT_STEP;
  if ((int32_t)width * (int32_t)height > profile->large_step_area_threshold) {
    step = profile->gradient_step_large;
  }

  int32_t trig_angle = (int32_t)(angle * (float)TRIG_MAX_ANGLE / 360.0f);
  float vx = sin_lookup(trig_angle) / (float)TRIG_MAX_RATIO;
  float vy = -cos_lookup(trig_angle) / (float)TRIG_MAX_RATIO;
  float cx = (float)width * 0.5f;
  float cy = (float)height * 0.5f;
  float max_projection = (prv_absf((float)width * vx) * 0.5f) + (prv_absf((float)height * vy) * 0.5f);
  float denom = max_projection > 0.000001f ? max_projection : 0.000001f;
  float projection_scale = 0.5f / denom;

  if (s_state.gradient_spread == 1) {
    projection_scale *= 1.35f;
  } else if (s_state.gradient_spread == 2) {
    projection_scale *= 0.65f;
  } else if (s_state.gradient_spread == 3) {
    projection_scale *= 0.40f;
  } else if (s_state.gradient_spread == 4) {
    projection_scale *= 2.05f;
  }

  float daylight_strength = prv_clampf((altitude_deg - 25.0f) / 30.0f, 0.0f, 1.0f);
  float altitude_normalized = prv_clampf((altitude_deg + 5.0f) / 70.0f, 0.0f, 1.0f);
  float phase_energy = prv_clampf((altitude_deg + 8.0f) / 30.0f, 0.0f, 1.0f);
  uint16_t daily_seed = prv_daily_variation_seed();

  int32_t sun_trig = (int32_t)(((float)s_state.azimuth_deg_x100 / 100.0f) * (float)TRIG_MAX_ANGLE / 360.0f);
  float sun_vx = sin_lookup(sun_trig) / (float)TRIG_MAX_RATIO;
  float sun_vy = -cos_lookup(sun_trig) / (float)TRIG_MAX_RATIO;

  float min_dim = width < height ? width : height;
  float bloom_offset = (1.0f - altitude_normalized) * (min_dim * 0.30f);
  float bloom_x = cx + (sun_vx * bloom_offset);
  float bloom_y = cy + (sun_vy * bloom_offset);
  float bloom_variation = ((float)(daily_seed & 31) - 15.0f) / 15.0f;
  float bloom_radius = min_dim * (0.18f + (0.08f * daylight_strength * profile->bloom_radius_mult));
  bloom_radius *= 1.0f + (0.02f * bloom_variation);
  float bloom_radius_sq = bloom_radius * bloom_radius;
  float bloom_gain = 0.11f * daylight_strength * profile->bloom_gain_mult;
  bloom_gain *= 1.0f + (0.05f * bloom_variation);
  bloom_gain *= 1.0f + (0.45f * event_moment);

  float horizon_center_t = prv_clampf(0.58f + (0.08f * (0.5f - altitude_normalized)), 0.44f, 0.70f);
  float horizon_target_t = prv_clampf(0.62f + (0.08f * (0.5f - altitude_normalized)), 0.48f, 0.74f);
  float horizon_half_width = 0.18f;
  float horizon_strength = (0.035f + (0.055f * phase_energy)) * (1.0f - (0.45f * daylight_strength));
  horizon_strength *= profile->gradient_widen_mult;
  horizon_strength *= 1.0f + (0.35f * event_moment);

  int32_t motion_mode = prv_effective_motion_mode();
  float texture_strength = 0.012f + (0.016f * phase_energy);
  if (motion_mode == MOTION_MODE_CALM) {
    texture_strength *= 0.65f;
  } else if (motion_mode == MOTION_MODE_DYNAMIC) {
    texture_strength *= 1.30f;
  }
  texture_strength *= 1.0f + (0.18f * event_moment);"""

NEW_TEXT = """  AtmosphereParams p = prv_calculate_atmosphere_params(bounds, now_ms, profile, (float)s_state.altitude_deg_x100 / 100.0f);"""

if text.count(OLD_TEXT) == 2:
    print("Found 2 occurrences of the huge block!")
    
    struct_def = "typedef struct {\n"
    struct_def += "  int16_t width;\n"
    struct_def += "  int16_t height;\n"
    struct_def += "  float vx;\n"
    struct_def += "  float vy;\n"
    struct_def += "  float cx;\n"
    struct_def += "  float cy;\n"
    struct_def += "  float projection_scale;\n"
    struct_def += "  int16_t step;\n"
    struct_def += "  float horizon_center_t;\n"
    struct_def += "  float horizon_target_t;\n"
    struct_def += "  float horizon_half_width;\n"
    struct_def += "  float horizon_strength;\n"
    struct_def += "  float texture_strength;\n"
    struct_def += "  float bloom_x;\n"
    struct_def += "  float bloom_y;\n"
    struct_def += "  float bloom_radius_sq;\n"
    struct_def += "  float bloom_gain;\n"
    struct_def += "} AtmosphereParams;\n\n"
    
    struct_def += "static AtmosphereParams prv_calculate_atmosphere_params(GRect bounds, uint32_t now_ms, const RenderProfile *profile, float altitude_deg) {\n"
    struct_def += "  AtmosphereParams p;\n"
    struct_def += "  p.width = bounds.size.w;\n"
    struct_def += "  p.height = bounds.size.h;\n"
    struct_def += OLD_TEXT[59:].replace("int16_t step", "p.step").replace("width", "p.width").replace("height", "p.height")
    struct_def += "\n  return p;\n}\n"

    # Replace usages in struct_def inside logic
    struct_def = struct_def.replace("float vx =", "p.vx =")
    struct_def = struct_def.replace("float vy =", "p.vy =")
    struct_def = struct_def.replace("float cx =", "p.cx =")
    struct_def = struct_def.replace("float cy =", "p.cy =")
    struct_def = struct_def.replace("float projection_scale =", "p.projection_scale =")
    struct_def = struct_def.replace("float horizon_center_t =", "p.horizon_center_t =")
    struct_def = struct_def.replace("float horizon_target_t =", "p.horizon_target_t =")
    struct_def = struct_def.replace("float horizon_half_width =", "p.horizon_half_width =")
    struct_def = struct_def.replace("float horizon_strength =", "p.horizon_strength =")
    struct_def = struct_def.replace("float texture_strength =", "p.texture_strength =")
    struct_def = struct_def.replace("float bloom_x =", "p.bloom_x =")
    struct_def = struct_def.replace("float bloom_y =", "p.bloom_y =")
    struct_def = struct_def.replace("float bloom_radius_sq =", "p.bloom_radius_sq =")
    struct_def = struct_def.replace("float bloom_gain =", "p.bloom_gain =")
    
    # We also need to map uses of variable names to p.<name>
    struct_def = struct_def.replace("step =", "p.step =")
    struct_def = struct_def.replace(" cx ", " p.cx ")
    struct_def = struct_def.replace(" cy ", " p.cy ")
    struct_def = struct_def.replace(" vx ", " p.vx ")
    struct_def = struct_def.replace(" vy ", " p.vy ")
    struct_def = struct_def.replace(" projection_scale ", " p.projection_scale ")
    struct_def = struct_def.replace(" projection_scale*", " p.projection_scale*")

    # The usages inside prv_sample_gradient_color and prv_draw_solar_gradient need replacement too
    text = text.replace(OLD_TEXT, NEW_TEXT)

    hook = "static Rgb prv_sample_gradient_color("
    text = text.replace(hook, struct_def + "\n" + hook)

else:
    print(f"ERROR: Found {text.count(OLD_TEXT)} occurrences instead of 2!")
