// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#include <stdlib.h>
#include <string.h>

#define WM_DISPATCH_R(ret, name, params, args) \
  internal ret wm_##name params { return (lnx_wm_backend == LNX_WM_Backend_Wayland) ? wm_wl_##name args : wm_x11_##name args; }
#define WM_DISPATCH_V(name, params, args) \
  internal void wm_##name params { if(lnx_wm_backend == LNX_WM_Backend_Wayland) { wm_wl_##name args; } else { wm_x11_##name args; } }

internal void
wm_init(void)
{

  LNX_WM_Backend want = LNX_WM_Backend_Wayland;
  char *env = getenv("RADDBG_BACKEND");
  char *wl_display_name = getenv("WAYLAND_DISPLAY");
  if(env != 0 && (strcmp(env, "x11") == 0 || strcmp(env, "X11") == 0))
  {
    want = LNX_WM_Backend_X11;
  }
  else if(env != 0 && strcmp(env, "wayland") == 0)
  {
    want = LNX_WM_Backend_Wayland;
  }
  else if(wl_display_name == 0 || wl_display_name[0] == 0)
  {

    want = LNX_WM_Backend_X11;
  }

  if(want == LNX_WM_Backend_Wayland && wm_wl_init())
  {
    lnx_wm_backend = LNX_WM_Backend_Wayland;
  }
  else
  {
    lnx_wm_backend = LNX_WM_Backend_X11;
    wm_x11_init();
  }
}

WM_DISPATCH_R(WM_SystemInfo *, get_system_info, (void), ())

WM_DISPATCH_V(set_clipboard_text, (String8 string), (string))
WM_DISPATCH_R(String8, get_clipboard_text, (Arena *arena), (arena))

WM_DISPATCH_R(WM_Window, window_open, (Rng2F32 rect, WM_WindowFlags flags, String8 title), (rect, flags, title))
WM_DISPATCH_V(window_close, (WM_Window window), (window))
WM_DISPATCH_V(window_set_title, (WM_Window window, String8 title), (window, title))
WM_DISPATCH_V(window_first_paint, (WM_Window window), (window))
WM_DISPATCH_V(window_focus, (WM_Window window), (window))
WM_DISPATCH_R(B32, window_is_focused, (WM_Window window), (window))
WM_DISPATCH_R(B32, window_is_fullscreen, (WM_Window window), (window))
WM_DISPATCH_V(window_set_fullscreen, (WM_Window window, B32 fullscreen), (window, fullscreen))
WM_DISPATCH_R(B32, window_is_maximized, (WM_Window window), (window))
WM_DISPATCH_V(window_set_maximized, (WM_Window window, B32 maximized), (window, maximized))
WM_DISPATCH_R(B32, window_is_minimized, (WM_Window window), (window))
WM_DISPATCH_V(window_set_minimized, (WM_Window window, B32 minimized), (window, minimized))
WM_DISPATCH_V(window_bring_to_front, (WM_Window window), (window))
WM_DISPATCH_V(window_set_monitor, (WM_Window window, WM_Monitor monitor), (window, monitor))
WM_DISPATCH_V(window_clear_custom_border_data, (WM_Window handle), (handle))
WM_DISPATCH_V(window_push_custom_title_bar, (WM_Window handle, F32 thickness), (handle, thickness))
WM_DISPATCH_V(window_push_custom_edges, (WM_Window handle, F32 thickness), (handle, thickness))
WM_DISPATCH_V(window_push_custom_title_bar_client_area, (WM_Window handle, Rng2F32 rect), (handle, rect))
WM_DISPATCH_R(Rng2F32, rect_from_window, (WM_Window window), (window))
WM_DISPATCH_R(Rng2F32, client_rect_from_window, (WM_Window window), (window))
WM_DISPATCH_R(F32, dpi_from_window, (WM_Window window), (window))

WM_DISPATCH_R(WM_ExtWindow, focused_external_window, (void), ())
WM_DISPATCH_V(focus_external_window, (WM_ExtWindow ext_window), (ext_window))

WM_DISPATCH_R(WM_MonitorArray, push_monitors_array, (Arena *arena), (arena))
WM_DISPATCH_R(WM_Monitor, primary_monitor, (void), ())
WM_DISPATCH_R(WM_Monitor, monitor_from_window, (WM_Window window), (window))
WM_DISPATCH_R(String8, name_from_monitor, (Arena *arena, WM_Monitor monitor), (arena, monitor))
WM_DISPATCH_R(Vec2F32, dim_from_monitor, (WM_Monitor monitor), (monitor))
WM_DISPATCH_R(F32, dpi_from_monitor, (WM_Monitor monitor), (monitor))

WM_DISPATCH_V(send_wakeup_event, (void), ())
WM_DISPATCH_R(WM_EventList, get_events, (Arena *arena, B32 wait), (arena, wait))
WM_DISPATCH_R(WM_Modifiers, get_modifiers, (void), ())
WM_DISPATCH_R(B32, key_is_down, (WM_Key key), (key))
WM_DISPATCH_R(Vec2F32, mouse_from_window, (WM_Window window), (window))

WM_DISPATCH_V(set_cursor, (WM_Cursor cursor), (cursor))

WM_DISPATCH_V(graphical_message, (B32 error, String8 title, String8 message), (error, title, message))
WM_DISPATCH_R(String8, graphical_pick_file, (Arena *arena, String8 title, String8 initial_path), (arena, title, initial_path))

WM_DISPATCH_V(show_in_filesystem_ui, (String8 path), (path))
WM_DISPATCH_V(open_in_browser, (String8 url), (url))

#undef WM_DISPATCH_R
#undef WM_DISPATCH_V
