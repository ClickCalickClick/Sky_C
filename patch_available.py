import sys

with open('src/c/Sky.c', 'r') as f:
    text = f.read()

old_block = """  const char* available_texts[] = {
    "", // 0 = None
    location_text, // 1
    altitude_text, // 2
    weather_text, // 3
    solar_phase_text, // 4
    next_phase_text, // 5
    date_text, // 6
    battery_text // 7
  };"""

new_block = """  const char* available_texts[] = {
    "", // 0 = None
    date_text, // 1
    location_text, // 2
    weather_text, // 3
    altitude_text, // 4
    solar_phase_text, // 5
    next_phase_text, // 6
    battery_text // 7
  };"""

if old_block in text:
    new_text = text.replace(old_block, new_block)
    with open('src/c/Sky.c', 'w') as f:
        f.write(new_text)
    print("PATCH SUCCESSFUL")
else:
    print("OLD BLOCK NOT FOUND")
