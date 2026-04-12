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

#define MOTION_MODE_HYBRID 0
#define MOTION_MODE_SUBTLE 1
#define MOTION_MODE_OFF 2

#define TIME_SIZE_COMPACT 0
#define TIME_SIZE_BALANCED 1
#define TIME_SIZE_LARGE 2

#define WEATHER_STATUS_STALE 1
#define WEATHER_STATUS_FAILED 2

#define LOADING_TIMEOUT_MS 15000
#define LOADING_STALE_HINT_MS 3500
#define LOADING_STILL_WORKING_MS 6000
#define LOADING_TIMER_INTERVAL_MS 125
#define LOADING_TRANSITION_HOLD_MS 180

#define ANGLE_TRANSITION_MS 3500
#define AMBIENT_DRIFT_PERIOD_MS 18000
#define AMBIENT_DRIFT_AMPLITUDE_DEG 2.8f
#define REFRESH_DRIFT_BOOST_MS 5000
#define REFRESH_BADGE_MS 1800

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
  int16_t width;
  int16_t height;
  int16_t gradient_step_large;
  int32_t large_step_area_threshold;
  uint16_t animation_interval_ms;
  float daylight_contrast_mult;
  float daylight_shift_mult;
  float gradient_widen_mult;
  float dither_mult;
  float bloom_gain_mult;
  float bloom_radius_mult;
  float drift_mult;
} RenderProfile;

