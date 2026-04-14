with open('src/c/Sky.c', 'r') as f:
    text = f.read()

old_loop = """  for (int16_t y = 0; y < height; y += step) {
    float y_center_t = prv_clampf(((float)y + ((float)step * 0.5f)) / (float)height, 0.0f, 1.0f);
    float center_band = 1.0f - prv_clampf(prv_absf(y_center_t - 0.5f) / 0.34f, 0.0f, 1.0f);
    float horizon_weight = 1.0f - prv_clampf(prv_absf(y_center_t - horizon_center_t) / horizon_half_width, 0.0f, 1.0f);
    horizon_weight *= horizon_weight;
    float y_projection = ((float)y - cy) * vy;

    for (int16_t x = 0; x < width; x += step) {
      int16_t block_x = x / step;
      int16_t block_y = y / step;
      float projection = y_projection + (((float)x - cx) * vx);
      float raw_factor = prv_clampf((projection * projection_scale) + 0.5f, 0.0f, 1.0f);

      // Keep a visible tonal shift through the center third where the time sits.
      float smooth_factor = raw_factor * raw_factor * (3.0f - (2.0f * raw_factor));
      float center_bias = 0.06f +
                          (0.12f * phase_energy) +
                          (0.08f * daylight_strength * phase_energy) +
                          (0.06f * center_band * phase_energy);
      center_bias += 0.05f * event_moment;
      float factor = (raw_factor * (1.0f - center_bias)) + (smooth_factor * center_bias);

      if (center_band > 0.0f && phase_energy > 0.0f) {
        float center_boost = 1.0f + (0.16f * center_band * phase_energy * (0.6f + (0.4f * daylight_strength)));
        factor = ((factor - 0.5f) * center_boost) + 0.5f;
      }
      factor = prv_clampf(factor, 0.0f, 1.0f);

      if (daylight_strength > 0.0f) {
        float widened = ((factor - 0.5f) * (1.0f + (0.35f * daylight_strength * profile->gradient_widen_mult))) + 0.5f;
        factor = prv_clampf(widened, 0.0f, 1.0f);
      }

      if (horizon_weight > 0.0f) {
        float pull = prv_clampf(horizon_strength * horizon_weight, 0.0f, 0.22f);
        factor = (factor * (1.0f - pull)) + (horizon_target_t * pull);
      }

      int32_t coarse_x = x / (step * 6);
      int32_t coarse_y = y / (step * 6);
      float coarse_noise = prv_noise_2d(coarse_x, coarse_y, daily_seed);
      float detail_noise = prv_noise_2d((coarse_x * 2) + 7, (coarse_y * 2) - 3, daily_seed ^ 0x9E37u);
      float cloud_noise = (coarse_noise * 0.70f) + (detail_noise * 0.30f);
      float texture = cloud_noise * texture_strength * (0.85f + (0.15f * center_band));
      factor = prv_clampf(factor + texture, 0.0f, 1.0f);

      factor = prv_clampf(factor + prv_dither_offset(block_x, block_y, dither_strength), 0.0f, 1.0f);

      if (daylight_strength > 0.0f) {
        float sample_x = (float)x + ((float)step * 0.5f);
        float sample_y = (float)y + ((float)step * 0.5f);
        float dx = sample_x - bloom_x;
        float dy = sample_y - bloom_y;
        float dist_sq = (dx * dx) + (dy * dy);

        if (dist_sq < bloom_radius_sq) {
          float sun_intensity = 1.0f - (dist_sq / bloom_radius_sq);
          factor = prv_clampf(factor + (sun_intensity * sun_intensity * bloom_gain), 0.0f, 1.0f);
        }
      }

      GColor color = prv_dithered_color(factor, dither_strength * 2.0f, dither_val, 0.0f, palette);
      graphics_context_set_fill_color(ctx, color);
      graphics_fill_rect(ctx, GRect(x, y, step, step), 0, GCornerNone);
    }
  }"""

