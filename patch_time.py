import sys

with open('src/c/Sky.c', 'r') as f:
    text = f.read()

old_block = """  time_t now = time(NULL);
  struct tm *time_info = localtime(&now);
  int hours = time_info->tm_hour % 12;
  if (hours == 0) {
    hours = 12;
  }

  char time_text[8];
  snprintf(time_text, sizeof(time_text), "%d:%02d", hours, time_info->tm_min);"""

new_block = """  time_t now = time(NULL);
  struct tm *time_info = localtime(&now);

  bool is_24h = clock_is_24h_style();
  if (s_state.time_format == 1) {
    is_24h = false;
  } else if (s_state.time_format == 2) {
    is_24h = true;
  }

  char time_text[8];
  if (is_24h) {
    snprintf(time_text, sizeof(time_text), "%d:%02d", time_info->tm_hour, time_info->tm_min);
  } else {
    int hours = time_info->tm_hour % 12;
    if (hours == 0) {
      hours = 12;
    }
    snprintf(time_text, sizeof(time_text), "%d:%02d", hours, time_info->tm_min);
  }"""

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
