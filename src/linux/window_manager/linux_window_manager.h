// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef LINUX_WINDOW_MANAGER_H
#define LINUX_WINDOW_MANAGER_H

////////////////////////////////
//~ rjf: Includes

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/extensions/sync.h>
#include <X11/keysym.h>
#include <X11/keysymdef.h>
#pragma push_macro("global")
#undef global
#include <wayland-client.h>
#include <wayland-egl.h>
#include <wayland-cursor.h>
#include <xkbcommon/xkbcommon.h>
#include "generated/xdg-shell-client-protocol.h"
#include "generated/xdg-decoration-client-protocol.h"
#pragma pop_macro("global")
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>

////////////////////////////////
//~ rjf: Backend Selection

typedef enum LNX_WM_Backend
{
  LNX_WM_Backend_X11,
  LNX_WM_Backend_Wayland,
}
LNX_WM_Backend;

global LNX_WM_Backend lnx_wm_backend = LNX_WM_Backend_X11;

////////////////////////////////
//~ rjf: X11 Types

#define MWM_HINTS_DECORATIONS (1L << 1)
#define MWM_DECOR_NONE 0

typedef struct LNX_MotifWMHints LNX_MotifWMHints;
struct LNX_MotifWMHints
{
  unsigned long flags;
  unsigned long functions;
  unsigned long decorations;
  long input_mode;
  unsigned long status;
};

#define LNX_WM_MOVERESIZE_SIZE_TOPLEFT     0
#define LNX_WM_MOVERESIZE_SIZE_TOP         1
#define LNX_WM_MOVERESIZE_SIZE_TOPRIGHT    2
#define LNX_WM_MOVERESIZE_SIZE_RIGHT       3
#define LNX_WM_MOVERESIZE_SIZE_BOTTOMRIGHT 4
#define LNX_WM_MOVERESIZE_SIZE_BOTTOM      5
#define LNX_WM_MOVERESIZE_SIZE_BOTTOMLEFT  6
#define LNX_WM_MOVERESIZE_SIZE_LEFT        7
#define LNX_WM_MOVERESIZE_MOVE             8
#define LNX_WM_MOVERESIZE_NONE             (-1)

#define LNX_WM_MAX_TITLE_BAR_CLIENT_AREAS 256

typedef struct LNX_WM_Window LNX_WM_Window;
struct LNX_WM_Window
{
  LNX_WM_Window *next;
  LNX_WM_Window *prev;
  Window window;
  XIC xic;
  XID counter_xid;
  U64 counter_value;
  B32 sync_request_pending;
  XID extended_counter_xid;
  S64 extended_counter_value;
  B32 extended_frame_pending;

  F32 custom_border_title_thickness;
  F32 custom_border_edge_thickness;
  U64 title_bar_client_area_count;
  Rng2F32 title_bar_client_areas[LNX_WM_MAX_TITLE_BAR_CLIENT_AREAS];
};

typedef struct LNX_WM_State LNX_WM_State;
struct LNX_WM_State
{
  Arena *arena;
  Display *display;
  XIM xim;
  LNX_WM_Window *first_window;
  LNX_WM_Window *last_window;
  LNX_WM_Window *free_window;
  Atom wm_delete_window_atom;
  Atom wm_sync_request_atom;
  Atom wm_sync_request_counter_atom;
  Atom motif_wm_hints_atom;
  Atom net_wm_moveresize_atom;
  Cursor cursors[WM_Cursor_COUNT];
  WM_Cursor last_set_cursor;
  WM_SystemInfo gfx_info;
  int wakeup_fd;
  Visual *window_visual;
  int window_depth;
  Colormap window_colormap;
};

global LNX_WM_State *lnx_wm_state = 0;

internal LNX_WM_Window *lnx_window_from_x11window(Window window);
internal void lnx_window_finish_frame_sync(WM_Window handle);

////////////////////////////////
//~ rjf: Wayland Types

#define WL_WM_MAX_TITLE_BAR_CLIENT_AREAS 256

typedef struct WL_WM_Window WL_WM_Window;
struct WL_WM_Window
{
  WL_WM_Window *next;
  WL_WM_Window *prev;

  struct wl_surface *surface;
  struct xdg_surface *xdg_surface;
  struct xdg_toplevel *xdg_toplevel;
  struct zxdg_toplevel_decoration_v1 *decoration;
  struct wl_egl_window *egl_window;

  Vec2S32 size;
  Vec2S32 pending_size;
  B32 configured;
  B32 wants_close;
  B32 fullscreen;
  B32 maximized;
  B32 activated;

  F32 custom_border_title_thickness;
  F32 custom_border_edge_thickness;
  U64 title_bar_client_area_count;
  Rng2F32 title_bar_client_areas[WL_WM_MAX_TITLE_BAR_CLIENT_AREAS];
};

typedef struct WL_WM_State WL_WM_State;
struct WL_WM_State
{
  Arena *arena;

  struct wl_display *display;
  struct wl_registry *registry;
  struct wl_compositor *compositor;
  struct wl_shm *shm;
  struct xdg_wm_base *wm_base;
  struct zxdg_decoration_manager_v1 *decoration_manager;
  struct wl_seat *seat;
  struct wl_pointer *pointer;
  struct wl_keyboard *keyboard;

  struct wl_cursor_theme *cursor_theme;
  struct wl_surface *cursor_surface;
  WM_Cursor last_set_cursor;
  WM_Cursor applied_cursor;
  U32 pointer_enter_serial;

  struct xkb_context *xkb_context;
  struct xkb_keymap *xkb_keymap;
  struct xkb_state *xkb_state;
  WM_Modifiers modifiers;

  int repeat_fd;
  S32 repeat_rate;
  S32 repeat_delay_ms;
  U32 repeat_key;
  WM_Key repeat_wm_key;
  U32 repeat_codepoint;

  WL_WM_Window *pointer_focus;
  WL_WM_Window *keyboard_focus;
  Vec2F32 pointer_pos;
  U32 last_serial;
  U32 last_pointer_button_serial;

  Arena *evt_arena;
  WM_EventList *evts;

  WL_WM_Window *first_window;
  WL_WM_Window *last_window;
  WL_WM_Window *free_window;

  WM_SystemInfo gfx_info;
  int wakeup_fd;
};

global WL_WM_State *wl_wm_state = 0;

internal WL_WM_Window *wl_window_from_surface(struct wl_surface *surface);

#endif // LINUX_WINDOW_MANAGER_H
