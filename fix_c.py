import os

with open('src/c/Sky.c', 'r') as f:
    text = f.read()

def repl(old, new):
    global text
    if old in text:
        text = text.replace(old, new)
        print("Success")
    else:
        print("Failed to find")

struct_start = "static Rgb prv_sample_gradient_color(Palette palette, GRect bounds, uint32_t now_ms,"

struct_code = """typedef struct {
  float vx;
  float vy;
  float cx;
  float cy;
  float projection_scale;
  int16_t step;
  float horizon_center_t;
  float horizon_half_width;
  float horizon_target_t;
  float horizon_strength;
  float texture_strength;
  float bloom_x;
  float bloom_y;
  float bloom_radius_sq;
  float bloom_gain;
} AtmosphereParams;

static AtmosphereParams prv_calculate_atmosphere_params(GRect bounds, uint32_t now_ms, const RenderProfile *profile, float altitude_deg) {
  AtmosphereParams p;
  int16_t width = bounds.size.w;
  int16_t height = bounds.size.h;

  float event_moment = prv_event_moment_strength(now_ms);
  float angle = prv_effective_gradient_angle_deg(now_ms, profile);
  p.step = GRADIENT_STEP;
  if ((int32_t)width * (int32_t)height > profile->large_step_area_threshold) {
    p.step = profile->gradient_step_large;
  }

  int32_t trig_angle = (int32_t)(angle * (float)TRIG_MAX_ANGLE / 360.0f);
  p.vx = sin_lookup(trig_angle) / (float)TRIG_MAX_RATIO;
  p.vy = -cos_lookup(trig_angle) / (float)TRIG_MAX_RATIO;
  p.cx = (float)width * 0.5f;
  p.cy = (float)height * 0.5f;
  float max_projection = (prv_absf((float)width * p.vx) * 0.5f) + (prv_absf((float)height * p.vy) * 0.5f);
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
  float altitude_normalized = prv_clampf((altitude_deg + 5.0f) / 70.0f, 0.0f, 1.0f);
  float phase_energy = prv_clampf((altitude_deg + 8.0f) / 30.0f, 0.0f, 1.0f);
  uint16_t daily_seed = prv_daily_variation_seed();

  int32_t sun_trig = (int32_t)(((float)s_state.azimuth_deg_x100 / 100.0f) * (float)TRIG_MAX_ANGLE / 360.0f);
  float sun_vx = sin_lookup(sun_trig) / (float)TRIG_MAX_RATIO;
  float sun_vy = -cos_lookup(sun_trig) / (float)TRIG_MAX_RATIO;

  float min_dim = width < height ? width : height;
  float bloom_offset = (1.0f - altitude_normalized) * (min_dim * 0.30f);
  p.bloom_x = p.cx + (sun_vx * bloom_offset);
  p.bloom_y = p.cy + (sun_vy * bloom_offset);
  float bloom_variation = ((float)(daily_seed & 31) - 15.0f) / 15.0f;
  float bloom_radius = min_dim * (0.18f + (0.08f * daylight_strength * profile->bloom_radius_mult));
  bloom_radius *= 1.0f + (0.02f * bloom_variation);
  p.bloom_radius_sq = bloom_radius * bloom_radius;
  p.bloom_gain = 0.11f * daylight_strength * profile->bloom_gain_mult;
  p.bloom_gain *= 1.0f + (0.05f * bloom_variation);
  p.bloom_gain *= 1.0f + (0.45f * event_moment);

  p.horizon_center_t = prv_clampf(0.58f + (0.08f * (0.5f - altitude_normalized)), 0.44f, 0.70f);
  p.horizon_target_t = prv_clampf(0.62f + (0.08f * (0.5f - altitude_normalized)), 0.48f, 0.74f);
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
repl(struct_start, struct_code + '\n' + struct_start)

redundant = """  int16_t width = bounds.size.w;
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

