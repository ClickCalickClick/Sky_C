with open("src/c/Sky.c", 'r') as f:
    text = f.read()

BAD_CODE = """      GColor color = prv_dithered_color(factor, dither_strength * 2.0f, dither_val, 0.0f, palette);
      graphics_context_set_fill_color(ctx, color);
      graphics_fill_rect(ctx, GRect(x, y, step, step), 0, GCornerNone);"""

GOOD_CODE = """      Rgb color;
      color.r = prv_lerp_i16(palette.top.r, palette.bottom.r, factor);
      color.g = prv_lerp_i16(palette.top.g, palette.bottom.g, factor);
      color.b = prv_lerp_i16(palette.top.b, palette.bottom.b, factor);

      graphics_context_set_fill_color(ctx, prv_make_color_rgb(color));
      graphics_fill_rect(ctx, GRect(x, y, step, step), 0, GCornerNone);"""

with open("src/c/Sky.c", 'w') as f:
    f.write(text.replace(BAD_CODE, GOOD_CODE))

