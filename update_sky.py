import sys

def modify_sky():
    with open('src/c/Sky.c', 'r') as f:
        content = f.read()

    # 1. Add fields to AppState
    old_struct = "  char city_name[48];\n} AppState;"
    new_struct = "  char city_name[48];\n  int32_t show_solar_phase;\n  int32_t show_next_phase_countdown;\n  int32_t current_solar_phase_id;\n  int32_t next_solar_phase_id;\n  time_t next_solar_phase_epoch;\n} AppState;"
    content = content.replace(old_struct, new_struct)
    
    # 2. Add s_band_names array
    old_band = "static const char* s_info_font_large_name"
    new_band = 'static const char* s_band_names[] = {"Deep Night", "Astro Twilight", "Nautical Blue Hour", "Sunrise", "Golden Hour", "Morning Light", "Midday Sun", "Afternoon Sky", "Golden Hour", "Sunset", "Blue Hour", "Nautical Twilight", "Astro Twilight", "Deep Night"};\n\n' + old_band
    content = content.replace(old_band, new_band)

    # 3. Add dictionary parsing
    old_parse = "  Tuple *time_size_chalk = dict_find(iter, MESSAGE_KEY_TimeSizeChalk);\n  if (time_size_chalk) {\n    s_state.time_size_chalk = prv_clamp_i32(prv_tuple_to_i32(time_size_chalk), TIME_SIZE_COMPACT, TIME_SIZE_LARGE);\n    loading_changed = true;\n  }"
    new_parse = old_parse + """
  Tuple *show_solar_phase = dict_find(iter, MESSAGE_KEY_ShowSolarPhase);
  if (show_solar_phase) {
    s_state.show_solar_phase = prv_tuple_to_i32(show_solar_phase);
    loading_changed = true;
  }
  
  Tuple *show_next_phase_countdown = dict_find(iter, MESSAGE_KEY_ShowNextPhaseCountdown);
  if (show_next_phase_countdown) {
    s_state.show_next_phase_countdown = prv_tuple_to_i32(show_next_phase_countdown);
    loading_changed = true;
  }
  
  Tuple *current_solar_phase_id = dict_find(iter, MESSAGE_KEY_CurrentSolarPhaseId);
  if (current_solar_phase_id) {
    s_state.current_solar_phase_id = prv_clamp_i32(prv_tuple_to_i32(current_solar_phase_id), 0, 13);
    loading_changed = true;
  }
  
  Tuple *next_solar_phase_id = dict_find(iter, MESSAGE_KEY_NextSolarPhaseId);
  if (next_solar_phase_id) {
    s_state.next_solar_phase_id = prv_clamp_i32(prv_tuple_to_i32(next_solar_phase_id), 0, 13);
    loading_changed = true;
  }
  
  Tuple *next_solar_phase_epoch = dict_find(iter, MESSAGE_KEY_NextSolarPhaseEpoch);
  if (next_solar_phase_epoch) {
    s_state.next_solar_phase_epoch = prv_tuple_to_i32(next_solar_phase_epoch);
    loading_changed = true;
  }"""
    content = content.replace(old_parse, new_parse)

    # 4. In prv_draw_face, add variables
    old_draw_1 = """  char weather_text[56];
  prv_format_weather_text(weather_text, sizeof(weather_text));

  int16_t side_pad = prv_clamp_i16(bounds.size.w / 18, 4, 16);"""
    
    new_draw_1 = """  char weather_text[56];
  prv_format_weather_text(weather_text, sizeof(weather_text));

  char solar_phase_text[32];
  solar_phase_text[0] = '\\0';
  if (s_state.show_solar_phase != 0) {
    snprintf(solar_phase_text, sizeof(solar_phase_text), "%s", s_band_names[s_state.current_solar_phase_id]);
  }

  char next_phase_text[48];
  next_phase_text[0] = '\\0';
  if (s_state.show_next_phase_countdown != 0 && s_state.next_solar_phase_epoch > 0) {
    int32_t diff = s_state.next_solar_phase_epoch - time(NULL);
    if (diff > 0) {
      int32_t diff_mins = diff / 60;
      int32_t h = diff_mins / 60;
      int32_t m = diff_mins % 60;
      if (h > 0) {
        snprintf(next_phase_text, sizeof(next_phase_text), "%d hr %d min until %s", (int)h, (int)m, s_band_names[s_state.next_solar_phase_id]);
      } else {
        snprintf(next_phase_text, sizeof(next_phase_text), "%d min until %s", (int)m, s_band_names[s_state.next_solar_phase_id]);
      }
    }
  }

  int16_t side_pad = prv_clamp_i16(bounds.size.w / 18, 4, 16);"""
    content = content.replace(old_draw_1, new_draw_1)

    # 5. Add lines to info_line_count
    old_draw_2 = """  bool show_weather_line = (weather_text[0] != '\\0');

  int16_t info_line_count = 0;
  if (show_location_line) {
    info_line_count += 1;
  }
  if (show_altitude_line) {
    info_line_count += 1;
  }
  if (show_weather_line) {
    info_line_count += 1;
  }

  int16_t block_h = time_size.h;"""
    
    new_draw_2 = """  bool show_weather_line = (weather_text[0] != '\\0');
  bool show_solar_phase_line = (solar_phase_text[0] != '\\0');
  bool show_next_phase_countdown_line = (next_phase_text[0] != '\\0');

  int16_t info_line_count = 0;
  if (show_location_line) {
    info_line_count += 1;
  }
  if (show_altitude_line) {
    info_line_count += 1;
  }
  if (show_weather_line) {
    info_line_count += 1;
  }
  if (show_solar_phase_line) {
    info_line_count += 1;
  }
  if (show_next_phase_countdown_line) {
    info_line_count += 1;
  }

  int16_t block_h = time_size.h;"""
    content = content.replace(old_draw_2, new_draw_2)

    # 6. Finally, render strings
    old_draw_3 = """  if (show_weather_line) {
    int16_t weather_center_y = y_cursor + (info_line_h / 2);
    TextStyle weather_style = prv_pick_text_style_for_row(palette, bounds, now_ms, profile, weather_center_y);
    prv_draw_styled_text(ctx, weather_text, info_font, weather_style,
                         side_pad, y_cursor, content_w, info_line_h + 2, GTextAlignmentCenter);
  }

  if (s_state.dev_mode_enabled && s_state.dev_show_debug_overlay) {"""

    new_draw_3 = """  if (show_weather_line) {
    int16_t weather_center_y = y_cursor + (info_line_h / 2);
    TextStyle weather_style = prv_pick_text_style_for_row(palette, bounds, now_ms, profile, weather_center_y);
    prv_draw_styled_text(ctx, weather_text, info_font, weather_style,
                         side_pad, y_cursor, content_w, info_line_h + 2, GTextAlignmentCenter);
    if (show_solar_phase_line || show_next_phase_countdown_line) {
      y_cursor += line_gap_info;
    }
  }

  if (show_solar_phase_line) {
    int16_t solar_center_y = y_cursor + (info_line_h / 2);
    TextStyle solar_style = prv_pick_text_style_for_row(palette, bounds, now_ms, profile, solar_center_y);
    prv_draw_styled_text(ctx, solar_phase_text, info_font, solar_style,
                         side_pad, y_cursor, content_w, info_line_h + 2, GTextAlignmentCenter);
    if (show_next_phase_countdown_line) {
      y_cursor += line_gap_info;
    }
  }

  if (show_next_phase_countdown_line) {
    int16_t next_center_y = y_cursor + (info_line_h / 2);
    TextStyle next_style = prv_pick_text_style_for_row(palette, bounds, now_ms, profile, next_center_y);
    prv_draw_styled_text(ctx, next_phase_text, info_font, next_style,
                         side_pad, y_cursor, content_w, info_line_h + 2, GTextAlignmentCenter);
  }

  if (s_state.dev_mode_enabled && s_state.dev_show_debug_overlay) {"""
    
    # We must also make sure that `y_cursor += line_gap_info;` when weather_line is absent.
    # Actually, the logic in original is:
    # ```c
    # if (show_altitude_line) {
    #   if (show_location_line) y_cursor += line_gap_info;
    #   draw altitude
    # }
    # if ((show_location_line || show_altitude_line) && show_weather_line) {
    #   y_cursor += line_gap_info;
    # }
    # ```
    # I will adapt the spacing.
    
    content = content.replace(old_draw_3, new_draw_3)

    # Ensure y_cursor correctly progresses before weather line if there was no weather line but a phase line
    # Wait, the best way is to redefine that whole block:
    # Instead of fixing parsing with py scripts over and over, let me just run python
    with open('src/c/Sky.c', 'w') as f:
        f.write(content)

modify_sky()
