#include <pebble.h>

#define SOURCE_PHONE 0
#define SOURCE_MANUAL 1
#define SOURCE_CHICAGO 2
#define SOURCE_CACHED 3

#define STATUS_STARTING 0
#define STATUS_GRABBING_LOCATION 1
#define STATUS_CALCULATING_SUN 2
#define STATUS_RESOLVING_CITY 3
#define STATUS_SENDING_PAYLOAD 4
#define STATUS_READY 6

#define TEXT_MODE_AUTO 0
#define TEXT_MODE_WHITE 1
#define TEXT_MODE_BLACK 2
#define TEXT_MODE_BLACK_GLOW 3

#define LOADING_TIMEOUT_MS 8000
#define LOADING_STALE_HINT_MS 3500
#define LOADING_STILL_WORKING_MS 6000
#define LOADING_TIMER_INTERVAL_MS 125
#define LOADING_TRANSITION_HOLD_MS 180

#define GRADIENT_STEP 2
#define DITHER_STRENGTH 0.03f
#define TEXT_BRIGHTNESS_THRESHOLD 145

typedef struct {
  int16_t r;
  int16_t g;
  int16_t b;
} Rgb;

typedef struct {
  int8_t altitude;
  Rgb top;
  Rgb bottom;
} AtmosphereBand;

typedef struct {
  Rgb top;
  Rgb bottom;
} Palette;

typedef struct {
  GColor color;
  bool glow;
} TextStyle;

typedef struct {
  int32_t source_code;
  int32_t latitude_e6;
  int32_t longitude_e6;
  int32_t azimuth_deg_x100;
  int32_t altitude_deg_x100;
  int32_t gradient_angle_deg_x100;
  int32_t computed_at_epoch;
  int32_t text_override_mode;
  int32_t custom_location_enabled;
  int32_t custom_latitude_e6;
  int32_t custom_longitude_e6;
  int32_t debug_benchmark;
  int32_t status_code;
  int32_t loading_progress;
  int32_t loading_progress_target;
  uint32_t last_reload_face_token;
  uint32_t refresh_counter;
  uint32_t loading_started_ms;
  uint32_t loading_status_started_ms;
  uint32_t launch_transition_deadline_ms;
  bool launch_done;
  bool has_payload;
  bool had_payload_once;
  bool bt_connected;
  bool dev_mode_enabled;
  bool dev_sweep_enabled;
  bool dev_show_debug_overlay;
  uint8_t loading_hint_mode;
  char city_name[48];
} AppState;

static const AtmosphereBand s_atmosphere[] = {
  { -18, {8, 8, 26}, {40, 54, 110} },
  { -6, {26, 23, 66}, {58, 41, 108} },
  { -2, {96, 55, 118}, {196, 70, 130} },
  { 2, {238, 116, 60}, {255, 113, 114} },
  { 10, {255, 189, 98}, {255, 151, 80} },
  { 20, {118, 190, 255}, {187, 226, 255} },
};

static Window *s_window;
static Layer *s_canvas_layer;
static AppTimer *s_loading_timer;

static GFont s_time_font;
static GFont s_info_font_large;
static GFont s_info_font_small;

static AppState s_state = {
  .source_code = SOURCE_CHICAGO,
  .latitude_e6 = 41878100,
  .longitude_e6 = -87629800,
  .azimuth_deg_x100 = 18000,
  .altitude_deg_x100 = -200,
  .gradient_angle_deg_x100 = 0,
  .computed_at_epoch = 0,
  .text_override_mode = TEXT_MODE_AUTO,
  .status_code = STATUS_STARTING,
  .loading_progress = 0,
  .loading_progress_target = 0,
  .launch_done = false,
  .has_payload = false,
  .had_payload_once = false,
  .loading_hint_mode = 0,
  .city_name = "Chicago",
};

static int16_t prv_clamp_i16(int32_t value, int16_t min, int16_t max) {
  if (value < min) {
    return min;
  }
  if (value > max) {
    return max;
  }
  return (int16_t)value;
}

