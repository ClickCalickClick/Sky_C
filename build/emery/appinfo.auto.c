#include "pebble_process_info.h"
#include "src/resource_ids.auto.h"

const PebbleProcessInfo __pbl_app_info __attribute__ ((section (".pbl_header"))) = {
  .header = "PBLAPP",
  .struct_version = { PROCESS_INFO_CURRENT_STRUCT_VERSION_MAJOR, PROCESS_INFO_CURRENT_STRUCT_VERSION_MINOR },
  .sdk_version = { PROCESS_INFO_CURRENT_SDK_VERSION_MAJOR, PROCESS_INFO_CURRENT_SDK_VERSION_MINOR },
  .process_version = { 1, 0 },
  .load_size = 0xb6b6,
  .offset = 0xb6b6b6b6,
  .crc = 0xb6b6b6b6,
  .name = "Sky",
  .company = "MakeAwesomeHappen",
  .icon_resource_id = DEFAULT_MENU_ICON,
  .sym_table_addr = 0xA7A7A7A7,
  .flags = PROCESS_INFO_WATCH_FACE | PROCESS_INFO_PLATFORM_EMERY,
  .num_reloc_entries = 0xdeadcafe,
  .uuid = { 0x08, 0x9D, 0x6F, 0x1E, 0x36, 0x1F, 0x48, 0x65, 0xBA, 0x3B, 0x32, 0x57, 0x8A, 0x39, 0x3E, 0x6F },
  .virtual_size = 0xb6b6
};
