import re

with open('src/c/Sky.c', 'r') as f:
    text = f.read()

# 1. Append to AppState
text = text.replace(
    '  char city_name[48];\n} AppState;',
    '  char city_name[48];\n  int32_t show_solar_phase;\n  int32_t show_next_phase_countdown;\n  int32_t current_solar_phase_id;\n  int32_t next_solar_phase_id;\n  int32_t next_solar_phase_epoch;\n} AppState;'
)

# 2. Add s_band_names near the top
text = text.replace(
    'static GColor s_text_color;',
    'static GColor s_text_color;\nstatic const char* s_band_names[] = {"Deep Night", "Astro Twilight", "Nautical Blue Hour", "Sunrise", "Golden Hour", "Morning Light", "Midday Sun", "Afternoon Sky", "Golden Hour", "Sunset", "Blue Hour", "Nautical Twilight", "Astro Twilight", "Deep Night"};'
)

# 3. Add to prv_on_message_received
text = text.replace(
    '  Tuple *time_size_chalk = dict_find(iter, MESSAGE_KEY_TimeSizeChalk);\n  if (time_size_chalk) {\n    s_state.time_size_chalk = prv_clamp_i32(prv_tuple_to_i32(time_size_chalk), TIME_SIZE_COMPACT, TIME_SIZE_LARGE);\n    loading_changed = true;\n  }',
    '''  Tuple *time_size_chalk = dict_find(iter, MESSAGE_KEY_TimeSizeChalk);
  if (time_size_chalk) {
    s_state.time_size_chalk = prv_clamp_i32(prv_tuple_to_i32(time_size_chalk), TIME_SIZE_COMPACT, TIME_SIZE_LARGE);
    loading_changed = true;
  }

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
  }'''
)

# 4. Update drawing logic, replace from `char weather_text[56];` up to `int16_t block_h = time_size.h;`
old_draw = '''  char weather_text[56];
  prv_format_weather_text(weather_text, sizeof(weather_text));

  int16_t side_pad = prv_clamp_i16(bounds.size.w / 18, 4, 16);
  int16_t top_bottom_pad = prv_clamp_i16(bounds.size.h / 12, 8, 26);
  int16_t content_w = bounds.size.w - (2 * side_pad);
  if (content_w < 20) {
    content_w = bounds.size.w;
    side_pad = 0;
  }

  GFont info_font = bounds.size.h < 200 ? s_info_font_small : s_info_font_large;
  int16_t info_line_h = prv_line_height_for_font(info_font);
  int16_t line_gap_main = prv_clamp_i16(bounds.size.h / 36, 2, 8);
  int16_t line_gap_info = prv_clamp_i16(bounds.size.h / 48, 1, 6);

  GSize time_size = prv_text_size(time_text, time_font, content_w, bounds.size.h);
  bool show_location_line = (location_text[0] != '\\0');
  bool show_altitude_line = (altitude_text[0] != '\\0');
  bool show_weather_line = (weather_text[0] != '\\0');

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

  int16_t block_h = time_size.h;'''

new_draw = '''  char weather_text[56];
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

  int16_t side_pad = prv_clamp_i16(bounds.size.w / 18, 4, 16);
  int16_t top_bottom_pad = prv_clamp_i16(bounds.size.h / 12, 8, 26);
  int16_t content_w = bounds.size.w - (2 * side_pad);
  if (content_w < 20) {
    content_w = bounds.size.w;
    side_pad = 0;
  }

  GFont info_font = bounds.size.h < 200 ? s_info_font_small : s_info_font_large;
  int16_t info_line_h = prv_line_height_for_font(info_font);
  int16_t line_gap_main = prv_clamp_i16(bounds.size.h / 36, 2, 8);
  int16_t line_gap_info = prv_clamp_i16(bounds.size.h / 48, 1, 6);

  GSize time_size = prv_text_size(time_text, time_font, content_w, bounds.size.h);
  bool show_location_line = (location_text[0] != '\\0');
  bool show_altitude_line = (altitude_text[0] != '\\0');
  bool show_weather_line = (weather_text[0] != '\\0');
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

  int16_t block_h = time_size.h;'''

if old_draw not in text:
    print("Warning: old_draw logic not found")
else:
    text = text.replace(old_draw, new_draw)

# 5. Fix text draw calls
old_draw_lines = '''  if (show_weather_line) {
    int16_t weather_center_y = y_cursor + (info_line_h / 2);
    TextStyle weather_style = prv_pick_text_style_for_row(palette, bounds, now_ms, profile, weather_center_y);
    prv_draw_styled_text(ctx, weather_text, info_font, weather_style,
                         side_pad, y_cursor, content_w, info_line_h + 2, GTextAlignmentCenter);
  }

  if (s_state.dev_mode_enabled && s_state.dev_show_debug_overlay) {'''

new_draw_lines = '''  if (show_weather_line) {
    int16_t weather_center_y = y_cursor + (info_line_h / 2);
    TextStyle weather_style = prv_pick_text_style_for_row(palette, bounds, now_ms, profile, weather_center_y);
    prv_draw_styled_text(ctx, weather_text, info_font, weather_style,
                         side_pad, y_cursor, content_w, info_line_h + 2, GTextAlignmentCenter);
  }

  if (show_weather_line && (show_solar_phase_line || show_next_phase_countdown_line)) {
    y_cursor += line_gap_info;
  } else if (!show_weather_line && (show_location_line || show_altitude_line) && (show_solar_phase_line || show_next_phase_countdown_line)) {
    y_cursor += line_gap_info;
  }

  if (show_solar_phase_line) {
    int16_t solar_center_y = y_cursor + (info_line_h / 2);
    TextStyle solar_style = prv_pick_text_style_for_row(palette, bounds, now_ms, profile, solar_center_y);
    prv_draw_styled_text(ctx, solar_phase_text, info_font, solar_style,
                         side_pad, y_cursor, content_w, info_line_h + 2, GTextAlignmentCenter);
    y_cursor += info_line_h;
  }

  if (show_solar_phase_line && show_next_phase_countdown_line) {
    y_cursor += line_gap_info;
  }

  if (show_next_phase_countdown_line) {
    int16_t next_center_y = y_cursor + (info_line_h / 2);
    TextStyle next_style = prv_pick_text_style_for_row(palette, bounds, now_ms, profile, next_center_y);
    prv_draw_styled_text(ctx, next_phase_text, info_font, next_style,
                         side_pad, y_cursor, content_w, info_line_h + 2, GTextAlignmentCenter);
  }

  if (s_state.dev_mode_enabled && s_state.dev_show_debug_overlay) {'''

if old_draw_lines not in text:
    print("Warning: old_draw_lines logic not found")
else:
    text = text.replace(old_draw_lines, new_draw_lines)

with open('src/c/Sky.c', 'w') as f:
    f.write(text)

