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
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>

////////////////////////////////
//~ rjf: Wayland Client Bindings
//
// Hand-declared subset of libwayland-client / libwayland-egl / libwayland-cursor
// / libxkbcommon / xdg-shell / xdg-decoration - just the parts the Wayland backend
// uses, so we do not pull in the system headers or generated protocol code. The
// libraries are still linked; the xdg protocol wire-data is defined in
// wayland_window_manager.c. `global` is undefined here because wl_registry_listener
// has a member named `global`.

#pragma push_macro("global")
#undef global

typedef int32_t wl_fixed_t;

struct wl_message { const char *name; const char *signature; const struct wl_interface **types; };
struct wl_interface { const char *name; int version; int method_count; const struct wl_message *methods; int event_count; const struct wl_message *events; };
struct wl_array { size_t size; size_t alloc; void *data; };

struct wl_proxy; struct wl_display; struct wl_registry; struct wl_compositor;
struct wl_surface; struct wl_seat; struct wl_pointer; struct wl_keyboard;
struct wl_shm; struct wl_region; struct wl_buffer; struct wl_output; struct wl_callback;

extern struct wl_proxy *wl_proxy_marshal_flags(struct wl_proxy *proxy, uint32_t opcode, const struct wl_interface *interface, uint32_t version, uint32_t flags, ...);
extern int wl_proxy_add_listener(struct wl_proxy *proxy, void (**implementation)(void), void *data);
extern uint32_t wl_proxy_get_version(struct wl_proxy *proxy);
extern struct wl_display *wl_display_connect(const char *name);
extern void wl_display_disconnect(struct wl_display *display);
extern int wl_display_roundtrip(struct wl_display *display);
extern int wl_display_dispatch_pending(struct wl_display *display);
extern int wl_display_flush(struct wl_display *display);
extern int wl_display_get_fd(struct wl_display *display);
extern int wl_display_prepare_read(struct wl_display *display);
extern int wl_display_read_events(struct wl_display *display);
extern void wl_display_cancel_read(struct wl_display *display);

#define WL_MARSHAL_FLAG_DESTROY (1 << 0)

static inline double wl_fixed_to_double(wl_fixed_t f) { return f / 256.0; }

extern const struct wl_interface wl_registry_interface;
extern const struct wl_interface wl_compositor_interface;
extern const struct wl_interface wl_surface_interface;
extern const struct wl_interface wl_seat_interface;
extern const struct wl_interface wl_pointer_interface;
extern const struct wl_interface wl_keyboard_interface;
extern const struct wl_interface wl_region_interface;
extern const struct wl_interface wl_shm_interface;
extern const struct wl_interface wl_output_interface;

#define WL_DISPLAY_GET_REGISTRY 1
#define WL_REGISTRY_BIND 0
#define WL_COMPOSITOR_CREATE_SURFACE 0
#define WL_COMPOSITOR_CREATE_REGION 1
#define WL_SURFACE_DESTROY 0
#define WL_SURFACE_ATTACH 1
#define WL_SURFACE_DAMAGE 2
#define WL_SURFACE_SET_OPAQUE_REGION 4
#define WL_SURFACE_COMMIT 6
#define WL_REGION_DESTROY 0
#define WL_REGION_ADD 1
#define WL_SEAT_GET_POINTER 0
#define WL_SEAT_GET_KEYBOARD 1
#define WL_POINTER_SET_CURSOR 0

enum { WL_SEAT_CAPABILITY_POINTER = 1, WL_SEAT_CAPABILITY_KEYBOARD = 2 };
enum { WL_POINTER_BUTTON_STATE_PRESSED = 1 };
enum { WL_POINTER_AXIS_HORIZONTAL_SCROLL = 1 };
enum { WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 = 1 };
enum { WL_KEYBOARD_KEY_STATE_PRESSED = 1 };

