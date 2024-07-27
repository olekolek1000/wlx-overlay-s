#pragma once

#define EXPORT_SYMBOL extern "C" __attribute__((visibility("default")))

EXPORT_SYMBOL const char *wlxcef_get_error();
EXPORT_SYMBOL int wlxcef_init();
EXPORT_SYMBOL int wlxcef_free();

/// @returns Tab ID (-1 on error)
EXPORT_SYMBOL int wlxcef_create_tab();

EXPORT_SYMBOL void wlxcef_tick_message_loop();

/// @returns -1 on failure, 0 on success
EXPORT_SYMBOL int wlxcef_free_tab(int handler_id);

/// @returns viewport width in pixels
EXPORT_SYMBOL int wlxcef_get_viewport_width(int handler_id);

/// @returns viewport height in pixels
EXPORT_SYMBOL int wlxcef_get_viewport_height(int handler_id);

/// @returns Packed RGBA8888 data. Size: viewport_width * viewport_height * 4
EXPORT_SYMBOL const void *wlxcef_get_viewport_data_rgba(int handler_id);

EXPORT_SYMBOL int wlxcef_mouse_move(int handler_id, int x, int y);

EXPORT_SYMBOL int wlxcef_mouse_set_state(int handler_id, int index, int down);

EXPORT_SYMBOL int wlxcef_mouse_scroll(int handler_id, float delta);