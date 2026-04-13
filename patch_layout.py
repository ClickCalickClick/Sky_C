import sys

with open('src/c/Sky.c', 'r') as f:
    text = f.read()

old_block = """\
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

  int16_t block_h = time_size.h;
  if (info_line_count > 0) {
    block_h += line_gap_main;
    block_h += (int16_t)(info_line_h * info_line_count);
    block_h += (int16_t)(line_gap_info * (info_line_count - 1));
  }

  int16_t available_h = bounds.size.h - (2 * top_bottom_pad);
  if (block_h > available_h) {
    line_gap_main = 1;
    line_gap_info = 1;
    block_h = time_size.h;
    if (info_line_count > 0) {
      block_h += line_gap_main;
      block_h += (int16_t)(info_line_h * info_line_count);
      block_h += (int16_t)(line_gap_info * (info_line_count - 1));
    }
  }

  int16_t start_y = top_bottom_pad;
  if (available_h > block_h) {
    start_y += (available_h - block_h) / 2;
  }

  int16_t y_cursor = start_y;
  int16_t time_center_y = y_cursor + (time_size.h / 2);
  TextStyle time_style = prv_pick_text_style_for_row(palette, bounds, now_ms, profile, time_center_y);
  prv_draw_styled_text(ctx, time_text, time_font, time_style,
                       side_pad, y_cursor, content_w, time_size.h + 2, GTextAlignmentCenter);
  y_cursor += time_size.h;

  if (info_line_count > 0) {
    y_cursor += line_gap_main;
  }

  if (show_location_line) {
    int16_t location_center_y = y_cursor + (info_line_h / 2);
    TextStyle location_style = prv_pick_text_style_for_row(palette, bounds, now_ms, profile, location_center_y);
    prv_draw_styled_text(ctx, location_text, info_font, location_style,
                         side_pad, y_cursor, content_w, info_line_h + 2, GTextAlignmentCenter);
    y_cursor += info_line_h;
  }

  if (show_location_line && (show_altitude_line || show_weather_line)) {
    y_cursor += line_gap_info;
  }

  if (show_altitude_line) {
    int16_t altitude_center_y = y_cursor + (info_line_h / 2);
    TextStyle altitude_style = prv_pick_text_style_for_row(palette, bounds, now_ms, profile, altitude_center_y);
    prv_draw_styled_text(ctx, altitude_text, info_font, altitude_style,
                         side_pad, y_cursor, content_w, info_line_h + 2, GTextAlignmentCenter);
    y_cursor += info_line_h;
  }

  if ((show_location_line || show_altitude_line) && show_weather_line) {
    y_cursor += line_gap_info;
  }

  if (show_weather_line) {
    int16_t weather_center_y = y_cursor + (info_line_h / 2);
    TextStyle weather_style = prv_pick_text_style_for_row(palette, bounds, now_ms, profile, weather_center_y);
    prv_draw_styled_text(ctx, weather_text, info_font, weather_style,
                         side_pad, y_cursor, content_w, info_line_h + 2, GTextAlignmentCenter);
    y_cursor += info_line_h;
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
"""

new_block = """\
  char date_text[32];
  date_text[0] = '\\0';
  strftime(date_text, sizeof(date_text), "%a, %b %e", time_info);

  char battery_text[16];
  battery_text[0] = '\\0';
  snprintf(battery_text, sizeof(battery_text), "Bat: %d%%", battery_state_service_peek().charge_percent);

  const char* available_texts[] = {
    "", // 0 = None
    location_text, // 1
    altitude_text, // 2
    weather_text, // 3
    solar_phase_text, // 4
    next_phase_text, // 5
    date_text, // 6
    battery_text // 7
  };

  const char *active_lines[6]; // Max 6 lines
  int16_t info_line_count = 0;

  int32_t slots[] = {
    s_state.footer_slot_1,
    s_state.footer_slot_2,
    s_state.footer_slot_3,
    s_state.footer_slot_4,
    s_state.footer_slot_5,
    s_state.footer_slot_6
  };

  for (int i = 0; i < 6; i++) {
    int32_t slot = slots[i];
    if (slot > 0 && slot <= 7) {
      const char *text = available_texts[slot];
      if (text && text[0] != '\\0') {
        active_lines[info_line_count++] = text;
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

  int16_t block_h = time_size.h;
  if (info_line_count > 0) {
    block_h += line_gap_main;
    block_h += (int16_t)(info_line_h * info_line_count);
    block_h += (int16_t)(line_gap_info * (info_line_count - 1));
  }

  int16_t available_h = bounds.size.h - (2 * top_bottom_pad);
  if (block_h > available_h) {
    line_gap_main = 1;
    line_gap_info = 1;
    block_h = time_size.h;
    if (info_line_count > 0) {
      block_h += line_gap_main;
      block_h += (int16_t)(info_line_h * info_line_count);
      block_h += (int16_t)(line_gap_info * (info_line_count - 1));
    }
  }

  int16_t start_y = top_bottom_pad;
  if (available_h > block_h) {
    start_y += (available_h - block_h) / 2;
  }
  start_y -= (bounds.size.h / 24); // optical adjustment for font padding

  int16_t y_cursor = start_y;
  int16_t time_center_y = y_cursor + (time_size.h / 2);
  TextStyle time_style = prv_pick_text_style_for_row(palette, bounds, now_ms, profile, time_center_y);
  prv_draw_styled_text(ctx, time_text, time_font, time_style,
                       side_pad, y_cursor, content_w, time_size.h + 2, GTextAlignmentCenter);
  y_cursor += time_size.h;

  if (info_line_count > 0) {
    y_cursor += line_gap_main;
  }

  for (int i = 0; i < info_line_count; i++) {
    int16_t center_y = y_cursor + (info_line_h / 2);
    TextStyle style = prv_pick_text_style_for_row(palette, bounds, now_ms, profile, center_y);
    prv_draw_styled_text(ctx, active_lines[i], info_font, style,
                         side_pad, y_cursor, content_w, info_line_h + 2, GTextAlignmentCenter);
    y_cursor += info_line_h;
    if (i < info_line_count - 1) {
      y_cursor += line_gap_info;
    }
  }
"""

if old_block in text:
    new_text = text.replace(old_block, new_block)
    with open('src/c/Sky.c', 'w') as f:
        f.write(new_text)
    print("PATCH SUCCESSFUL")
else:
    print("OLD BLOCK NOT FOUND")