new_loop = """  float dither_val_inner = dither_val; // For syntax check placeholder
  float proj_step = (float)step * vx * projection_scale; // accumulation basis
  
  float daylight_widen = (1.0f + (0.35f * daylight_strength * profile->gradient_widen_mult));

  for (int16_t y = 0; y < height; y += step) {
    int16_t block_y = y / step;
    int32_t coarse_y = y / (step * 6);
    float y_center_t = prv_clampf(((float)y + ((float)step * 0.5f)) / (float)height, 0.0f, 1.0f);
    float center_band = 1.0f - prv_clampf(prv_absf(y_center_t - 0.5f) / 0.34f, 0.0f, 1.0f);
    float horizon_weight = 1.0f - prv_clampf(prv_absf(y_center_t - horizon_center_t) / horizon_half_width, 0.0f, 1.0f);
    horizon_weight *= horizon_weight;
    
    // Y-hoists
    float sample_y = (float)y + ((float)step * 0.5f);
    float dy = sample_y - bloom_y;
    float dy_sq = dy * dy;
    
    float y_projection = ((float)y - cy) * vy;

    float center_bias = 0.06f + (0.12f * phase_energy) + 
                        (0.08f * daylight_strength * phase_energy) + 
                        (0.06f * center_band * phase_energy) + 
                        (0.05f * event_moment);

    float center_boost = 1.0f;
    if (center_band > 0.0f && phase_energy > 0.0f) {
      center_boost = 1.0f + (0.16f * center_band * phase_energy * (0.6f + (0.4f * daylight_strength)));
    }

    float pull = 0.0f;
    if (horizon_weight > 0.0f) {
      pull = prv_clampf(horizon_strength * horizon_weight, 0.0f, 0.22f);
    }
    
    float texture_mult = texture_strength * (0.85f + (0.15f * center_band));
    
    float base_projection = (y_projection + ((float)(-cx) * vx)) * projection_scale;

    int32_t last_coarse_x = -1;
    float cloud_noise = 0.0f;
    
    for (int16_t x = 0; x < width; x += step) {
      int16_t block_x = x / step;
      
      // Loop accumulator for projection
      float projection_scaled = base_projection + (((float)x * vx) * projection_scale);
      float raw_factor = prv_clampf(projection_scaled + 0.5f, 0.0f, 1.0f);

      float smooth_factor = raw_factor * raw_factor * (3.0f - (2.0f * raw_factor));
      float factor = (raw_factor * (1.0f - center_bias)) + (smooth_factor * center_bias);

      if (center_boost > 1.0f) {
        factor = ((factor - 0.5f) * center_boost) + 0.5f;
      }
      factor = prv_clampf(factor, 0.0f, 1.0f);

      if (daylight_strength > 0.0f) {
        factor = prv_clampf(((factor - 0.5f) * daylight_widen) + 0.5f, 0.0f, 1.0f);
      }

      if (pull > 0.0f) {
        factor = (factor * (1.0f - pull)) + (horizon_target_t * pull);
      }

      // Noise memoization
      int32_t coarse_x = x / (step * 6);
      if (coarse_x != last_coarse_x) {
        last_coarse_x = coarse_x;
        float coarse_noise = prv_noise_2d(coarse_x, coarse_y, daily_seed);
        float detail_noise = prv_noise_2d((coarse_x * 2) + 7, (coarse_y * 2) - 3, daily_seed ^ 0x9E37u);
        cloud_noise = (coarse_noise * 0.70f) + (detail_noise * 0.30f);
      }
      
      factor = prv_clampf(factor + (cloud_noise * texture_mult), 0.0f, 1.0f);
      factor = prv_clampf(factor + prv_dither_offset(block_x, block_y, dither_strength), 0.0f, 1.0f);

      if (daylight_strength > 0.0f) {
        float sample_x = (float)x + ((float)step * 0.5f);
        float dx = sample_x - bloom_x;
        float dist_sq = (dx * dx) + dy_sq; // use hoisted dy_sq

        if (dist_sq < bloom_radius_sq) {
          float sun_intensity = 1.0f - (dist_sq / bloom_radius_sq);
          factor = prv_clampf(factor + (sun_intensity * sun_intensity * bloom_gain), 0.0f, 1.0f);
        }
      }

      GColor color = prv_dithered_color(factor, dither_strength * 2.0f, dither_val_inner, 0.0f, palette);
      graphics_context_set_fill_color(ctx, color);
      graphics_fill_rect(ctx, GRect(x, y, step, step), 0, GCornerNone);
    }
  }"""

if old_loop in text:
    print("Found loop successfully")
    # Quick fix for dither parameter which is actually hardcoded `dither_val` in old code. 
    # Just need to check the actual name.
    
    # Wait, `dither_val` is passed from the function. 
    new_loop = new_loop.replace('dither_val_inner', 'dither_val')

    with open('src/c/Sky.c', 'w') as f:
        f.write(text.replace(old_loop, new_loop))
else:
    print("Failed to find loop.")