param_call = """  AtmosphereParams p = prv_calculate_atmosphere_params(bounds, now_ms, profile, (float)s_state.altitude_deg_x100 / 100.0f);"""

repl(redundant, param_call)
repl(redundant, param_call) # Call twice to replace both!

# fix prv_sample variables
c1 = """  float y_t = prv_clampf(sample_y / (float)height, 0.0f, 1.0f);
  float center_band = 1.0f - prv_clampf(prv_absf(y_t - 0.5f) / 0.34f, 0.0f, 1.0f);
  float horizon_weight = 1.0f - prv_clampf(prv_absf(y_t - horizon_center_t) / horizon_half_width, 0.0f, 1.0f);
  horizon_weight *= horizon_weight;

  float projection = ((sample_y - cy) * vy) + ((sample_x - cx) * vx);
  float raw_factor = prv_clampf((projection * projection_scale) + 0.5f, 0.0f, 1.0f);
  float smooth_factor = raw_factor * raw_factor * (3.0f - (2.0f * raw_factor));

  float noise_x = sample_x * 0.012f;
  float noise_y = (sample_y * 0.045f) - ((float)now_ms * 0.0003f);
  float noise = prv_noise_2d(noise_x, noise_y) - 0.5f;"""

c2 = """  float y_t = prv_clampf(sample_y / (float)bounds.size.h, 0.0f, 1.0f);
  float center_band = 1.0f - prv_clampf(prv_absf(y_t - 0.5f) / 0.34f, 0.0f, 1.0f);
  float horizon_weight = 1.0f - prv_clampf(prv_absf(y_t - p.horizon_center_t) / p.horizon_half_width, 0.0f, 1.0f);
  horizon_weight *= horizon_weight;

  float projection = ((sample_y - p.cy) * p.vy) + ((sample_x - p.cx) * p.vx);
  float raw_factor = prv_clampf((projection * p.projection_scale) + 0.5f, 0.0f, 1.0f);
  float smooth_factor = raw_factor * raw_factor * (3.0f - (2.0f * raw_factor));

  float noise_x = sample_x * 0.012f;
  float noise_y = (sample_y * 0.045f) - ((float)now_ms * 0.0003f);
  float noise = prv_noise_2d(noise_x, noise_y) - 0.5f;"""

repl(c1, c2)

d1 = """  float sun_intensity = 0.0f;
  if (bloom_gain > 0.0f) {
    float dx = sample_x - bloom_x;
    float dy = sample_y - bloom_y;
    float dist_sq = (dx * dx) + (dy * dy);
    if (dist_sq < bloom_radius_sq) {
      sun_intensity = 1.0f - (dist_sq / bloom_radius_sq);
      sun_intensity *= sun_intensity * bloom_gain;
    }
  }

  uint8_t dither = prv_bayer_matrix[((int)sample_y % 4) * 4 + ((int)sample_x % 4)];
  float dither_val = ((float)dither / 16.0f) - 0.5f;

  float final_factor = prv_clampf(smooth_factor + (noise * texture_strength * center_band) + 
                                (horizon_weight * horizon_strength), 0.0f, 1.0f);"""

d2 = """  float sun_intensity = 0.0f;
  if (p.bloom_gain > 0.0f) {
    float dx = sample_x - p.bloom_x;
    float dy = sample_y - p.bloom_y;
    float dist_sq = (dx * dx) + (dy * dy);
    if (dist_sq < p.bloom_radius_sq) {
      sun_intensity = 1.0f - (dist_sq / p.bloom_radius_sq);
      sun_intensity *= sun_intensity * p.bloom_gain;
    }
  }

  uint8_t dither = prv_bayer_matrix[((int)sample_y % 4) * 4 + ((int)sample_x % 4)];
  float dither_val = ((float)dither / 16.0f) - 0.5f;

  float final_factor = prv_clampf(smooth_factor + (noise * p.texture_strength * center_band) + 
                                (horizon_weight * p.horizon_strength), 0.0f, 1.0f);"""

