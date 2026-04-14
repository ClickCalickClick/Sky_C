import re

with open('src/c/Sky.c', 'r') as f:
    text = f.read()

# Let's extract the common part.
start_str = "  float event_moment = prv_event_moment_strength(now_ms);\n  float angle = prv_effective_gradient_angle_deg(now_ms, profile);"
end_str = "  texture_strength *= 1.0f + (0.18f * event_moment);"

matches = re.finditer(re.escape(start_str) + r'(.*?)' + re.escape(end_str), text, re.DOTALL)
m_list = list(matches)
print(f"Found {len(m_list)} matches to extract.")

# Let's write the struct at the top of prv_sample_gradient_color
struct_def = """
typedef struct {
  int16_t width;
  int16_t height;
  float vx;
  float vy;
  float cx;
  float cy;
  float projection_scale;
  int16_t step;
  float sun_vx;
  float sun_vy;
  float min_dim;
  float altitude_normalized;
  float horizon_center_t;
  float horizon_target_t;
  float horizon_half_width;
  float horizon_strength;
  float texture_strength;
  float bloom_x;
  float bloom_y;
  float bloom_radius;
  float bloom_radius_sq;
  float bloom_gain;
} AtmosphereParams;

static AtmosphereParams prv_calc_atmosphere(GRect bounds, uint32_t now_ms, const RenderProfile *profile, float altitude_deg) {
  AtmosphereParams p;
  p.width = bounds.size.w;
  p.height = bounds.size.h;

  float event_moment = prv_event_moment_strength(now_ms);
  float angle = prv_effective_gradient_angle_deg(now_ms, profile);
  p.step = GRADIENT_STEP;
  if ((int32_t)p.width * (int32_t)p.height > profile->large_step_area_threshold) {
    p.step = profile->gradient_step_large;
  }

  int32_t trig_angle = (int32_t)(angle * (float)TRIG_MAX_ANGLE / 360.0f);
  p.vx = sin_lookup(trig_angle) / (float)TRIG_MAX_RATIO;
  p.vy = -cos_lookup(trig_angle) / (float)TRIG_MAX_RATIO;
  p.cx = (float)p.width * 0.5f;
  p.cy = (float)p.height * 0.5f;
  float max_projection = (prv_absf((float)p.width * p.vx) * 0.5f) + (prv_absf((float)p.height * p.vy) * 0.5f);
  float denom = max_projection > 0.000001f ? max_projection : 0.000001f;
  p.projection_scale = 0.5f / denom;

  if (s_state.gradient_spread == 1) {
    p.projection_scale *= 1.35f;
  } else if (s_state.gradient_spread == 2) {
    p.projection_scale *= 0.65f;
  } else if (s_state.gradient_spread == 3) {
    p.projection_scale *= 0.40f;
  } else if (s_state.gradient_spread == 4) {
    p.projection_scale *= 2.05f;
  }

  float daylight_strength = prv_clampf((altitude_deg - 25.0f) / 30.0f, 0.0f, 1.0f);
  p.altitude_normalized = prv_clampf((altitude_deg + 5.0f) / 70.0f, 0.0f, 1.0f);
  float phase_energy = prv_clampf((altitude_deg + 8.0f) / 30.0f, 0.0f, 1.0f);
  uint16_t daily_seed = prv_daily_variation_seed();

  int32_t sun_trig = (int32_t)(((float)s_state.azimuth_deg_x100 / 100.0f) * (float)TRIG_MAX_ANGLE / 360.0f);
  p.sun_vx = sin_lookup(sun_trig) / (float)TRIG_MAX_RATIO;
  p.sun_vy = -cos_lookup(sun_trig) / (float)TRIG_MAX_RATIO;

  p.min_dim = p.width < p.height ? p.width : p.height;
  float bloom_offset = (1.0f - p.altitude_normalized) * (p.min_dim * 0.30f);
  p.bloom_x = p.cx + (p.sun_vx * bloom_offset);
  p.bloom_y = p.cy + (p.sun_vy * bloom_offset);
  float bloom_variation = ((float)(daily_seed & 31) - 15.0f) / 15.0f;
  p.bloom_radius = p.min_dim * (0.18f + (0.08f * daylight_strength * profile->bloom_radius_mult));
  p.bloom_radius *= 1.0f + (0.02f * bloom_variation);
  p.bloom_radius_sq = p.bloom_radius * p.bloom_radius;
  p.bloom_gain = 0.11f * daylight_strength * profile->bloom_gain_mult;
  p.bloom_gain *= 1.0f + (0.05f * bloom_variation);
  p.bloom_gain *= 1.0f + (0.45f * event_moment);

  p.horizon_center_t = prv_clampf(0.58f + (0.08f * (0.5f - p.altitude_normalized)), 0.44f, 0.70f);
  p.horizon_target_t = prv_clampf(0.62f + (0.08f * (0.5f - p.altitude_normalized)), 0.48f, 0.74f);
  p.horizon_half_width = 0.18f;
  p.horizon_strength = (0.035f + (0.055f * phase_energy)) * (1.0f - (0.45f * daylight_strength));
  p.horizon_strength *= profile->gradient_widen_mult;
  p.horizon_strength *= 1.0f + (0.35f * event_moment);

  int32_t motion_mode = prv_effective_motion_mode();
  p.texture_strength = 0.012f + (0.016f * phase_energy);
  if (motion_mode == MOTION_MODE_CALM) {
    p.texture_strength *= 0.65f;
  } else if (motion_mode == MOTION_MODE_DYNAMIC) {
    p.texture_strength *= 1.30f;
  }
  p.texture_strength *= 1.0f + (0.18f * event_moment);

  return p;
}
"""

"""
In prv_sample_gradient_color:
  int16_t width = bounds.size.w;
  int16_t height = bounds.size.h;
  float altitude_deg = (float)s_state.altitude_deg_x100 / 100.0f;
  ... end of match ...
"""

"""
In prv_draw_solar_gradient:
  int16_t width = bounds.size.w;
  int16_t height = bounds.size.h;
  float altitude_deg = (float)s_state.altitude_deg_x100 / 100.0f;
  ... end of match ...
"""

