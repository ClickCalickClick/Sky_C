import sys

with open('src/c/Sky.c', 'r') as f:
    text = f.read()

# 1. Fix AppState
old_appstate = """  int32_t show_location;
  int32_t show_altitude;
  int32_t weather_enabled;
  int32_t weather_unit_fahrenheit;
  int32_t weather_detail_level;"""
new_appstate = """  int32_t footer_slot_1;
  int32_t footer_slot_2;
  int32_t footer_slot_3;
  int32_t footer_slot_4;
  int32_t footer_slot_5;
  int32_t footer_slot_6;
  int32_t weather_enabled;
  int32_t weather_unit_fahrenheit;
  int32_t weather_detail_level;"""
if old_appstate in text: text = text.replace(old_appstate, new_appstate)
else: print("ERROR 1")

# 2. Fix AppState show_solar_phase
old2 = """  char city_name[48];
  int32_t show_solar_phase;
  int32_t show_next_phase_countdown;
  int32_t current_solar_phase_id;
  int32_t next_solar_phase_id;
  int32_t next_solar_phase_epoch;
  int32_t time_format;
  int32_t show_date;
  int32_t show_battery;
} AppState;"""
new2 = """  char city_name[48];
  int32_t current_solar_phase_id;
  int32_t next_solar_phase_id;
  int32_t next_solar_phase_epoch;
  int32_t time_format;
  bool phase_refresh_requested;
} AppState;"""
if old2 in text: text = text.replace(old2, new2)
else: print("ERROR 2")

# 3. Fix AppState init defaults
old_defaults = """  .show_location = 1,
  .show_altitude = 1,
  .weather_enabled = 1,"""
new_defaults = """  .footer_slot_1 = 1,
  .footer_slot_2 = 2,
  .footer_slot_3 = 0,
  .footer_slot_4 = 0,
  .footer_slot_5 = 0,
  .footer_slot_6 = 0,
  .weather_enabled = 1,"""
if old_defaults in text: text = text.replace(old_defaults, new_defaults)
else: print("ERROR 3")

# 4. Fix AppState init defaults bottom
old3 = """  .loading_hint_mode = 0,
  .city_name = "Chicago",
  .time_format = 0,
  .show_date = 1,
  .show_battery = 0,
};"""
new3 = """  .loading_hint_mode = 0,
  .city_name = "Chicago",
  .time_format = 0,
};"""
if old3 in text: text = text.replace(old3, new3)
else: print("ERROR 4")

# 5. Fix Message reading block
old_msg = """  Tuple *show_solar_phase = dict_find(iter, MESSAGE_KEY_ShowSolarPhase);
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
"""
new_msg = """  Tuple *footer_slot_3 = dict_find(iter, MESSAGE_KEY_FooterSlot3);
  if (footer_slot_3) {
    s_state.footer_slot_3 = prv_clamp_i32(prv_tuple_to_i32(footer_slot_3), 0, 7);
    loading_changed = true;
  }

  Tuple *footer_slot_4 = dict_find(iter, MESSAGE_KEY_FooterSlot4);
  if (footer_slot_4) {
    s_state.footer_slot_4 = prv_clamp_i32(prv_tuple_to_i32(footer_slot_4), 0, 7);
    loading_changed = true;
  }

  Tuple *current_solar_phase_id = dict_find(iter, MESSAGE_KEY_CurrentSolarPhaseId);
"""
if old_msg in text: text = text.replace(old_msg, new_msg)
else: print("ERROR 5")

# 6. Fix Message reading block 2
old_msg2 = """  Tuple *next_solar_phase_epoch = dict_find(iter, MESSAGE_KEY_NextSolarPhaseEpoch);
  if (next_solar_phase_epoch) {
    s_state.next_solar_phase_epoch = prv_tuple_to_i32(next_solar_phase_epoch);
    loading_changed = true;
  }
"""
new_msg2 = """  Tuple *next_solar_phase_epoch = dict_find(iter, MESSAGE_KEY_NextSolarPhaseEpoch);
  if (next_solar_phase_epoch) {
    s_state.next_solar_phase_epoch = prv_tuple_to_i32(next_solar_phase_epoch);
    s_state.phase_refresh_requested = false;
    loading_changed = true;
  }
"""
if old_msg2 in text: text = text.replace(old_msg2, new_msg2)
else: print("ERROR 6")

# 7. Fix Message reading block 3
old_msg3 = """  Tuple *show_location = dict_find(iter, MESSAGE_KEY_ShowLocation);
  if (show_location) {
    s_state.show_location = prv_tuple_to_i32(show_location) != 0 ? 1 : 0;
    loading_changed = true;
  }

  Tuple *show_altitude = dict_find(iter, MESSAGE_KEY_ShowAltitude);
  if (show_altitude) {
    s_state.show_altitude = prv_tuple_to_i32(show_altitude) != 0 ? 1 : 0;
    loading_changed = true;
  }"""
new_msg3 = """  Tuple *footer_slot_5 = dict_find(iter, MESSAGE_KEY_FooterSlot5);
  if (footer_slot_5) {
    s_state.footer_slot_5 = prv_clamp_i32(prv_tuple_to_i32(footer_slot_5), 0, 7);
    loading_changed = true;
  }

  Tuple *footer_slot_6 = dict_find(iter, MESSAGE_KEY_FooterSlot6);
  if (footer_slot_6) {
    s_state.footer_slot_6 = prv_clamp_i32(prv_tuple_to_i32(footer_slot_6), 0, 7);
    loading_changed = true;
  }"""
if old_msg3 in text: text = text.replace(old_msg3, new_msg3)
else: print("ERROR 7")


# 8. Fix missing reading blocks for slot 1 & 2 before text_override_mode ? Or I can just put them before TimeSizeEmery ... no wait, let's just put all 6.
def add_slot_1_2():
    global text
    search = """  Tuple *time_format = dict_find(iter, MESSAGE_KEY_TimeFormat);
  if (time_format) {
    s_state.time_format = prv_clamp_i32(prv_tuple_to_i32(time_format), 0, 2);
    loading_changed = true;
  }"""
    rep = search + """\n\n  Tuple *footer_slot_1 = dict_find(iter, MESSAGE_KEY_FooterSlot1);
  if (footer_slot_1) {
    s_state.footer_slot_1 = prv_clamp_i32(prv_tuple_to_i32(footer_slot_1), 0, 7);
    loading_changed = true;
  }

  Tuple *footer_slot_2 = dict_find(iter, MESSAGE_KEY_FooterSlot2);
  if (footer_slot_2) {
    s_state.footer_slot_2 = prv_clamp_i32(prv_tuple_to_i32(footer_slot_2), 0, 7);
    loading_changed = true;
  }"""
    if search in text:
        text = text.replace(search, rep)
    else:
        print("ERROR 8")

add_slot_1_2()

# And delete old ShowDate / ShowBattery handlers that the compiler caught
def remove_show_date():
    global text
    rep = """  Tuple *show_date = dict_find(iter, MESSAGE_KEY_ShowDate);
  if (show_date) {
    s_state.show_date = prv_tuple_to_i32(show_date) != 0 ? 1 : 0;
    loading_changed = true;
  }

  Tuple *show_battery = dict_find(iter, MESSAGE_KEY_ShowBattery);
  if (show_battery) {
    s_state.show_battery = prv_tuple_to_i32(show_battery) != 0 ? 1 : 0;
    loading_changed = true;
  }"""
    if rep in text:
        text = text.replace(rep, "")
    else:
        print("ERROR 9")
remove_show_date()


with open('src/c/Sky.c', 'w') as f:
    f.write(text)
print("DONE")