static int32_t prv_clamp_i32(int32_t value, int32_t min, int32_t max) {
  if (value < min) {
    return min;
  }
  if (value > max) {
    return max;
  }
  return value;
}

static int32_t prv_round_i32(float value) {
  return (int32_t)(value >= 0.0f ? value + 0.5f : value - 0.5f);
}

static float prv_absf(float value) {
  return value < 0.0f ? -value : value;
}

static int32_t prv_tuple_to_i32(const Tuple *tuple) {
  if (!tuple) {
    return 0;
  }

  if (tuple->length == 1) {
    return (tuple->type == TUPLE_INT) ? tuple->value->int8 : tuple->value->uint8;
  }
  if (tuple->length == 2) {
    return (tuple->type == TUPLE_INT) ? tuple->value->int16 : tuple->value->uint16;
  }
  return (tuple->type == TUPLE_INT) ? tuple->value->int32 : (int32_t)tuple->value->uint32;
}

static float prv_clampf(float value, float min, float max) {
  if (value < min) {
    return min;
  }
  if (value > max) {
    return max;
  }
  return value;
}

static uint32_t prv_now_ms(void) {
  time_t sec;
  uint16_t ms;
  time_ms(&sec, &ms);
  return (uint32_t)(sec * 1000 + ms);
}

static int16_t prv_lerp_i16(int16_t start, int16_t end, float factor) {
  return (int16_t)prv_round_i32((float)start + ((float)(end - start) * factor));
}

static Rgb prv_interpolate_rgb(Rgb a, Rgb b, float t) {
  Rgb out;
  out.r = prv_lerp_i16(a.r, b.r, t);
  out.g = prv_lerp_i16(a.g, b.g, t);
  out.b = prv_lerp_i16(a.b, b.b, t);
  return out;
}

static void prv_widen_pair(int16_t a, int16_t b, float scale, int16_t *out_a, int16_t *out_b) {
  float mid = ((float)a + (float)b) * 0.5f;
  *out_a = prv_clamp_i16(prv_round_i32(mid + (((float)a - mid) * scale)), 0, 255);
  *out_b = prv_clamp_i16(prv_round_i32(mid + (((float)b - mid) * scale)), 0, 255);
}

static Palette prv_widen_palette_contrast(Palette base, float scale) {
  Palette out = base;
  prv_widen_pair(base.top.r, base.bottom.r, scale, &out.top.r, &out.bottom.r);
  prv_widen_pair(base.top.g, base.bottom.g, scale, &out.top.g, &out.bottom.g);
  prv_widen_pair(base.top.b, base.bottom.b, scale, &out.top.b, &out.bottom.b);
  return out;
}

static Palette prv_palette_for_altitude(float altitude_deg) {
  Palette out;
  const int band_count = (int)(sizeof(s_atmosphere) / sizeof(s_atmosphere[0]));

  if (altitude_deg <= s_atmosphere[0].altitude) {
    out.top = s_atmosphere[0].top;
    out.bottom = s_atmosphere[0].bottom;
    return out;
  }

  const AtmosphereBand *max_band = &s_atmosphere[band_count - 1];
  if (altitude_deg >= max_band->altitude) {
    float daylight_strength = prv_clampf((altitude_deg - (float)max_band->altitude) / 35.0f, 0.0f, 1.0f);
    float contrast_scale = 1.0f + (daylight_strength * 0.45f);
    out.top = max_band->top;
    out.bottom = max_band->bottom;
    return prv_widen_palette_contrast(out, contrast_scale);
  }

  for (int i = 0; i < band_count - 1; i++) {
    const AtmosphereBand *low = &s_atmosphere[i];
    const AtmosphereBand *high = &s_atmosphere[i + 1];
    if (altitude_deg >= low->altitude && altitude_deg <= high->altitude) {
      float range = (float)(high->altitude - low->altitude);
      float t = (altitude_deg - (float)low->altitude) / (range <= 0.0f ? 1.0f : range);
      out.top = prv_interpolate_rgb(low->top, high->top, t);
      out.bottom = prv_interpolate_rgb(low->bottom, high->bottom, t);
      return out;
    }
  }

  out.top = s_atmosphere[0].top;
  out.bottom = s_atmosphere[0].bottom;
  return out;
}