struct wl_registry_listener
{
  void (*global)(void *data, struct wl_registry *wl_registry, uint32_t name, const char *interface, uint32_t version);
  void (*global_remove)(void *data, struct wl_registry *wl_registry, uint32_t name);
};
struct wl_seat_listener
{
  void (*capabilities)(void *data, struct wl_seat *wl_seat, uint32_t capabilities);
  void (*name)(void *data, struct wl_seat *wl_seat, const char *name);
};
struct wl_pointer_listener
{
  void (*enter)(void *data, struct wl_pointer *wl_pointer, uint32_t serial, struct wl_surface *surface, wl_fixed_t surface_x, wl_fixed_t surface_y);
  void (*leave)(void *data, struct wl_pointer *wl_pointer, uint32_t serial, struct wl_surface *surface);
  void (*motion)(void *data, struct wl_pointer *wl_pointer, uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y);
  void (*button)(void *data, struct wl_pointer *wl_pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state);
  void (*axis)(void *data, struct wl_pointer *wl_pointer, uint32_t time, uint32_t axis, wl_fixed_t value);
  void (*frame)(void *data, struct wl_pointer *wl_pointer);
  void (*axis_source)(void *data, struct wl_pointer *wl_pointer, uint32_t axis_source);
  void (*axis_stop)(void *data, struct wl_pointer *wl_pointer, uint32_t time, uint32_t axis);
  void (*axis_discrete)(void *data, struct wl_pointer *wl_pointer, uint32_t axis, int32_t discrete);
  void (*axis_value120)(void *data, struct wl_pointer *wl_pointer, uint32_t axis, int32_t value120);
  void (*axis_relative_direction)(void *data, struct wl_pointer *wl_pointer, uint32_t axis, uint32_t direction);
};
struct wl_keyboard_listener
{
  void (*keymap)(void *data, struct wl_keyboard *wl_keyboard, uint32_t format, int32_t fd, uint32_t size);
  void (*enter)(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, struct wl_surface *surface, struct wl_array *keys);
  void (*leave)(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, struct wl_surface *surface);
  void (*key)(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state);
  void (*modifiers)(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group);
  void (*repeat_info)(void *data, struct wl_keyboard *wl_keyboard, int32_t rate, int32_t delay);
};