repl(d1, d2)

# fix prv_draw variables  
e1 = """  for (int16_t y = 0; y < height; y += step) {
    float y_center_t = prv_clampf(((float)y + ((float)step * 0.5f)) / (float)height, 0.0f, 1.0f);
    float center_band = 1.0f - prv_clampf(prv_absf(y_center_t - 0.5f) / 0.34f, 0.0f, 1.0f);
    float horizon_weight = 1.0f - prv_clampf(prv_absf(y_center_t - horizon_center_t) / horizon_half_width, 0.0f, 1.0f);
    horizon_weight *= horizon_weight;
    float y_projection = ((float)y - cy) * vy;"""

e2 = """  for (int16_t y = 0; y < bounds.size.h; y += p.step) {
    float y_center_t = prv_clampf(((float)y + ((float)p.step * 0.5f)) / (float)bounds.size.h, 0.0f, 1.0f);
    float center_band = 1.0f - prv_clampf(prv_absf(y_center_t - 0.5f) / 0.34f, 0.0f, 1.0f);
    float horizon_weight = 1.0f - prv_clampf(prv_absf(y_center_t - p.horizon_center_t) / p.horizon_half_width, 0.0f, 1.0f);
    horizon_weight *= horizon_weight;
    float y_projection = ((float)y - p.cy) * p.vy;"""

repl(e1, e2)

# inside loop draw
f1 = """    for (int16_t x = 0; x < width; x += step) {
      float projection = y_projection + (((float)x - cx) * vx);
      float raw_factor = prv_clampf((projection * projection_scale) + 0.5f, 0.0f, 1.0f);"""

f2 = """    for (int16_t x = 0; x < bounds.size.w; x += p.step) {
      float projection = y_projection + (((float)x - p.cx) * p.vx);
      float raw_factor = prv_clampf((projection * p.projection_scale) + 0.5f, 0.0f, 1.0f);"""

repl(f1, f2)

g1 = """      float noise = detail_noise * texture_strength * center_band;
      float final_factor = prv_clampf(smooth_factor + noise + (horizon_weight * horizon_strength), 0.0f, 1.0f);

      float sun_intensity = 0.0f;
      if (bloom_gain > 0.0f) {
        float dx = (float)x - bloom_x;
        float dy = (float)y - bloom_y;
        float dist_sq = (dx * dx) + (dy * dy);
        if (dist_sq < bloom_radius_sq) {
          sun_intensity = 1.0f - (dist_sq / bloom_radius_sq);
          sun_intensity *= sun_intensity * bloom_gain;
        }
      }"""

g2 = """      float noise = detail_noise * p.texture_strength * center_band;
      float final_factor = prv_clampf(smooth_factor + noise + (horizon_weight * p.horizon_strength), 0.0f, 1.0f);

      float sun_intensity = 0.0f;
      if (p.bloom_gain > 0.0f) {
        float dx = (float)x - p.bloom_x;
        float dy = (float)y - p.bloom_y;
        float dist_sq = (dx * dx) + (dy * dy);
        if (dist_sq < p.bloom_radius_sq) {
          sun_intensity = 1.0f - (dist_sq / p.bloom_radius_sq);
          sun_intensity *= sun_intensity * p.bloom_gain;
        }
      }"""

repl(g1, g2)

h1 = """      GColor color = prv_dithered_color(final_factor, final_lum, dither_strength, dither_val, palette);
      graphics_context_set_fill_color(ctx, color);
      graphics_fill_rect(ctx, GRect(x, y, step, step), 0, GCornerNone);"""

h2 = """      GColor color = prv_dithered_color(final_factor, final_lum, dither_strength, dither_val, palette);
      graphics_context_set_fill_color(ctx, color);
      graphics_fill_rect(ctx, GRect(x, y, p.step, p.step), 0, GCornerNone);"""

repl(h1, h2)

with open('src/c/Sky.c', 'w') as f:
    f.write(text)