static float prv_dither_offset(int16_t block_x, int16_t block_y, float amount) {
  int16_t pattern = (int16_t)(((block_x * 13) + (block_y * 17)) & 3);
  return (((float)pattern - 1.5f) / 1.5f) * amount;
}

static GColor prv_make_color_rgb(Rgb rgb) {
#ifdef PBL_COLOR
  return GColorFromRGB((uint8_t)prv_clamp_i16(rgb.r, 0, 255),
                       (uint8_t)prv_clamp_i16(rgb.g, 0, 255),
                       (uint8_t)prv_clamp_i16(rgb.b, 0, 255));
#else
  int32_t brightness = (rgb.r * 299 + rgb.g * 587 + rgb.b * 114) / 1000;
  return brightness > 127 ? GColorWhite : GColorBlack;
#endif
}

static bool prv_should_use_dark_foreground(Rgb sample_rgb) {
  if (s_state.text_override_mode == TEXT_MODE_WHITE) {
    return false;
  }
  if (s_state.text_override_mode == TEXT_MODE_BLACK || s_state.text_override_mode == TEXT_MODE_BLACK_GLOW) {
    return true;
  }
  int32_t brightness = (sample_rgb.r * 299 + sample_rgb.g * 587 + sample_rgb.b * 114) / 1000;
  return brightness > TEXT_BRIGHTNESS_THRESHOLD;
}

static TextStyle prv_pick_text_style(Rgb sample_rgb) {
  TextStyle style;
  if (s_state.text_override_mode == TEXT_MODE_BLACK_GLOW) {
    style.color = GColorBlack;
    style.glow = true;
    return style;
  }

  style.color = prv_should_use_dark_foreground(sample_rgb) ? GColorBlack : GColorWhite;
  style.glow = false;
  return style;
}

