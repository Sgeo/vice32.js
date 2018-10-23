echo compiling js...
cp src/x64 src/x64.o
emcc src/x64.o  -O2 -o ../js/x64.js -s DOUBLE_MODE=0 -s EMTERPRETIFY=1 -s EMTERPRETIFY_ASYNC=1 -s EMTERPRETIFY_WHITELIST="[ \
        '_sdl_ui_trap_top', \
        '_sdl_ui_menu_display', \
        '_attach_disk_callback', \
        '_sdl_ui_file_selection_dialog', \
        '_sdl_ui_menu_poll_input', \
        '_custom_volume_callback', \
        '_sdl_ui_slider_input_dialog', \
        '_autostart_callback', \
        '_SDL_WaitEvent', \
        '_pause_trap_top', \
        '_sdl_ui_readline', \
        '_sdl_ui_readline_input', 
        '_sdl_ui_text_input_dialog', \
        '_sdl_ui_menu_string_helper', \
        '_string_RsDevice1_callback', \
        '_string_RsDevice2_callback', \
        '_string_RsDevice3_callback', \
        '_string_RsDevice4_callback', \
        '_monitor_callback', \
        '_monitor_startup', \
        '_uimon_in', \
        '_uimon_get_in', \
        '_custom_ui_keyset_callback', \
        '_custom_keyset_callback', \
        '_sdl_ui_poll_event', \
        '_handle_message_box', \
        '_message_box', \
        '_ui_message', \
        '_show_text', \
        '/.*_callback/',
        '_dummy_entry_must_be_last' \
    ]" -s PRECISE_I64_MATH=0 -s WARN_ON_UNDEFINED_SYMBOLS=1 -s TOTAL_MEMORY=33554432 -s ALLOW_MEMORY_GROWTH=1 -s USE_SDL=1 -s WASM=1 -s SINGLE_FILE=1 \
    -s EXPORTED_FUNCTIONS="[ \
        '_autostart_autodetect', \
        '_cmdline_options_string', \
        '_file_system_attach_disk', \
        '_file_system_detach_disk', \
        '_file_system_get_disk_name', \
        '_joystick_set_value_and', \
        '_joystick_set_value_or', \
        '_keyboard_key_pressed', \
        '_keyboard_key_released', \
        '_machine_trigger_reset', \
        '_main', \
        '_set_playback_enabled' \
    ]" \
    --embed-file "data/C64@/C64" --embed-file "data/DRIVES@/DRIVES" --embed-file "data/fonts@/fonts"