static inline struct wl_registry *wl_display_get_registry(struct wl_display *wl_display)
{ return (struct wl_registry *)wl_proxy_marshal_flags((struct wl_proxy *)wl_display, WL_DISPLAY_GET_REGISTRY, &wl_registry_interface, wl_proxy_get_version((struct wl_proxy *)wl_display), 0, NULL); }
static inline void *wl_registry_bind(struct wl_registry *wl_registry, uint32_t name, const struct wl_interface *interface, uint32_t version)
{ return (void *)wl_proxy_marshal_flags((struct wl_proxy *)wl_registry, WL_REGISTRY_BIND, interface, version, 0, name, interface->name, version, NULL); }
static inline int wl_registry_add_listener(struct wl_registry *wl_registry, const struct wl_registry_listener *listener, void *data)
{ return wl_proxy_add_listener((struct wl_proxy *)wl_registry, (void (**)(void))listener, data); }
static inline struct wl_surface *wl_compositor_create_surface(struct wl_compositor *wl_compositor)
{ return (struct wl_surface *)wl_proxy_marshal_flags((struct wl_proxy *)wl_compositor, WL_COMPOSITOR_CREATE_SURFACE, &wl_surface_interface, wl_proxy_get_version((struct wl_proxy *)wl_compositor), 0, NULL); }
static inline struct wl_region *wl_compositor_create_region(struct wl_compositor *wl_compositor)
{ return (struct wl_region *)wl_proxy_marshal_flags((struct wl_proxy *)wl_compositor, WL_COMPOSITOR_CREATE_REGION, &wl_region_interface, wl_proxy_get_version((struct wl_proxy *)wl_compositor), 0, NULL); }
static inline void wl_surface_destroy(struct wl_surface *wl_surface)
{ wl_proxy_marshal_flags((struct wl_proxy *)wl_surface, WL_SURFACE_DESTROY, NULL, wl_proxy_get_version((struct wl_proxy *)wl_surface), WL_MARSHAL_FLAG_DESTROY); }
static inline void wl_surface_attach(struct wl_surface *wl_surface, struct wl_buffer *buffer, int32_t x, int32_t y)
{ wl_proxy_marshal_flags((struct wl_proxy *)wl_surface, WL_SURFACE_ATTACH, NULL, wl_proxy_get_version((struct wl_proxy *)wl_surface), 0, buffer, x, y); }
static inline void wl_surface_damage(struct wl_surface *wl_surface, int32_t x, int32_t y, int32_t width, int32_t height)
{ wl_proxy_marshal_flags((struct wl_proxy *)wl_surface, WL_SURFACE_DAMAGE, NULL, wl_proxy_get_version((struct wl_proxy *)wl_surface), 0, x, y, width, height); }
static inline void wl_surface_set_opaque_region(struct wl_surface *wl_surface, struct wl_region *region)
{ wl_proxy_marshal_flags((struct wl_proxy *)wl_surface, WL_SURFACE_SET_OPAQUE_REGION, NULL, wl_proxy_get_version((struct wl_proxy *)wl_surface), 0, region); }
static inline void wl_surface_commit(struct wl_surface *wl_surface)
{ wl_proxy_marshal_flags((struct wl_proxy *)wl_surface, WL_SURFACE_COMMIT, NULL, wl_proxy_get_version((struct wl_proxy *)wl_surface), 0); }
static inline void wl_region_add(struct wl_region *wl_region, int32_t x, int32_t y, int32_t width, int32_t height)
{ wl_proxy_marshal_flags((struct wl_proxy *)wl_region, WL_REGION_ADD, NULL, wl_proxy_get_version((struct wl_proxy *)wl_region), 0, x, y, width, height); }
static inline void wl_region_destroy(struct wl_region *wl_region)
{ wl_proxy_marshal_flags((struct wl_proxy *)wl_region, WL_REGION_DESTROY, NULL, wl_proxy_get_version((struct wl_proxy *)wl_region), WL_MARSHAL_FLAG_DESTROY); }
static inline int wl_seat_add_listener(struct wl_seat *wl_seat, const struct wl_seat_listener *listener, void *data)
{ return wl_proxy_add_listener((struct wl_proxy *)wl_seat, (void (**)(void))listener, data); }
static inline struct wl_pointer *wl_seat_get_pointer(struct wl_seat *wl_seat)
{ return (struct wl_pointer *)wl_proxy_marshal_flags((struct wl_proxy *)wl_seat, WL_SEAT_GET_POINTER, &wl_pointer_interface, wl_proxy_get_version((struct wl_proxy *)wl_seat), 0, NULL); }
static inline struct wl_keyboard *wl_seat_get_keyboard(struct wl_seat *wl_seat)
{ return (struct wl_keyboard *)wl_proxy_marshal_flags((struct wl_proxy *)wl_seat, WL_SEAT_GET_KEYBOARD, &wl_keyboard_interface, wl_proxy_get_version((struct wl_proxy *)wl_seat), 0, NULL); }
static inline int wl_pointer_add_listener(struct wl_pointer *wl_pointer, const struct wl_pointer_listener *listener, void *data)
{ return wl_proxy_add_listener((struct wl_proxy *)wl_pointer, (void (**)(void))listener, data); }
static inline void wl_pointer_set_cursor(struct wl_pointer *wl_pointer, uint32_t serial, struct wl_surface *surface, int32_t hotspot_x, int32_t hotspot_y)
{ wl_proxy_marshal_flags((struct wl_proxy *)wl_pointer, WL_POINTER_SET_CURSOR, NULL, wl_proxy_get_version((struct wl_proxy *)wl_pointer), 0, serial, surface, hotspot_x, hotspot_y); }
static inline int wl_keyboard_add_listener(struct wl_keyboard *wl_keyboard, const struct wl_keyboard_listener *listener, void *data)
{ return wl_proxy_add_listener((struct wl_proxy *)wl_keyboard, (void (**)(void))listener, data); }

// rjf: libwayland-egl
struct wl_egl_window;
extern struct wl_egl_window *wl_egl_window_create(struct wl_surface *surface, int width, int height);
extern void wl_egl_window_destroy(struct wl_egl_window *egl_window);
extern void wl_egl_window_resize(struct wl_egl_window *egl_window, int width, int height, int dx, int dy);

// rjf: libwayland-cursor
struct wl_cursor_theme;
struct wl_cursor_image { uint32_t width; uint32_t height; uint32_t hotspot_x; uint32_t hotspot_y; uint32_t delay; };
struct wl_cursor { unsigned int image_count; struct wl_cursor_image **images; char *name; };
extern struct wl_cursor_theme *wl_cursor_theme_load(const char *name, int size, struct wl_shm *shm);
extern struct wl_cursor *wl_cursor_theme_get_cursor(struct wl_cursor_theme *theme, const char *name);
extern struct wl_buffer *wl_cursor_image_get_buffer(struct wl_cursor_image *image);