static GSize prv_text_size(const char *text, GFont font, int16_t width, int16_t height) {
  return graphics_text_layout_get_content_size(text, font, GRect(0, 0, width, height),
                                               GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
}

static void prv_draw_styled_text(GContext *ctx, const char *text, GFont font, TextStyle style,
                                 int16_t x, int16_t y, int16_t width, int16_t height) {
  GRect rect = GRect(x, y, width, height);
  if (style.glow) {
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, text, font, GRect(rect.origin.x - 1, rect.origin.y, rect.size.w, rect.size.h),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    graphics_draw_text(ctx, text, font, GRect(rect.origin.x + 1, rect.origin.y, rect.size.w, rect.size.h),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    graphics_draw_text(ctx, text, font, GRect(rect.origin.x, rect.origin.y - 1, rect.size.w, rect.size.h),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    graphics_draw_text(ctx, text, font, GRect(rect.origin.x, rect.origin.y + 1, rect.size.w, rect.size.h),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }

  graphics_context_set_text_color(ctx, style.color);
  graphics_draw_text(ctx, text, font, rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static int32_t prv_loading_floor_for_code(int32_t code) {
  if (code == STATUS_GRABBING_LOCATION) return 10;
  if (code == STATUS_CALCULATING_SUN) return 42;
  if (code == STATUS_RESOLVING_CITY) return 58;
  if (code == STATUS_SENDING_PAYLOAD) return 86;
  if (code == STATUS_READY) return 100;
  return 2;
}

static int32_t prv_loading_ceiling_for_code(int32_t code) {
  if (code == STATUS_GRABBING_LOCATION) return 45;
  if (code == STATUS_CALCULATING_SUN) return 65;
  if (code == STATUS_RESOLVING_CITY) return 88;
  if (code == STATUS_SENDING_PAYLOAD) return 95;
  if (code == STATUS_READY) return 100;
  return 25;
}

static const char *prv_status_text_from_code(int32_t code) {
  if (code == STATUS_STARTING) return "Starting";
  if (code == STATUS_GRABBING_LOCATION) return "Grabbing location";
  if (code == STATUS_CALCULATING_SUN) return "Calculating sun";
  if (code == STATUS_RESOLVING_CITY) return "Resolving city";
  if (code == STATUS_SENDING_PAYLOAD) return "Sending payload";
  if (code == STATUS_READY) return "Ready";
  return "Starting";
}

static const char *prv_loading_status_line(void) {
  if (s_state.loading_hint_mode == 1) {
    return "Waiting for phone GPS";
  }
  if (s_state.loading_hint_mode == 2) {
    return "Still loading";
  }
  return prv_status_text_from_code(s_state.status_code);
}

static void prv_set_loading_status(int32_t code) {
  int32_t clamped = prv_clamp_i32(code, STATUS_STARTING, STATUS_READY);
  if (clamped != s_state.status_code) {
    s_state.status_code = clamped;
    s_state.loading_status_started_ms = prv_now_ms();
  }
}

static void prv_set_loading_progress_target(int32_t percent) {
  int32_t clamped = prv_clamp_i32(percent, 0, 100);
  if (clamped > s_state.loading_progress_target) {
    s_state.loading_progress_target = clamped;
  }
}

static bool prv_advance_loading_progress(uint32_t now_ms) {
  int32_t ceiling = prv_loading_ceiling_for_code(s_state.status_code);
  int32_t floor = prv_loading_floor_for_code(s_state.status_code);
  int32_t elapsed_in_stage = (int32_t)(now_ms - s_state.loading_status_started_ms);
  int32_t idle_ramp_target = floor + (elapsed_in_stage / 320);
  if (idle_ramp_target > ceiling) {
    idle_ramp_target = ceiling;
  }
  int32_t desired = s_state.loading_progress_target;
  if (desired > ceiling) {
    desired = ceiling;
  }
  if (desired < idle_ramp_target) {
    desired = idle_ramp_target;
  }

  if (desired <= s_state.loading_progress) {
    return false;
  }

  int32_t delta = desired - s_state.loading_progress;
  int32_t step = delta > 10 ? 3 : 1;
  s_state.loading_progress = desired < (s_state.loading_progress + step) ? desired : (s_state.loading_progress + step);
  return true;
}

static void prv_start_loading_timer(void);

static void prv_begin_loading(void) {
  s_state.launch_done = false;
  s_state.has_payload = false;
  s_state.loading_started_ms = prv_now_ms();
  s_state.loading_status_started_ms = s_state.loading_started_ms;
  s_state.status_code = STATUS_STARTING;
  s_state.loading_progress = 0;
  s_state.loading_progress_target = 0;
  s_state.loading_hint_mode = 0;
  s_state.launch_transition_deadline_ms = 0;
  prv_start_loading_timer();
}

static void prv_format_altitude_x100(int32_t altitude_x100, char *out, size_t len) {
  int32_t abs_value = altitude_x100 < 0 ? -altitude_x100 : altitude_x100;
  int32_t whole = abs_value / 100;
  int32_t tenth = (abs_value % 100 + 5) / 10;
  if (tenth == 10) {
    whole += 1;
    tenth = 0;
  }
  snprintf(out, len, "%s%ld.%ld", altitude_x100 < 0 ? "-" : "", (long)whole, (long)tenth);
}

static void prv_format_azimuth_x100(int32_t azimuth_x100, char *out, size_t len) {
  int32_t normalized = azimuth_x100 % 36000;
  if (normalized < 0) {
    normalized += 36000;
  }

  int32_t whole = normalized / 100;
  int32_t tenth = (normalized % 100 + 5) / 10;
  if (tenth == 10) {
    whole = (whole + 1) % 360;
    tenth = 0;
  }
  snprintf(out, len, "%ld.%ld", (long)whole, (long)tenth);
}

static void prv_draw_solar_gradient(GContext *ctx, GRect bounds, Palette palette) {
  int16_t width = bounds.size.w;
  int16_t height = bounds.size.h;
  float angle = (float)s_state.gradient_angle_deg_x100 / 100.0f;
  int16_t step = GRADIENT_STEP;
  if ((int32_t)width * (int32_t)height > 45000) {
    step = 4;
  }

  int32_t trig_angle = (int32_t)(angle * (float)TRIG_MAX_ANGLE / 360.0f);
  float vx = sin_lookup(trig_angle) / (float)TRIG_MAX_RATIO;
  float vy = -cos_lookup(trig_angle) / (float)TRIG_MAX_RATIO;
  float cx = (float)width * 0.5f;
  float cy = (float)height * 0.5f;
  float max_projection = (prv_absf((float)width * vx) * 0.5f) + (prv_absf((float)height * vy) * 0.5f);
  float denom = max_projection > 0.000001f ? max_projection : 0.000001f;
  float projection_scale = 0.5f / denom;

  for (int16_t y = 0; y < height; y += step) {
    float y_projection = ((float)y - cy) * vy;
    for (int16_t x = 0; x < width; x += step) {
      int16_t block_x = x / step;
      int16_t block_y = y / step;
      float projection = y_projection + (((float)x - cx) * vx);
      float factor = prv_clampf((projection * projection_scale) + 0.5f, 0.0f, 1.0f);
      factor = prv_clampf(factor + prv_dither_offset(block_x, block_y, DITHER_STRENGTH), 0.0f, 1.0f);

      Rgb color;
      color.r = prv_lerp_i16(palette.top.r, palette.bottom.r, factor);
      color.g = prv_lerp_i16(palette.top.g, palette.bottom.g, factor);
      color.b = prv_lerp_i16(palette.top.b, palette.bottom.b, factor);

      graphics_context_set_fill_color(ctx, prv_make_color_rgb(color));
      graphics_fill_rect(ctx, GRect(x, y, step, step), 0, GCornerNone);
    }
  }
}

static void prv_resolve_city_name(char *out, size_t len) {
  if (s_state.city_name[0] != '\0' && strcmp(s_state.city_name, "Current") != 0 &&
      strcmp(s_state.city_name, "Manual") != 0 && strcmp(s_state.city_name, "Cached") != 0 &&
      strcmp(s_state.city_name, "Backup") != 0) {
    snprintf(out, len, "%s", s_state.city_name);
    return;
  }

  if (s_state.source_code == SOURCE_CHICAGO) {
    snprintf(out, len, "Chicago");
    return;
  }
  if (s_state.source_code == SOURCE_MANUAL) {
    snprintf(out, len, "Manual");
    return;
  }
  if (s_state.source_code == SOURCE_CACHED) {
    snprintf(out, len, "Cached");
    return;
  }
  snprintf(out, len, "Local");
}

static void prv_draw_face(GContext *ctx, GRect bounds) {
  float altitude_deg = (float)s_state.altitude_deg_x100 / 100.0f;
  Palette palette = prv_palette_for_altitude(altitude_deg);
  prv_draw_solar_gradient(ctx, bounds, palette);

  time_t now = time(NULL);
  struct tm *time_info = localtime(&now);
  int hours = time_info->tm_hour % 12;
  if (hours == 0) {
    hours = 12;
  }

  char time_text[8];
  snprintf(time_text, sizeof(time_text), "%d:%02d", hours, time_info->tm_min);

  GSize time_size = prv_text_size(time_text, s_time_font, bounds.size.w, bounds.size.h);
  int16_t time_x = (bounds.size.w - time_size.w) / 2;
  int16_t time_y = (bounds.size.h - time_size.h) / 2;
  Rgb time_sample = prv_interpolate_rgb(palette.top, palette.bottom, 0.5f);
  TextStyle time_style = prv_pick_text_style(time_sample);
  prv_draw_styled_text(ctx, time_text, s_time_font, time_style, time_x, time_y, time_size.w + 2, time_size.h + 2);

  GFont info_font = bounds.size.h < 200 ? s_info_font_small : s_info_font_large;
  char alt_text[24];
  prv_format_altitude_x100(s_state.altitude_deg_x100, alt_text, sizeof(alt_text));

  char city_text[48];
  prv_resolve_city_name(city_text, sizeof(city_text));

  char info_text[96];
  snprintf(info_text, sizeof(info_text), "%s  alt %s deg", city_text, alt_text);

  GSize info_size = prv_text_size(info_text, info_font, bounds.size.w, bounds.size.h);
  int16_t info_x = (bounds.size.w - info_size.w) / 2;
  int16_t info_y = bounds.size.h - info_size.h - 24;
  Rgb info_sample = prv_interpolate_rgb(palette.top, palette.bottom, 0.8f);
  TextStyle info_style = prv_pick_text_style(info_sample);
  prv_draw_styled_text(ctx, info_text, info_font, info_style, info_x, info_y, info_size.w + 2, info_size.h + 2);

  if (s_state.dev_mode_enabled && s_state.dev_show_debug_overlay) {
    char az_text[16];
    prv_format_azimuth_x100(s_state.azimuth_deg_x100, az_text, sizeof(az_text));

    char dev_text[48];
    snprintf(dev_text, sizeof(dev_text), "%s az %s", s_state.dev_sweep_enabled ? "SWEEP" : "DEV", az_text);

    Rgb dev_sample = prv_interpolate_rgb(palette.top, palette.bottom, 0.2f);
    TextStyle dev_style = prv_pick_text_style(dev_sample);
    prv_draw_styled_text(ctx, dev_text, s_info_font_small, dev_style, 4, 4, bounds.size.w - 8, 18);
  }
}

static void prv_draw_loading_card(GContext *ctx, GRect bounds) {
  GColor black = GColorBlack;
  GColor white = GColorWhite;
  GColor panel = prv_make_color_rgb((Rgb){18, 18, 28});
  GColor track = prv_make_color_rgb((Rgb){42, 42, 58});
  GColor fill = prv_make_color_rgb((Rgb){104, 177, 255});

  graphics_context_set_fill_color(ctx, black);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  int16_t panel_w = bounds.size.w - 20;
  if (panel_w > 200) panel_w = 200;
  int16_t panel_h = bounds.size.h - 28;
  if (panel_h > 120) panel_h = 120;
  int16_t panel_x = (bounds.size.w - panel_w) / 2;
  int16_t panel_y = (bounds.size.h - panel_h) / 2;

  graphics_context_set_fill_color(ctx, panel);
  graphics_fill_rect(ctx, GRect(panel_x, panel_y, panel_w, panel_h), 0, GCornerNone);

  graphics_context_set_text_color(ctx, white);
  graphics_draw_text(ctx, "Solar Gradient", s_info_font_large,
                     GRect(panel_x, panel_y + 18, panel_w, 24),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  graphics_draw_text(ctx, prv_loading_status_line(), s_info_font_small,
                     GRect(panel_x + 4, panel_y + 54, panel_w - 8, 18),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  int16_t bar_w = panel_w - 28;
  int16_t bar_h = 8;
  int16_t bar_x = panel_x + 14;
  int16_t bar_y = panel_y + 76;

  graphics_context_set_fill_color(ctx, track);
  graphics_fill_rect(ctx, GRect(bar_x, bar_y, bar_w, bar_h), 0, GCornerNone);

  int16_t fill_w = (int16_t)((bar_w * prv_clamp_i32(s_state.loading_progress, 0, 100)) / 100);
  if (fill_w > 0) {
    graphics_context_set_fill_color(ctx, fill);
    graphics_fill_rect(ctx, GRect(bar_x, bar_y, fill_w, bar_h), 0, GCornerNone);
  }

  char percent_text[8];
  snprintf(percent_text, sizeof(percent_text), "%ld%%", (long)prv_clamp_i32(s_state.loading_progress, 0, 100));
  graphics_context_set_text_color(ctx, white);
  graphics_draw_text(ctx, percent_text, s_info_font_small,
                     GRect(panel_x, panel_y + 90, panel_w, 18),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void prv_canvas_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  if (!s_state.bt_connected && !s_state.has_payload) {
    s_state.launch_done = true;
  }

  if (s_state.launch_done) {
    prv_draw_face(ctx, bounds);
  } else {
    prv_draw_loading_card(ctx, bounds);
  }
}

static void prv_send_refresh_request(void) {
  DictionaryIterator *iter = NULL;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK || !iter) {
    return;
  }

  s_state.refresh_counter = (s_state.refresh_counter + 1) & 0x7fffffff;
  dict_write_uint8(iter, MESSAGE_KEY_RefreshRequest, 1);
  dict_write_uint32(iter, MESSAGE_KEY_ReloadFaceToken, s_state.refresh_counter);
  dict_write_end(iter);
  app_message_outbox_send();
}

static void prv_on_message_received(DictionaryIterator *iter, void *context) {
  bool loading_changed = false;

  Tuple *reload_token = dict_find(iter, MESSAGE_KEY_ReloadFaceToken);
  if (reload_token) {
    uint32_t token = (uint32_t)prv_tuple_to_i32(reload_token);
    if (token != s_state.last_reload_face_token) {
      s_state.last_reload_face_token = token;
      prv_begin_loading();
      loading_changed = true;
    }
  }

  Tuple *override_mode = dict_find(iter, MESSAGE_KEY_TextOverrideMode);
  if (override_mode) {
    s_state.text_override_mode = prv_clamp_i32(prv_tuple_to_i32(override_mode), 0, 3);
    loading_changed = true;
  }

  Tuple *dev_mode = dict_find(iter, MESSAGE_KEY_DevModeEnabled);
  if (dev_mode) {
    s_state.dev_mode_enabled = prv_tuple_to_i32(dev_mode) != 0;
    loading_changed = true;
  }

  Tuple *dev_sweep = dict_find(iter, MESSAGE_KEY_DevSweepEnabled);
  if (dev_sweep) {
    s_state.dev_sweep_enabled = prv_tuple_to_i32(dev_sweep) != 0;
    loading_changed = true;
  }

  Tuple *dev_overlay = dict_find(iter, MESSAGE_KEY_DevShowDebugOverlay);
  if (dev_overlay) {
    s_state.dev_show_debug_overlay = prv_tuple_to_i32(dev_overlay) != 0;
    loading_changed = true;
  }

  Tuple *status_code = dict_find(iter, MESSAGE_KEY_StatusCode);
  if (status_code) {
    int32_t code = prv_tuple_to_i32(status_code);
    prv_set_loading_status(code);
    if (code == STATUS_READY) {
      prv_set_loading_progress_target(100);
    }
    loading_changed = true;
  }

  Tuple *progress_percent = dict_find(iter, MESSAGE_KEY_ProgressPercent);
  if (progress_percent) {
    prv_set_loading_progress_target(prv_tuple_to_i32(progress_percent));
    loading_changed = true;
  }

  Tuple *lat = dict_find(iter, MESSAGE_KEY_LatitudeE6);
  Tuple *lon = dict_find(iter, MESSAGE_KEY_LongitudeE6);
  Tuple *az = dict_find(iter, MESSAGE_KEY_AzimuthDegX100);
  Tuple *alt = dict_find(iter, MESSAGE_KEY_AltitudeDegX100);
  Tuple *angle = dict_find(iter, MESSAGE_KEY_GradientAngleDegX100);
  Tuple *computed = dict_find(iter, MESSAGE_KEY_ComputedAtEpoch);
  Tuple *source = dict_find(iter, MESSAGE_KEY_SourceCode);
  Tuple *city = dict_find(iter, MESSAGE_KEY_CityName);

  if (!lat || !lon || !az || !alt || !angle || !computed) {
    if (loading_changed) {
      layer_mark_dirty(s_canvas_layer);
    }
    return;
  }

  s_state.latitude_e6 = prv_tuple_to_i32(lat);
  s_state.longitude_e6 = prv_tuple_to_i32(lon);
  s_state.azimuth_deg_x100 = prv_tuple_to_i32(az);
  s_state.altitude_deg_x100 = prv_tuple_to_i32(alt);
  s_state.gradient_angle_deg_x100 = prv_tuple_to_i32(angle);
  s_state.computed_at_epoch = prv_tuple_to_i32(computed);

  if (source) {
    s_state.source_code = prv_tuple_to_i32(source);
  }
  if (city) {
    snprintf(s_state.city_name, sizeof(s_state.city_name), "%s", city->value->cstring);
  }

  s_state.has_payload = true;
  s_state.had_payload_once = true;
  prv_set_loading_status(STATUS_READY);
  s_state.loading_progress_target = 100;
  s_state.loading_progress = 100;
  s_state.launch_transition_deadline_ms = prv_now_ms() + LOADING_TRANSITION_HOLD_MS;
  prv_start_loading_timer();
  layer_mark_dirty(s_canvas_layer);
}

static void prv_on_connection_event(bool connected) {
  s_state.bt_connected = connected;
  if (connected && s_state.had_payload_once) {
    prv_begin_loading();
    prv_send_refresh_request();
  }
  layer_mark_dirty(s_canvas_layer);
}

static void prv_on_minute_tick(struct tm *tick_time, TimeUnits changed) {
  (void)tick_time;
  (void)changed;
  layer_mark_dirty(s_canvas_layer);
}

static void prv_loading_timer_cb(void *context) {
  (void)context;
  s_loading_timer = NULL;
  bool changed = false;
  uint32_t now_ms = prv_now_ms();

  if (!s_state.launch_done) {
    changed = prv_advance_loading_progress(now_ms);

    uint32_t elapsed_ms = now_ms - s_state.loading_started_ms;
    uint8_t next_hint = 0;
    if (elapsed_ms > LOADING_STALE_HINT_MS && elapsed_ms <= LOADING_STILL_WORKING_MS &&
        s_state.status_code == STATUS_GRABBING_LOCATION) {
      next_hint = 1;
    } else if (elapsed_ms > LOADING_STILL_WORKING_MS && elapsed_ms <= LOADING_TIMEOUT_MS) {
      next_hint = 2;
    }
    if (next_hint != s_state.loading_hint_mode) {
      s_state.loading_hint_mode = next_hint;
      changed = true;
    }

    if (elapsed_ms > LOADING_TIMEOUT_MS) {
      s_state.launch_done = true;
      changed = true;
    }

    if (s_state.launch_transition_deadline_ms != 0 && now_ms >= s_state.launch_transition_deadline_ms) {
      s_state.launch_transition_deadline_ms = 0;
      s_state.launch_done = true;
      changed = true;
    }
  }

  if (changed) {
    layer_mark_dirty(s_canvas_layer);
  }

  if (!s_state.launch_done) {
    s_loading_timer = app_timer_register(LOADING_TIMER_INTERVAL_MS, prv_loading_timer_cb, NULL);
  }
}

static void prv_start_loading_timer(void) {
  if (!s_loading_timer && !s_state.launch_done) {
    s_loading_timer = app_timer_register(LOADING_TIMER_INTERVAL_MS, prv_loading_timer_cb, NULL);
  }
}

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_canvas_layer = layer_create(bounds);
  layer_set_update_proc(s_canvas_layer, prv_canvas_update_proc);
  layer_add_child(window_layer, s_canvas_layer);
}

static void prv_window_unload(Window *window) {
  (void)window;
  if (s_canvas_layer) {
    layer_destroy(s_canvas_layer);
    s_canvas_layer = NULL;
  }
}

static void prv_init(void) {
  s_time_font = fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD);
  s_info_font_large = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  s_info_font_small = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);

  s_state.bt_connected = bluetooth_connection_service_peek();
  s_state.loading_started_ms = prv_now_ms();
  s_state.loading_status_started_ms = s_state.loading_started_ms;

  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(s_window, true);

  app_message_register_inbox_received(prv_on_message_received);
  app_message_open(512, 256);

  tick_timer_service_subscribe(MINUTE_UNIT, prv_on_minute_tick);
  bluetooth_connection_service_subscribe(prv_on_connection_event);

  prv_begin_loading();
  prv_send_refresh_request();
}

static void prv_deinit(void) {
  if (s_loading_timer) {
    app_timer_cancel(s_loading_timer);
    s_loading_timer = NULL;
  }

  tick_timer_service_unsubscribe();
  bluetooth_connection_service_unsubscribe();
  app_message_deregister_callbacks();

  window_destroy(s_window);
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}