typedef struct {
  int32_t source_code;
  int32_t latitude_e6;
  int32_t longitude_e6;
  int32_t azimuth_deg_x100;
  int32_t altitude_deg_x100;
  int32_t gradient_angle_deg_x100;
  int32_t computed_at_epoch;
  int32_t text_override_mode;
  int32_t motion_mode;
  int32_t battery_save_mode;
  int32_t time_size_basalt;
  int32_t time_size_chalk;
  int32_t time_size_emery;
  int32_t time_size_gabbro;
  int32_t show_location;
  int32_t show_altitude;
  int32_t weather_enabled;
  int32_t weather_unit_fahrenheit;
  int32_t weather_detail_level;
  int32_t weather_status;
  int32_t weather_temp_x10;
  int32_t weather_cloud_cover;
  int32_t weather_code;
  int32_t weather_wind_x10;
  int32_t weather_precip_x100;
  int32_t weather_updated_epoch;
  int32_t custom_location_enabled;
  int32_t custom_latitude_e6;
  int32_t custom_longitude_e6;
  int32_t debug_benchmark;
  int32_t status_code;
  int32_t loading_progress;
  int32_t loading_progress_target;
  int32_t target_gradient_angle_deg_x100;
  int32_t previous_gradient_angle_deg_x100;
  uint32_t last_reload_face_token;
  uint32_t refresh_counter;
  uint32_t loading_started_ms;
  uint32_t loading_status_started_ms;
  uint32_t launch_transition_deadline_ms;
  uint32_t angle_transition_started_ms;
  uint32_t last_payload_received_ms;
  uint32_t refresh_badge_until_ms;
  bool launch_done;
  bool has_payload;
  bool had_payload_once;
  bool angle_transition_active;
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

static const RenderProfile s_render_profiles[] = {
  // baseline fallback profile (used when no exact resolution match exists)
  { 0, 0, 4, 45000, 1000, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
  // basalt 144x168
  { 144, 168, 3, 70000, 1000, 1.04f, 1.05f, 1.05f, 1.05f, 1.00f, 0.95f, 1.00f },
  // chalk 180x180
  { 180, 180, 3, 70000, 900, 1.08f, 1.08f, 1.10f, 1.10f, 1.08f, 1.05f, 1.08f },
  // emery 200x228
  { 200, 228, 3, 45000, 1100, 1.15f, 1.12f, 1.15f, 1.00f, 1.12f, 1.10f, 0.95f },
  // gabbro 260x260
  { 260, 260, 3, 50000, 1200, 1.20f, 1.15f, 1.20f, 1.05f, 1.18f, 1.18f, 1.02f },
};

static Window *s_window;
static Layer *s_canvas_layer;
static AppTimer *s_loading_timer;
static AppTimer *s_animation_timer;
static const RenderProfile *s_active_profile;

static GFont s_time_font;
static GFont s_info_font_large;
static GFont s_info_font_small;

static AppState s_state = {
  .source_code = SOURCE_CHICAGO,
  .latitude_e6 = 41878100,
  .longitude_e6 = -87629800,
  .azimuth_deg_x100 = 18000,
  .altitude_deg_x100 = -1800,
  .gradient_angle_deg_x100 = 0,
  .target_gradient_angle_deg_x100 = 0,
  .previous_gradient_angle_deg_x100 = 0,
  .computed_at_epoch = 0,
  .text_override_mode = TEXT_MODE_AUTO,
  .motion_mode = MOTION_MODE_HYBRID,
  .battery_save_mode = 0,
  .time_size_basalt = TIME_SIZE_BALANCED,
  .time_size_chalk = TIME_SIZE_BALANCED,
  .time_size_emery = TIME_SIZE_BALANCED,
  .time_size_gabbro = TIME_SIZE_BALANCED,
  .show_location = 1,
  .show_altitude = 1,
  .weather_enabled = 0,
  .weather_unit_fahrenheit = 1,
  .weather_detail_level = 1,
  .weather_status = WEATHER_STATUS_STALE,
  .weather_temp_x10 = 0,
  .weather_cloud_cover = 0,
  .weather_code = 0,
  .weather_wind_x10 = 0,
  .weather_precip_x100 = 0,
  .weather_updated_epoch = 0,
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

static const RenderProfile *prv_render_profile_for_bounds(GRect bounds) {
  for (size_t i = 1; i < sizeof(s_render_profiles) / sizeof(s_render_profiles[0]); i++) {
    if (s_render_profiles[i].width == bounds.size.w && s_render_profiles[i].height == bounds.size.h) {
      return &s_render_profiles[i];
    }
  }
  return &s_render_profiles[0];
}

static int32_t prv_time_size_mode_for_profile(const RenderProfile *profile) {
  if (profile->width == 144 && profile->height == 168) {
    return prv_clamp_i32(s_state.time_size_basalt, TIME_SIZE_COMPACT, TIME_SIZE_LARGE);
  }
  if (profile->width == 180 && profile->height == 180) {
    return prv_clamp_i32(s_state.time_size_chalk, TIME_SIZE_COMPACT, TIME_SIZE_LARGE);
  }
  if (profile->width == 200 && profile->height == 228) {
    return prv_clamp_i32(s_state.time_size_emery, TIME_SIZE_COMPACT, TIME_SIZE_LARGE);
  }
  if (profile->width == 260 && profile->height == 260) {
    return prv_clamp_i32(s_state.time_size_gabbro, TIME_SIZE_COMPACT, TIME_SIZE_LARGE);
  }
  return TIME_SIZE_BALANCED;
}

static GFont prv_time_font_for_mode(const RenderProfile *profile, int32_t size_mode) {
  GFont font = s_time_font;
  int32_t mode = prv_clamp_i32(size_mode, TIME_SIZE_COMPACT, TIME_SIZE_LARGE);

  if (mode == TIME_SIZE_COMPACT) {
    font = fonts_get_system_font(FONT_KEY_BITHAM_34_MEDIUM_NUMBERS);
  } else if (mode == TIME_SIZE_LARGE && profile->width >= 200 && profile->height >= 200) {
    font = fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS);
  } else {
    font = fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD);
  }

  return font ? font : s_time_font;
}

static int32_t prv_effective_motion_mode(void) {
  int32_t mode = prv_clamp_i32(s_state.motion_mode, MOTION_MODE_HYBRID, MOTION_MODE_OFF);
  if (s_state.battery_save_mode != 0 && mode != MOTION_MODE_OFF) {
    return MOTION_MODE_SUBTLE;
  }
  return mode;
}

static uint32_t prv_animation_interval_for_profile(const RenderProfile *profile) {
  int32_t mode = prv_effective_motion_mode();
  if (mode == MOTION_MODE_OFF) {
    return 3000;
  }
  if (mode == MOTION_MODE_SUBTLE) {
    return (uint32_t)profile->animation_interval_ms * 2;
  }
  return profile->animation_interval_ms;
}

static int32_t prv_wrap_degrees_x100(int32_t angle_x100) {
  int32_t wrapped = angle_x100 % 36000;
  if (wrapped < 0) {
    wrapped += 36000;
  }
  return wrapped;
}

static int32_t prv_shortest_delta_degrees_x100(int32_t from_x100, int32_t to_x100) {
  int32_t from = prv_wrap_degrees_x100(from_x100);
  int32_t to = prv_wrap_degrees_x100(to_x100);
  int32_t delta = to - from;
  if (delta > 18000) {
    delta -= 36000;
  } else if (delta < -18000) {
    delta += 36000;
  }
  return delta;
}

static float prv_wave_sine_ms(uint32_t now_ms, uint32_t period_ms, uint32_t phase_ms) {
  if (period_ms == 0) {
    return 0.0f;
  }
  uint32_t shifted = (now_ms + phase_ms) % period_ms;
  int32_t trig_angle = (int32_t)(((int64_t)shifted * TRIG_MAX_ANGLE) / period_ms);
  return sin_lookup(trig_angle) / (float)TRIG_MAX_RATIO;
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

static Palette prv_enhance_daylight_palette(Palette base, float altitude_deg, const RenderProfile *profile) {
#ifdef PBL_COLOR
  if (altitude_deg < 35.0f) {
    return base;
  }

  float strength = prv_clampf((altitude_deg - 35.0f) / 20.0f, 0.0f, 1.0f);
  Palette out = prv_widen_palette_contrast(base, 1.0f + (0.35f * strength * profile->daylight_contrast_mult));
  float shift_scale = profile->daylight_shift_mult;

  out.top.r = prv_clamp_i16(out.top.r - prv_round_i32(26.0f * strength * shift_scale), 0, 255);
  out.top.g = prv_clamp_i16(out.top.g - prv_round_i32(18.0f * strength * shift_scale), 0, 255);
  out.top.b = prv_clamp_i16(out.top.b + prv_round_i32(10.0f * strength * shift_scale), 0, 255);

  out.bottom.r = prv_clamp_i16(out.bottom.r + prv_round_i32(18.0f * strength * shift_scale), 0, 255);
  out.bottom.g = prv_clamp_i16(out.bottom.g + prv_round_i32(14.0f * strength * shift_scale), 0, 255);
  out.bottom.b = prv_clamp_i16(out.bottom.b + prv_round_i32(6.0f * strength * shift_scale), 0, 255);
  return out;
#else
  (void)profile;
  (void)altitude_deg;
  return base;
#endif
}

static Palette prv_palette_for_altitude(float altitude_deg, const RenderProfile *profile) {
  Palette out;
  const int band_count = (int)(sizeof(s_atmosphere) / sizeof(s_atmosphere[0]));

  if (altitude_deg <= s_atmosphere[0].altitude) {
    out.top = s_atmosphere[0].top;
    out.bottom = s_atmosphere[0].bottom;
    return prv_enhance_daylight_palette(out, altitude_deg, profile);
  }

  const AtmosphereBand *max_band = &s_atmosphere[band_count - 1];
  if (altitude_deg >= max_band->altitude) {
    float daylight_strength = prv_clampf((altitude_deg - (float)max_band->altitude) / 35.0f, 0.0f, 1.0f);
    float contrast_scale = 1.0f + (daylight_strength * 0.45f * profile->gradient_widen_mult);
    out.top = max_band->top;
    out.bottom = max_band->bottom;
    out = prv_widen_palette_contrast(out, contrast_scale);
    return prv_enhance_daylight_palette(out, altitude_deg, profile);
  }

  for (int i = 0; i < band_count - 1; i++) {
    const AtmosphereBand *low = &s_atmosphere[i];
    const AtmosphereBand *high = &s_atmosphere[i + 1];
    if (altitude_deg >= low->altitude && altitude_deg <= high->altitude) {
      float range = (float)(high->altitude - low->altitude);
      float t = (altitude_deg - (float)low->altitude) / (range <= 0.0f ? 1.0f : range);
      out.top = prv_interpolate_rgb(low->top, high->top, t);
      out.bottom = prv_interpolate_rgb(low->bottom, high->bottom, t);
      return prv_enhance_daylight_palette(out, altitude_deg, profile);
    }
  }

  out.top = s_atmosphere[0].top;
  out.bottom = s_atmosphere[0].bottom;
  return prv_enhance_daylight_palette(out, altitude_deg, profile);
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
                                 int16_t x, int16_t y, int16_t width, int16_t height,
                                 GTextAlignment alignment) {
  GRect rect = GRect(x, y, width, height);
  if (style.glow) {
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, text, font, GRect(rect.origin.x - 1, rect.origin.y, rect.size.w, rect.size.h),
                       GTextOverflowModeTrailingEllipsis, alignment, NULL);
    graphics_draw_text(ctx, text, font, GRect(rect.origin.x + 1, rect.origin.y, rect.size.w, rect.size.h),
                       GTextOverflowModeTrailingEllipsis, alignment, NULL);
    graphics_draw_text(ctx, text, font, GRect(rect.origin.x, rect.origin.y - 1, rect.size.w, rect.size.h),
                       GTextOverflowModeTrailingEllipsis, alignment, NULL);
    graphics_draw_text(ctx, text, font, GRect(rect.origin.x, rect.origin.y + 1, rect.size.w, rect.size.h),
                       GTextOverflowModeTrailingEllipsis, alignment, NULL);
  }

  graphics_context_set_text_color(ctx, style.color);
  graphics_draw_text(ctx, text, font, rect, GTextOverflowModeTrailingEllipsis, alignment, NULL);
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
static void prv_start_animation_timer(void);
static void prv_restart_animation_timer(void);

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

static const char *prv_weather_code_label(int32_t code) {
  if (code == 0) return "clear";
  if (code >= 1 && code <= 3) return "clouds";
  if (code == 45 || code == 48) return "fog";
  if (code >= 51 && code <= 57) return "drizzle";
  if ((code >= 61 && code <= 67) || (code >= 80 && code <= 82)) return "rain";
  if ((code >= 71 && code <= 77) || code == 85 || code == 86) return "snow";
  if (code >= 95) return "storm";
  return "wx";
}

static void prv_format_weather_text(char *out, size_t len) {
  out[0] = '\0';

  if (s_state.weather_enabled == 0 || s_state.weather_detail_level <= 0) {
    return;
  }

  if (s_state.weather_status == WEATHER_STATUS_FAILED) {
    snprintf(out, len, "wx err");
    return;
  }

  if (s_state.weather_updated_epoch <= 0) {
    snprintf(out, len, "wx --");
    return;
  }

  int32_t temp_x10 = s_state.weather_temp_x10;
  int32_t abs_temp_x10 = temp_x10 < 0 ? -temp_x10 : temp_x10;
  int32_t temp_whole = abs_temp_x10 / 10;
  int32_t temp_tenth = abs_temp_x10 % 10;
  const char *label = prv_weather_code_label(s_state.weather_code);
  char unit = s_state.weather_unit_fahrenheit ? 'F' : 'C';
  const char *stale = (s_state.weather_status == WEATHER_STATUS_STALE) ? "~" : "";

  if (s_state.weather_detail_level >= 2) {
    int32_t cloud = prv_clamp_i32(s_state.weather_cloud_cover, 0, 100);
    snprintf(out, len, "%s%s%ld.%ld%c %s %ld%%",
             stale,
             temp_x10 < 0 ? "-" : "",
             (long)temp_whole,
             (long)temp_tenth,
             unit,
             label,
             (long)cloud);
    return;
  }

  snprintf(out, len, "%s%s%ld.%ld%c %s",
           stale,
           temp_x10 < 0 ? "-" : "",
           (long)temp_whole,
           (long)temp_tenth,
           unit,
           label);
}

static float prv_effective_gradient_angle_deg(uint32_t now_ms, const RenderProfile *profile) {
  int32_t base_angle_x100 = s_state.gradient_angle_deg_x100;

  if (s_state.angle_transition_active) {
    uint32_t elapsed = now_ms - s_state.angle_transition_started_ms;
    float t = prv_clampf((float)elapsed / (float)ANGLE_TRANSITION_MS, 0.0f, 1.0f);
    float eased = t * t * (3.0f - (2.0f * t));
    int32_t delta = prv_shortest_delta_degrees_x100(s_state.previous_gradient_angle_deg_x100,
                                                     s_state.target_gradient_angle_deg_x100);
    base_angle_x100 = s_state.previous_gradient_angle_deg_x100 + prv_round_i32((float)delta * eased);
    base_angle_x100 = prv_wrap_degrees_x100(base_angle_x100);

    if (elapsed >= ANGLE_TRANSITION_MS) {
      s_state.angle_transition_active = false;
      s_state.gradient_angle_deg_x100 = s_state.target_gradient_angle_deg_x100;
      base_angle_x100 = s_state.gradient_angle_deg_x100;
    }
  }

  int32_t motion_mode = prv_effective_motion_mode();
  if (motion_mode == MOTION_MODE_OFF) {
    return (float)prv_wrap_degrees_x100(base_angle_x100) / 100.0f;
  }

  float drift_scale = profile->drift_mult * (motion_mode == MOTION_MODE_SUBTLE ? 0.35f : 1.0f);
  float ambient_drift = prv_wave_sine_ms(now_ms, AMBIENT_DRIFT_PERIOD_MS, 0) * AMBIENT_DRIFT_AMPLITUDE_DEG * drift_scale;
  float refresh_ease_drift = 0.0f;
  if (s_state.last_payload_received_ms != 0) {
    uint32_t since_payload = now_ms - s_state.last_payload_received_ms;
    uint32_t boost_window_ms = (motion_mode == MOTION_MODE_SUBTLE) ? 2500 : REFRESH_DRIFT_BOOST_MS;
    if (since_payload < boost_window_ms) {
      float ramp = 1.0f - ((float)since_payload / (float)boost_window_ms);
      float boost_amount = (motion_mode == MOTION_MODE_SUBTLE) ? 0.45f : 1.2f;
      refresh_ease_drift = prv_wave_sine_ms(now_ms, 3000, 450) * boost_amount * ramp * profile->drift_mult;
    }
  }

  float angle = ((float)base_angle_x100 / 100.0f) + ambient_drift + refresh_ease_drift;
  while (angle < 0.0f) {
    angle += 360.0f;
  }
  while (angle >= 360.0f) {
    angle -= 360.0f;
  }
  return angle;
}

static float prv_dynamic_dither_strength(uint32_t now_ms, const RenderProfile *profile) {
  int32_t motion_mode = prv_effective_motion_mode();
  if (motion_mode == MOTION_MODE_OFF) {
    return prv_clampf((DITHER_STRENGTH * 0.70f) * profile->dither_mult, 0.010f, 0.045f);
  }

  float wave = (prv_wave_sine_ms(now_ms, 22000, 1700) + 1.0f) * 0.5f;
  float variation_scale = (motion_mode == MOTION_MODE_SUBTLE) ? 0.45f : 1.0f;
  float strength = DITHER_STRENGTH * (0.85f + (0.25f * wave * variation_scale));
  strength *= profile->dither_mult;

  if (s_state.last_payload_received_ms != 0) {
    uint32_t since_payload = now_ms - s_state.last_payload_received_ms;
    if (since_payload < REFRESH_DRIFT_BOOST_MS) {
      float boost = 1.0f - ((float)since_payload / (float)REFRESH_DRIFT_BOOST_MS);
      float boost_amount = (motion_mode == MOTION_MODE_SUBTLE) ? 0.15f : 0.35f;
      strength *= 1.0f + (boost_amount * boost);
    }
  }
  return prv_clampf(strength, 0.015f, 0.07f);
}

static void prv_draw_solar_gradient(GContext *ctx, GRect bounds, Palette palette, uint32_t now_ms,
                                    const RenderProfile *profile) {
  int16_t width = bounds.size.w;
  int16_t height = bounds.size.h;
  float angle = prv_effective_gradient_angle_deg(now_ms, profile);
  float altitude_deg = (float)s_state.altitude_deg_x100 / 100.0f;
  float dither_strength = prv_dynamic_dither_strength(now_ms, profile);
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

  float daylight_strength = prv_clampf((altitude_deg - 25.0f) / 30.0f, 0.0f, 1.0f);
  int32_t sun_trig = (int32_t)(((float)s_state.azimuth_deg_x100 / 100.0f) * (float)TRIG_MAX_ANGLE / 360.0f);
  float sun_vx = sin_lookup(sun_trig) / (float)TRIG_MAX_RATIO;
  float sun_vy = -cos_lookup(sun_trig) / (float)TRIG_MAX_RATIO;
  float min_dim = width < height ? width : height;
  float altitude_normalized = prv_clampf((altitude_deg + 5.0f) / 70.0f, 0.0f, 1.0f);
  float bloom_offset = (1.0f - altitude_normalized) * (min_dim * 0.30f);
  float bloom_x = cx + (sun_vx * bloom_offset);
  float bloom_y = cy + (sun_vy * bloom_offset);
  float bloom_radius = min_dim * (0.18f + (0.08f * daylight_strength * profile->bloom_radius_mult));
  float bloom_radius_sq = bloom_radius * bloom_radius;
  float bloom_gain = 0.11f * daylight_strength * profile->bloom_gain_mult;

  for (int16_t y = 0; y < height; y += step) {
    float y_projection = ((float)y - cy) * vy;
    for (int16_t x = 0; x < width; x += step) {
      int16_t block_x = x / step;
      int16_t block_y = y / step;
      float projection = y_projection + (((float)x - cx) * vx);
      float factor = prv_clampf((projection * projection_scale) + 0.5f, 0.0f, 1.0f);
      if (daylight_strength > 0.0f) {
        float widened = ((factor - 0.5f) * (1.0f + (0.35f * daylight_strength * profile->gradient_widen_mult))) + 0.5f;
        factor = prv_clampf(widened, 0.0f, 1.0f);
      }
      factor = prv_clampf(factor + prv_dither_offset(block_x, block_y, dither_strength), 0.0f, 1.0f);

      if (daylight_strength > 0.0f) {
        float sample_x = (float)x + ((float)step * 0.5f);
        float sample_y = (float)y + ((float)step * 0.5f);
        float dx = sample_x - bloom_x;
        float dy = sample_y - bloom_y;
        float dist_sq = (dx * dx) + (dy * dy);
        if (dist_sq < bloom_radius_sq) {
          float bloom = 1.0f - (dist_sq / bloom_radius_sq);
          factor = prv_clampf(factor + (bloom * bloom_gain), 0.0f, 1.0f);
        }
      }

      Rgb color;
      color.r = prv_lerp_i16(palette.top.r, palette.bottom.r, factor);
      color.g = prv_lerp_i16(palette.top.g, palette.bottom.g, factor);
      color.b = prv_lerp_i16(palette.top.b, palette.bottom.b, factor);

      graphics_context_set_fill_color(ctx, prv_make_color_rgb(color));
      graphics_fill_rect(ctx, GRect(x, y, step, step), 0, GCornerNone);
    }
  }
}

static void prv_draw_refresh_badge(GContext *ctx, GRect bounds, uint32_t now_ms) {
  if (now_ms >= s_state.refresh_badge_until_ms) {
    return;
  }

  float remaining = (float)(s_state.refresh_badge_until_ms - now_ms) / (float)REFRESH_BADGE_MS;
  remaining = prv_clampf(remaining, 0.0f, 1.0f);
  float pulse = (prv_wave_sine_ms(now_ms, 650, 0) + 1.0f) * 0.5f;
  int16_t size = 4 + prv_round_i32(2.0f * pulse);
  int16_t x = bounds.size.w - size - 5;
  int16_t y = 5;

#ifdef PBL_COLOR
  Rgb indicator = {
    .r = prv_clamp_i16(160 + prv_round_i32(70.0f * remaining), 0, 255),
    .g = prv_clamp_i16(220 + prv_round_i32(20.0f * remaining), 0, 255),
    .b = 255
  };
  graphics_context_set_fill_color(ctx, prv_make_color_rgb(indicator));
#else
  graphics_context_set_fill_color(ctx, GColorWhite);
#endif
  graphics_fill_rect(ctx, GRect(x, y, size, size), 1, GCornersAll);
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
  uint32_t now_ms = prv_now_ms();
  const RenderProfile *profile = s_active_profile ? s_active_profile : &s_render_profiles[0];
  float altitude_deg = (float)s_state.altitude_deg_x100 / 100.0f;
  Palette palette = prv_palette_for_altitude(altitude_deg, profile);
  prv_draw_solar_gradient(ctx, bounds, palette, now_ms, profile);

  time_t now = time(NULL);
  struct tm *time_info = localtime(&now);
  int hours = time_info->tm_hour % 12;
  if (hours == 0) {
    hours = 12;
  }

  char time_text[8];
  snprintf(time_text, sizeof(time_text), "%d:%02d", hours, time_info->tm_min);

  int32_t time_size_mode = prv_time_size_mode_for_profile(profile);
  GFont time_font = prv_time_font_for_mode(profile, time_size_mode);

  char location_text[96];
  location_text[0] = '\0';

  if (s_state.show_location != 0 || s_state.show_altitude != 0) {
    char city_text[48];
    char alt_text[24];
    city_text[0] = '\0';
    alt_text[0] = '\0';

    if (s_state.show_location != 0) {
      prv_resolve_city_name(city_text, sizeof(city_text));
    }

    if (s_state.show_altitude != 0) {
      prv_format_altitude_x100(s_state.altitude_deg_x100, alt_text, sizeof(alt_text));
    }

    if (s_state.show_location != 0 && s_state.show_altitude != 0) {
      snprintf(location_text, sizeof(location_text), "%s / alt %s deg", city_text, alt_text);
    } else if (s_state.show_location != 0) {
      snprintf(location_text, sizeof(location_text), "%s", city_text);
    } else if (s_state.show_altitude != 0) {
      snprintf(location_text, sizeof(location_text), "alt %s deg", alt_text);
    }
  }

  char weather_text[56];
  prv_format_weather_text(weather_text, sizeof(weather_text));

  int16_t side_pad = prv_clamp_i16(bounds.size.w / 14, 6, 20);
  int16_t top_bottom_pad = prv_clamp_i16(bounds.size.h / 12, 8, 26);
  int16_t content_w = bounds.size.w - (2 * side_pad);
  if (content_w < 20) {
    content_w = bounds.size.w;
    side_pad = 0;
  }

  GFont info_font = bounds.size.h < 200 ? s_info_font_small : s_info_font_large;
  int16_t line_gap_main = prv_clamp_i16(bounds.size.h / 36, 2, 8);
  int16_t line_gap_info = prv_clamp_i16(bounds.size.h / 48, 1, 6);

  GSize time_size = prv_text_size(time_text, time_font, content_w, bounds.size.h);
  GSize location_size = GSize(0, 0);
  GSize weather_size = GSize(0, 0);
  bool show_location_line = (location_text[0] != '\0');
  bool show_weather_line = (weather_text[0] != '\0');

  if (show_location_line) {
    location_size = prv_text_size(location_text, info_font, content_w, bounds.size.h);
  }
  if (show_weather_line) {
    weather_size = prv_text_size(weather_text, info_font, content_w, bounds.size.h);
  }

  int16_t block_h = time_size.h;
  if (show_location_line || show_weather_line) {
    block_h += line_gap_main;
  }
  if (show_location_line) {
    block_h += location_size.h;
  }
  if (show_location_line && show_weather_line) {
    block_h += line_gap_info;
  }
  if (show_weather_line) {
    block_h += weather_size.h;
  }

  int16_t available_h = bounds.size.h - (2 * top_bottom_pad);
  if (block_h > available_h) {
    line_gap_main = 1;
    line_gap_info = 1;
    block_h = time_size.h;
    if (show_location_line || show_weather_line) {
      block_h += line_gap_main;
    }
    if (show_location_line) {
      block_h += location_size.h;
    }
    if (show_location_line && show_weather_line) {
      block_h += line_gap_info;
    }
    if (show_weather_line) {
      block_h += weather_size.h;
    }
  }

  int16_t start_y = top_bottom_pad;
  if (available_h > block_h) {
    start_y += (available_h - block_h) / 2;
  }

  int16_t y_cursor = start_y;
  float time_sample_t = prv_clampf(((float)y_cursor + ((float)time_size.h * 0.5f)) / (float)bounds.size.h, 0.0f, 1.0f);
  Rgb time_sample = prv_interpolate_rgb(palette.top, palette.bottom, time_sample_t);
  TextStyle time_style = prv_pick_text_style(time_sample);
  prv_draw_styled_text(ctx, time_text, time_font, time_style,
                       side_pad, y_cursor, content_w, time_size.h + 2, GTextAlignmentCenter);
  y_cursor += time_size.h;

  if (show_location_line || show_weather_line) {
    y_cursor += line_gap_main;
  }

  if (show_location_line) {
    float location_sample_t = prv_clampf(((float)y_cursor + ((float)location_size.h * 0.5f)) / (float)bounds.size.h,
                                         0.0f, 1.0f);
    Rgb location_sample = prv_interpolate_rgb(palette.top, palette.bottom, location_sample_t);
    TextStyle location_style = prv_pick_text_style(location_sample);
    prv_draw_styled_text(ctx, location_text, info_font, location_style,
                         side_pad, y_cursor, content_w, location_size.h + 2, GTextAlignmentCenter);
    y_cursor += location_size.h;
  }

  if (show_location_line && show_weather_line) {
    y_cursor += line_gap_info;
  }

  if (show_weather_line) {
    float weather_sample_t = prv_clampf(((float)y_cursor + ((float)weather_size.h * 0.5f)) / (float)bounds.size.h,
                                        0.0f, 1.0f);
    Rgb weather_sample = prv_interpolate_rgb(palette.top, palette.bottom, weather_sample_t);
    TextStyle weather_style = prv_pick_text_style(weather_sample);
    prv_draw_styled_text(ctx, weather_text, info_font, weather_style,
                         side_pad, y_cursor, content_w, weather_size.h + 2, GTextAlignmentCenter);
  }

  if (s_state.dev_mode_enabled && s_state.dev_show_debug_overlay) {
    char az_text[16];
    prv_format_azimuth_x100(s_state.azimuth_deg_x100, az_text, sizeof(az_text));

    char dev_text[48];
    snprintf(dev_text, sizeof(dev_text), "%s az %s", s_state.dev_sweep_enabled ? "SWEEP" : "DEV", az_text);

    Rgb dev_sample = prv_interpolate_rgb(palette.top, palette.bottom, 0.2f);
    TextStyle dev_style = prv_pick_text_style(dev_sample);
    prv_draw_styled_text(ctx, dev_text, s_info_font_small, dev_style,
                         4, 4, bounds.size.w - 8, 18, GTextAlignmentLeft);
  }

  prv_draw_refresh_badge(ctx, bounds, now_ms);
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
  s_active_profile = prv_render_profile_for_bounds(bounds);

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
  bool motion_changed = false;
  (void)context;

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

  Tuple *motion_mode = dict_find(iter, MESSAGE_KEY_MotionMode);
  if (motion_mode) {
    s_state.motion_mode = prv_clamp_i32(prv_tuple_to_i32(motion_mode), MOTION_MODE_HYBRID, MOTION_MODE_OFF);
    loading_changed = true;
    motion_changed = true;
  }

  Tuple *battery_mode = dict_find(iter, MESSAGE_KEY_BatterySaveMode);
  if (battery_mode) {
    s_state.battery_save_mode = prv_tuple_to_i32(battery_mode) != 0 ? 1 : 0;
    loading_changed = true;
    motion_changed = true;
  }

  Tuple *time_size_basalt = dict_find(iter, MESSAGE_KEY_TimeSizeBasalt);
  if (time_size_basalt) {
    s_state.time_size_basalt = prv_clamp_i32(prv_tuple_to_i32(time_size_basalt), TIME_SIZE_COMPACT, TIME_SIZE_LARGE);
    loading_changed = true;
  }

  Tuple *time_size_chalk = dict_find(iter, MESSAGE_KEY_TimeSizeChalk);
  if (time_size_chalk) {
    s_state.time_size_chalk = prv_clamp_i32(prv_tuple_to_i32(time_size_chalk), TIME_SIZE_COMPACT, TIME_SIZE_LARGE);
    loading_changed = true;
  }

  Tuple *time_size_emery = dict_find(iter, MESSAGE_KEY_TimeSizeEmery);
  if (time_size_emery) {
    s_state.time_size_emery = prv_clamp_i32(prv_tuple_to_i32(time_size_emery), TIME_SIZE_COMPACT, TIME_SIZE_LARGE);
    loading_changed = true;
  }

  Tuple *time_size_gabbro = dict_find(iter, MESSAGE_KEY_TimeSizeGabbro);
  if (time_size_gabbro) {
    s_state.time_size_gabbro = prv_clamp_i32(prv_tuple_to_i32(time_size_gabbro), TIME_SIZE_COMPACT, TIME_SIZE_LARGE);
    loading_changed = true;
  }

  Tuple *show_location = dict_find(iter, MESSAGE_KEY_ShowLocation);
  if (show_location) {
    s_state.show_location = prv_tuple_to_i32(show_location) != 0 ? 1 : 0;
    loading_changed = true;
  }

  Tuple *show_altitude = dict_find(iter, MESSAGE_KEY_ShowAltitude);
  if (show_altitude) {
    s_state.show_altitude = prv_tuple_to_i32(show_altitude) != 0 ? 1 : 0;
    loading_changed = true;
  }

  Tuple *weather_enabled = dict_find(iter, MESSAGE_KEY_WeatherEnabled);
  if (weather_enabled) {
    s_state.weather_enabled = prv_tuple_to_i32(weather_enabled) != 0 ? 1 : 0;
    loading_changed = true;
  }

  Tuple *weather_unit_fahrenheit = dict_find(iter, MESSAGE_KEY_WeatherUnitFahrenheit);
  if (weather_unit_fahrenheit) {
    s_state.weather_unit_fahrenheit = prv_tuple_to_i32(weather_unit_fahrenheit) != 0 ? 1 : 0;
    loading_changed = true;
  }

  Tuple *weather_detail_level = dict_find(iter, MESSAGE_KEY_WeatherDetailLevel);
  if (weather_detail_level) {
    s_state.weather_detail_level = prv_clamp_i32(prv_tuple_to_i32(weather_detail_level), 0, 2);
    loading_changed = true;
  }

  Tuple *weather_status = dict_find(iter, MESSAGE_KEY_WeatherStatus);
  if (weather_status) {
    s_state.weather_status = prv_clamp_i32(prv_tuple_to_i32(weather_status), 0, WEATHER_STATUS_FAILED);
    loading_changed = true;
  }

  Tuple *weather_temp_x10 = dict_find(iter, MESSAGE_KEY_WeatherTempX10);
  if (weather_temp_x10) {
    s_state.weather_temp_x10 = prv_tuple_to_i32(weather_temp_x10);
    loading_changed = true;
  }

  Tuple *weather_cloud_cover = dict_find(iter, MESSAGE_KEY_WeatherCloudCover);
  if (weather_cloud_cover) {
    s_state.weather_cloud_cover = prv_clamp_i32(prv_tuple_to_i32(weather_cloud_cover), 0, 100);
    loading_changed = true;
  }

  Tuple *weather_code = dict_find(iter, MESSAGE_KEY_WeatherCode);
  if (weather_code) {
    s_state.weather_code = prv_tuple_to_i32(weather_code);
    loading_changed = true;
  }

  Tuple *weather_wind_x10 = dict_find(iter, MESSAGE_KEY_WeatherWindX10);
  if (weather_wind_x10) {
    s_state.weather_wind_x10 = prv_tuple_to_i32(weather_wind_x10);
    loading_changed = true;
  }

  Tuple *weather_precip_x100 = dict_find(iter, MESSAGE_KEY_WeatherPrecipX100);
  if (weather_precip_x100) {
    s_state.weather_precip_x100 = prv_tuple_to_i32(weather_precip_x100);
    loading_changed = true;
  }

  Tuple *weather_updated_epoch = dict_find(iter, MESSAGE_KEY_WeatherUpdatedEpoch);
  if (weather_updated_epoch) {
    s_state.weather_updated_epoch = prv_tuple_to_i32(weather_updated_epoch);
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
    if (motion_changed) {
      prv_restart_animation_timer();
    }
    if (loading_changed) {
      layer_mark_dirty(s_canvas_layer);
    }
    return;
  }

  s_state.latitude_e6 = prv_tuple_to_i32(lat);
  s_state.longitude_e6 = prv_tuple_to_i32(lon);
  uint32_t now_ms = prv_now_ms();
  bool was_ready = s_state.had_payload_once;

  s_state.azimuth_deg_x100 = prv_tuple_to_i32(az);
  s_state.altitude_deg_x100 = prv_tuple_to_i32(alt);
  int32_t next_angle_x100 = prv_wrap_degrees_x100(prv_tuple_to_i32(angle));
  if (!was_ready || prv_effective_motion_mode() == MOTION_MODE_OFF) {
    s_state.gradient_angle_deg_x100 = next_angle_x100;
    s_state.previous_gradient_angle_deg_x100 = next_angle_x100;
    s_state.target_gradient_angle_deg_x100 = next_angle_x100;
    s_state.angle_transition_active = false;
  } else {
    s_state.previous_gradient_angle_deg_x100 = s_state.gradient_angle_deg_x100;
    s_state.target_gradient_angle_deg_x100 = next_angle_x100;
    s_state.angle_transition_started_ms = now_ms;
    s_state.angle_transition_active = true;
  }
  s_state.computed_at_epoch = prv_tuple_to_i32(computed);

  if (source) {
    s_state.source_code = prv_tuple_to_i32(source);
  }
  if (city) {
    snprintf(s_state.city_name, sizeof(s_state.city_name), "%s", city->value->cstring);
  }

  s_state.has_payload = true;
  s_state.had_payload_once = true;
  s_state.last_payload_received_ms = now_ms;
  if (was_ready && s_state.launch_done) {
    s_state.refresh_badge_until_ms = now_ms + REFRESH_BADGE_MS;
  }
  prv_set_loading_status(STATUS_READY);
  s_state.loading_progress_target = 100;
  s_state.loading_progress = 100;
  s_state.launch_transition_deadline_ms = now_ms + LOADING_TRANSITION_HOLD_MS;
  prv_start_loading_timer();
  if (motion_changed) {
    prv_restart_animation_timer();
  }
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

static void prv_animation_timer_cb(void *context) {
  (void)context;
  s_animation_timer = NULL;
  if (s_canvas_layer && s_state.launch_done) {
    layer_mark_dirty(s_canvas_layer);
  }
  prv_start_animation_timer();
}

static void prv_start_loading_timer(void) {
  if (!s_loading_timer && !s_state.launch_done) {
    s_loading_timer = app_timer_register(LOADING_TIMER_INTERVAL_MS, prv_loading_timer_cb, NULL);
  }
}

static void prv_start_animation_timer(void) {
  if (!s_animation_timer) {
    const RenderProfile *profile = s_active_profile ? s_active_profile : &s_render_profiles[0];
    uint32_t interval = prv_animation_interval_for_profile(profile);
    s_animation_timer = app_timer_register(interval, prv_animation_timer_cb, NULL);
  }
}

static void prv_restart_animation_timer(void) {
  if (s_animation_timer) {
    app_timer_cancel(s_animation_timer);
    s_animation_timer = NULL;
  }
  prv_start_animation_timer();
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
  app_message_open(768, 256);

  tick_timer_service_subscribe(MINUTE_UNIT, prv_on_minute_tick);
  bluetooth_connection_service_subscribe(prv_on_connection_event);

  prv_begin_loading();
  prv_send_refresh_request();
  prv_start_animation_timer();
}

static void prv_deinit(void) {
  if (s_loading_timer) {
    app_timer_cancel(s_loading_timer);
    s_loading_timer = NULL;
  }
  if (s_animation_timer) {
    app_timer_cancel(s_animation_timer);
    s_animation_timer = NULL;
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
