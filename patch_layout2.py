import sys

with open('src/c/Sky.c', 'r') as f:
    text = f.read()

with open('src/Sky-draw.txt', 'r') as f:
    old_block = f.read()

new_block = """\
  int hours = time_info->tm_hour % 12;
  if (hours == 0) {
    hours = 12;
  }

  char time_text[8];
  snprintf(time_text, sizeof(time_text), "%d:%02d", hours, time_info->tm_min);

  int32_t time_size_mode = prv_time_size_mode_for_profile(profile);
  GFont time_font = prv_time_font_for_mode(profile, time_size_mode);

  char location_text[48];
  location_text[0] = '\\0';
  prv_resolve_city_name(location_text, sizeof(location_text));

  char altitude_text[32];
  altitude_text[0] = '\\0';
  char alt_value[24];
  prv_format_altitude_x100(s_state.altitude_deg_x100, alt_value, sizeof(alt_value));
  snprintf(altitude_text, sizeof(altitude_text), "alt %s deg", alt_value);

  char weather_text[56];
  prv_format_weather_text(weather_text, sizeof(weather_text));

  char solar_phase_text[32];
  solar_phase_text[0] = '\\0';
  snprintf(solar_phase_text, sizeof(solar_phase_text), "%s", s_band_names[s_state.current_solar_phase_id]);

  char next_phase_text[48];
  next_phase_text[0] = '\\0';
  if (s_state.next_solar_phase_epoch > 0) {
    int32_t diff = s_state.next_solar_phase_epoch - time(NULL);
    if (diff > 0 || s_state.phase_refresh_requested) {
      if (diff >= 60) {
        int32_t diff_mins = diff / 60;
        int32_t h = diff_mins / 60;
        int32_t m = diff_mins % 60;
        if (h > 0) {
          snprintf(next_phase_text, sizeof(next_phase_text), "%dh %dm to %s", (int)h, (int)m, s_band_names[s_state.next_solar_phase_id]);
        } else {
          snprintf(next_phase_text, sizeof(next_phase_text), "%dm to %s", (int)m, s_band_names[s_state.next_solar_phase_id]);
        }
      } else {
        snprintf(next_phase_text, sizeof(next_phase_text), "Transitioning...");
      }
    }
  }

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
    print("OLD BLOCK NOT FOUND, trying whitespace normalization")
    old_mod = ' '.join(old_block.split())
    text_mod = ' '.join(text.split())
    if old_mod in text_mod:
        print("FOUND BUT WHITESPACE DIFFERED")