// rjf: libxkbcommon
typedef uint32_t xkb_keycode_t;
typedef uint32_t xkb_keysym_t;
typedef uint32_t xkb_mod_mask_t;
typedef uint32_t xkb_layout_index_t;
struct xkb_context; struct xkb_keymap; struct xkb_state;
enum { XKB_CONTEXT_NO_FLAGS = 0 };
enum { XKB_KEYMAP_FORMAT_TEXT_V1 = 1 };
enum { XKB_KEYMAP_COMPILE_NO_FLAGS = 0 };
enum { XKB_STATE_MODS_EFFECTIVE = (1 << 3) };
#define XKB_MOD_NAME_SHIFT "Shift"
#define XKB_MOD_NAME_CTRL  "Control"
#define XKB_MOD_NAME_ALT   "Mod1"
extern struct xkb_context *xkb_context_new(int flags);
extern struct xkb_keymap *xkb_keymap_new_from_string(struct xkb_context *context, const char *string, int format, int flags);
extern void xkb_keymap_unref(struct xkb_keymap *keymap);
extern int xkb_keymap_key_repeats(struct xkb_keymap *keymap, xkb_keycode_t key);
extern struct xkb_state *xkb_state_new(struct xkb_keymap *keymap);
extern void xkb_state_unref(struct xkb_state *state);
extern xkb_keysym_t xkb_state_key_get_one_sym(struct xkb_state *state, xkb_keycode_t key);
extern int xkb_state_key_get_utf8(struct xkb_state *state, xkb_keycode_t key, char *buffer, size_t size);
extern int xkb_state_update_mask(struct xkb_state *state, xkb_mod_mask_t depressed_mods, xkb_mod_mask_t latched_mods, xkb_mod_mask_t locked_mods, xkb_layout_index_t depressed_layout, xkb_layout_index_t latched_layout, xkb_layout_index_t locked_layout);
extern int xkb_state_mod_name_is_active(struct xkb_state *state, const char *name, int type);

