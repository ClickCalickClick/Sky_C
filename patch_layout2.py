import re

with open('patch_layout.py', 'r') as f:
    text_py = f.read()

# Append to it
with open('patch_layout.py', 'a') as f:
    f.write('''
NEW_LAYOUT = """
  static char s_time_text[8];
  static char s_location_text[48];
  static char s_altitude_text[32];
  static char s_weather_text[56];
  static char s_solar_phase_text[32];
  static char s_next_phase_text[48];
  static char s_date_text[32];
  static char s_battery_text[16];

  static const char *s_active_lines[6];
  static int16_t s_info_line_count = 0;
  static int16_t s_line_gap_main = 0;
  static int16_t s_line_gap_info = 0;
  static int16_t s_start_y = 0;
  static GFont s_time_font;
  static GFont s_info_font;
  static GSize s_time_size;
  static int16_t s_info_line_h = 0;

  static bool s_text_layout_dirty = true;
  static uint32_t s_last_time_size_mode = 0;
  static const RenderProfile *s_last_profile = NULL;

  if (s_text_layout_dirty || s_last_profile != profile) {
      s_text_layout_dirty = false;
      s_last_profile = profile;

      bool is_24h = clock_is_24h_style();
      if (s_state.time_format == 1) {
        is_24h = false;
      } else if (s_state.time_format == 2) {
        is_24h = true;
      }

      if (is_24h) {
        snprintf(s_time_text, sizeof(s_time_text), "%02d:%02d", time_info->tm_hour, time_info->tm_min);
      } else {
        int hours = time_info->tm_hour % 12;
        if (hours == 0) hours = 12;
        snprintf(s_time_text, sizeof(s_time_text), "%d:%02d", hours, time_info->tm_min);
      }

      int32_t time_size_mode = prv_time_size_mode_for_profile(profile);
      s_last_time_size_mode = time_size_mode;
      s_time_font = prv_time_font_for_mode(profile, time_size_mode);

      s_location_text[0] = '\\0';
      prv_resolve_city_name(s_location_text, sizeof(s_location_text));

      s_altitude_text[0] = '\\0';
      char alt_value[24];
      prv_format_altitude_x100(s_state.altitude_deg_x100, alt_value, sizeof(alt_value));
      snprintf(s_altitude_text, sizeof(s_altitude_text), "alt %s deg", alt_value);

      prv_format_weather_text(s_weather_text, sizeof(s_weather_text));

      s_solar_phase_text[0] = '\\0';
      if (s_state.current_solar_phase_id >= 0) {
        snprintf(s_solar_phase_text, sizeof(s_solar_phase_text), "%s", s_band_names[s_state.current_solar_phase_id]);
      }

      s_next_phase_text[0] = '\\0';
      if (s_state.next_solar_phase_epoch > 0) {
        int32_t diff = s_state.next_solar_phase_epoch - time(NULL);
        if (diff > 0) {
          int32_t diff_mins = diff / 60;
          int32_t h = diff_mins / 60;
          int32_t m = diff_mins % 60;
          if (h > 0) {
            snprintf(s_next_phase_text, sizeof(s_next_phase_text), "%dh %dm to %s", (int)h, (int)m, s_band_names[s_state.next_solar_phase_id]);
          } else {
            snprintf(s_next_phase_text, sizeof(s_next_phase_text), "%dm to %s", (int)m, s_band_names[s_state.next_solar_phase_id]);
          }
        }
      }

      s_date_text[0] = '\\0';
      strftime(s_date_text, sizeof(s_date_text), "%a, %b %e", time_info);

      s_battery_text[0] = '\\0';
      snprintf(s_battery_text, sizeof(s_battery_text), "Bat: %d%%", battery_state_service_peek().charge_percent);

      const char* available_texts[] = {
        "",
        s_date_text,
        s_location_text,
        s_weather_text,
        s_altitude_text,
        s_solar_phase_text,
        s_next_phase_text,
        s_battery_text
      };

      s_info_line_count = 0;
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
            s_active_lines[s_info_line_count++] = text;
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

      s_info_font = bounds.size.h < 200 ? s_info_font_small : s_info_font_large;
      s_info_line_h = prv_line_height_for_font(s_info_font);
      s_line_gap_main = prv_clamp_i16(bounds.size.h / 36, 2, 8);
      s_line_gap_info = prv_clamp_i16(bounds.size.h / 48, 1, 6);

      s_time_size = prv_text_size(s_time_text, s_time_font, content_w, bounds.size.h);

      int16_t block_h = s_time_size.h;
      if (s_info_line_count > 0) {
        block_h += s_line_gap_main;
        block_h += (int16_t)(s_info_line_h * s_info_line_count);
        block_h += (int16_t)(s_line_gap_info * (s_info_line_count - 1));
      }

      int16_t available_h = bounds.size.h - (2 * top_bottom_pad);
      if (block_h > available_h) {
        s_line_gap_main = 1;
        s_line_gap_info = 1;
        block_h = s_time_size.h;
        if (s_info_line_count > 0) {
          block_h += s_line_gap_main;
          block_h += (int16_t)(s_info_line_h * s_info_line_count);
          block_h += (int16_t)(s_line_gap_info * (s_info_line_count - 1));
        }
      }

      s_start_y = top_bottom_pad;
      if (available_h > block_h) {
        s_start_y += (available_h - block_h) / 2;
      }
      s_start_y -= (bounds.size.h / 24);
  }

  int16_t y_cursor = s_start_y;"""

    f.write('text = text.replace(OLD_LAYOUT, NEW_LAYOUT)\n')

    # Also rename the variables downstream in prv_draw_face
    f.write('text = text.replace("prv_draw_styled_text(ctx, time_text, time_font, time_style,", "prv_draw_styled_text(ctx, s_time_text, s_time_font, time_style,")\n')
    f.write('text = text.replace("time_center_y = y_cursor + (time_size.h / 2)", "time_center_y = y_cursor + (s_time_size.h / 2)")\n')
    f.write('text = text.replace("y_cursor += time_size.h + line_gap_main;", "y_cursor += s_time_size.h + s_line_gap_main;")\n')

    f.write('text = text.replace("active_lines[i]", "s_active_lines[i]")\n')
    f.write('text = text.replace("info_line_count>", "s_info_line_count>")\n')
    f.write('text = text.replace("i < info_line_count", "i < s_info_line_count")\n')
    f.write('text = text.replace("info_font", "s_info_font")\n')
    f.write('text = text.replace("info_line_h + line_gap_info", "s_info_line_h + s_line_gap_info")\n')
    f.write('text = text.replace("s_s_info_font", "s_info_font")\n')
    f.write('text = text.replace("s_info_line_count>", "s_info_line_count >")\n')

    # We also need to add a hook in prv_on_minute_tick to set s_text_layout_dirty = true
    # wait, s_text_layout_dirty is a static local within prv_draw_face? If it's static inside prv_draw_face it's only scoped there!
    # I should declare `s_text_layout_dirty` as a global variable.
    # Ah, in C, `static bool s_text_layout_dirty = true;` inside `prv_draw_face` makes it keep its value, but I need to modify it from OUTSIDE. 
    # That means I need to declare it at the file level! I will extract `bool s_text_layout_dirty = true;` to file level instead using python.
''')