// rjf: xdg-shell (wire-data defined in wayland_window_manager.c)
struct xdg_wm_base; struct xdg_surface; struct xdg_toplevel;
extern const struct wl_interface xdg_wm_base_interface;
extern const struct wl_interface xdg_surface_interface;
extern const struct wl_interface xdg_toplevel_interface;
#define XDG_WM_BASE_GET_XDG_SURFACE 2
#define XDG_WM_BASE_PONG 3
#define XDG_SURFACE_DESTROY 0
#define XDG_SURFACE_GET_TOPLEVEL 1
#define XDG_SURFACE_ACK_CONFIGURE 4
#define XDG_TOPLEVEL_DESTROY 0
#define XDG_TOPLEVEL_SET_TITLE 2
#define XDG_TOPLEVEL_SET_APP_ID 3
#define XDG_TOPLEVEL_MOVE 5
#define XDG_TOPLEVEL_RESIZE 6
#define XDG_TOPLEVEL_SET_MAXIMIZED 9
#define XDG_TOPLEVEL_UNSET_MAXIMIZED 10
#define XDG_TOPLEVEL_SET_FULLSCREEN 11
#define XDG_TOPLEVEL_UNSET_FULLSCREEN 12
#define XDG_TOPLEVEL_SET_MINIMIZED 13
enum xdg_toplevel_resize_edge
{
  XDG_TOPLEVEL_RESIZE_EDGE_NONE = 0,
  XDG_TOPLEVEL_RESIZE_EDGE_TOP = 1,
  XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM = 2,
  XDG_TOPLEVEL_RESIZE_EDGE_LEFT = 4,
  XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT = 5,
  XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_LEFT = 6,
  XDG_TOPLEVEL_RESIZE_EDGE_RIGHT = 8,
  XDG_TOPLEVEL_RESIZE_EDGE_TOP_RIGHT = 9,
  XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT = 10,
};
enum xdg_toplevel_state
{
  XDG_TOPLEVEL_STATE_MAXIMIZED = 1,
  XDG_TOPLEVEL_STATE_FULLSCREEN = 2,
  XDG_TOPLEVEL_STATE_ACTIVATED = 4,
};
struct xdg_wm_base_listener { void (*ping)(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial); };
struct xdg_surface_listener { void (*configure)(void *data, struct xdg_surface *xdg_surface, uint32_t serial); };
struct xdg_toplevel_listener
{
  void (*configure)(void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height, struct wl_array *states);
  void (*close)(void *data, struct xdg_toplevel *xdg_toplevel);
  void (*configure_bounds)(void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height);
  void (*wm_capabilities)(void *data, struct xdg_toplevel *xdg_toplevel, struct wl_array *capabilities);
};
static inline int xdg_wm_base_add_listener(struct xdg_wm_base *xdg_wm_base, const struct xdg_wm_base_listener *listener, void *data)
{ return wl_proxy_add_listener((struct wl_proxy *)xdg_wm_base, (void (**)(void))listener, data); }
static inline struct xdg_surface *xdg_wm_base_get_xdg_surface(struct xdg_wm_base *xdg_wm_base, struct wl_surface *surface)
{ return (struct xdg_surface *)wl_proxy_marshal_flags((struct wl_proxy *)xdg_wm_base, XDG_WM_BASE_GET_XDG_SURFACE, &xdg_surface_interface, wl_proxy_get_version((struct wl_proxy *)xdg_wm_base), 0, NULL, surface); }
static inline void xdg_wm_base_pong(struct xdg_wm_base *xdg_wm_base, uint32_t serial)
{ wl_proxy_marshal_flags((struct wl_proxy *)xdg_wm_base, XDG_WM_BASE_PONG, NULL, wl_proxy_get_version((struct wl_proxy *)xdg_wm_base), 0, serial); }
static inline int xdg_surface_add_listener(struct xdg_surface *xdg_surface, const struct xdg_surface_listener *listener, void *data)
{ return wl_proxy_add_listener((struct wl_proxy *)xdg_surface, (void (**)(void))listener, data); }
static inline struct xdg_toplevel *xdg_surface_get_toplevel(struct xdg_surface *xdg_surface)
{ return (struct xdg_toplevel *)wl_proxy_marshal_flags((struct wl_proxy *)xdg_surface, XDG_SURFACE_GET_TOPLEVEL, &xdg_toplevel_interface, wl_proxy_get_version((struct wl_proxy *)xdg_surface), 0, NULL); }
static inline void xdg_surface_ack_configure(struct xdg_surface *xdg_surface, uint32_t serial)
{ wl_proxy_marshal_flags((struct wl_proxy *)xdg_surface, XDG_SURFACE_ACK_CONFIGURE, NULL, wl_proxy_get_version((struct wl_proxy *)xdg_surface), 0, serial); }
static inline void xdg_surface_destroy(struct xdg_surface *xdg_surface)
{ wl_proxy_marshal_flags((struct wl_proxy *)xdg_surface, XDG_SURFACE_DESTROY, NULL, wl_proxy_get_version((struct wl_proxy *)xdg_surface), WL_MARSHAL_FLAG_DESTROY); }
static inline int xdg_toplevel_add_listener(struct xdg_toplevel *xdg_toplevel, const struct xdg_toplevel_listener *listener, void *data)
{ return wl_proxy_add_listener((struct wl_proxy *)xdg_toplevel, (void (**)(void))listener, data); }
static inline void xdg_toplevel_destroy(struct xdg_toplevel *xdg_toplevel)
{ wl_proxy_marshal_flags((struct wl_proxy *)xdg_toplevel, XDG_TOPLEVEL_DESTROY, NULL, wl_proxy_get_version((struct wl_proxy *)xdg_toplevel), WL_MARSHAL_FLAG_DESTROY); }
static inline void xdg_toplevel_set_title(struct xdg_toplevel *xdg_toplevel, const char *title)
{ wl_proxy_marshal_flags((struct wl_proxy *)xdg_toplevel, XDG_TOPLEVEL_SET_TITLE, NULL, wl_proxy_get_version((struct wl_proxy *)xdg_toplevel), 0, title); }
static inline void xdg_toplevel_set_app_id(struct xdg_toplevel *xdg_toplevel, const char *app_id)
{ wl_proxy_marshal_flags((struct wl_proxy *)xdg_toplevel, XDG_TOPLEVEL_SET_APP_ID, NULL, wl_proxy_get_version((struct wl_proxy *)xdg_toplevel), 0, app_id); }
static inline void xdg_toplevel_move(struct xdg_toplevel *xdg_toplevel, struct wl_seat *seat, uint32_t serial)
{ wl_proxy_marshal_flags((struct wl_proxy *)xdg_toplevel, XDG_TOPLEVEL_MOVE, NULL, wl_proxy_get_version((struct wl_proxy *)xdg_toplevel), 0, seat, serial); }
static inline void xdg_toplevel_resize(struct xdg_toplevel *xdg_toplevel, struct wl_seat *seat, uint32_t serial, uint32_t edges)
{ wl_proxy_marshal_flags((struct wl_proxy *)xdg_toplevel, XDG_TOPLEVEL_RESIZE, NULL, wl_proxy_get_version((struct wl_proxy *)xdg_toplevel), 0, seat, serial, edges); }
static inline void xdg_toplevel_set_maximized(struct xdg_toplevel *xdg_toplevel)
{ wl_proxy_marshal_flags((struct wl_proxy *)xdg_toplevel, XDG_TOPLEVEL_SET_MAXIMIZED, NULL, wl_proxy_get_version((struct wl_proxy *)xdg_toplevel), 0); }
static inline void xdg_toplevel_unset_maximized(struct xdg_toplevel *xdg_toplevel)
{ wl_proxy_marshal_flags((struct wl_proxy *)xdg_toplevel, XDG_TOPLEVEL_UNSET_MAXIMIZED, NULL, wl_proxy_get_version((struct wl_proxy *)xdg_toplevel), 0); }
static inline void xdg_toplevel_set_fullscreen(struct xdg_toplevel *xdg_toplevel, struct wl_output *output)
{ wl_proxy_marshal_flags((struct wl_proxy *)xdg_toplevel, XDG_TOPLEVEL_SET_FULLSCREEN, NULL, wl_proxy_get_version((struct wl_proxy *)xdg_toplevel), 0, output); }
static inline void xdg_toplevel_unset_fullscreen(struct xdg_toplevel *xdg_toplevel)
{ wl_proxy_marshal_flags((struct wl_proxy *)xdg_toplevel, XDG_TOPLEVEL_UNSET_FULLSCREEN, NULL, wl_proxy_get_version((struct wl_proxy *)xdg_toplevel), 0); }
static inline void xdg_toplevel_set_minimized(struct xdg_toplevel *xdg_toplevel)
{ wl_proxy_marshal_flags((struct wl_proxy *)xdg_toplevel, XDG_TOPLEVEL_SET_MINIMIZED, NULL, wl_proxy_get_version((struct wl_proxy *)xdg_toplevel), 0); }

// rjf: xdg-decoration (wire-data defined in wayland_window_manager.c)
struct zxdg_decoration_manager_v1; struct zxdg_toplevel_decoration_v1;
extern const struct wl_interface zxdg_decoration_manager_v1_interface;
extern const struct wl_interface zxdg_toplevel_decoration_v1_interface;
#define ZXDG_DECORATION_MANAGER_V1_GET_TOPLEVEL_DECORATION 1
#define ZXDG_TOPLEVEL_DECORATION_V1_DESTROY 0
#define ZXDG_TOPLEVEL_DECORATION_V1_SET_MODE 1
enum { ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE = 1 };
static inline struct zxdg_toplevel_decoration_v1 *zxdg_decoration_manager_v1_get_toplevel_decoration(struct zxdg_decoration_manager_v1 *manager, struct xdg_toplevel *toplevel)
{ return (struct zxdg_toplevel_decoration_v1 *)wl_proxy_marshal_flags((struct wl_proxy *)manager, ZXDG_DECORATION_MANAGER_V1_GET_TOPLEVEL_DECORATION, &zxdg_toplevel_decoration_v1_interface, wl_proxy_get_version((struct wl_proxy *)manager), 0, NULL, toplevel); }
static inline void zxdg_toplevel_decoration_v1_set_mode(struct zxdg_toplevel_decoration_v1 *decoration, uint32_t mode)
{ wl_proxy_marshal_flags((struct wl_proxy *)decoration, ZXDG_TOPLEVEL_DECORATION_V1_SET_MODE, NULL, wl_proxy_get_version((struct wl_proxy *)decoration), 0, mode); }
static inline void zxdg_toplevel_decoration_v1_destroy(struct zxdg_toplevel_decoration_v1 *decoration)
{ wl_proxy_marshal_flags((struct wl_proxy *)decoration, ZXDG_TOPLEVEL_DECORATION_V1_DESTROY, NULL, wl_proxy_get_version((struct wl_proxy *)decoration), WL_MARSHAL_FLAG_DESTROY); }

#pragma pop_macro("global")

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

  WL_WM_Window *pointer_focus;
  WL_WM_Window *keyboard_focus;
  Vec2F32 pointer_pos;

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
