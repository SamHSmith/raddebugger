// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#include <xkbcommon/xkbcommon-keysyms.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>

internal WL_WM_Window *
wl_window_from_surface(struct wl_surface *surface)
{
  WL_WM_Window *result = 0;
  for(WL_WM_Window *w = wl_wm_state->first_window; w != 0; w = w->next)
  {
    if(w->surface == surface)
    {
      result = w;
      break;
    }
  }
  return result;
}

internal WM_Event *
wl_push_event(WM_EventKind kind)
{
  WM_Event *result = 0;
  if(wl_wm_state->evts != 0 && wl_wm_state->evt_arena != 0)
  {
    result = wm_event_list_push_new(wl_wm_state->evt_arena, wl_wm_state->evts, kind);
  }
  return result;
}

internal S32
wl_window_moveresize_from_point(WL_WM_Window *w, Vec2F32 p)
{
  F32 edge = w->custom_border_edge_thickness;
  F32 title = w->custom_border_title_thickness;
  Vec2F32 dim = v2f32((F32)w->size.x, (F32)w->size.y);

  for EachIndex(idx, w->title_bar_client_area_count)
  {
    Rng2F32 rect = w->title_bar_client_areas[idx];
    if(rect.x0 <= p.x && p.x < rect.x1 &&
       rect.y0 <= p.y && p.y < rect.y1)
    {
      return -1;
    }
  }

  B32 on_left   = (edge > 0 && p.x <= edge);
  B32 on_right  = (edge > 0 && p.x >= dim.x - edge);
  B32 on_top    = (edge > 0 && p.y <= edge);
  B32 on_bottom = (edge > 0 && p.y >= dim.y - edge);
  if(on_top && on_left)     { return XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT; }
  if(on_top && on_right)    { return XDG_TOPLEVEL_RESIZE_EDGE_TOP_RIGHT; }
  if(on_bottom && on_left)  { return XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_LEFT; }
  if(on_bottom && on_right) { return XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT; }
  if(on_left)               { return XDG_TOPLEVEL_RESIZE_EDGE_LEFT; }
  if(on_right)              { return XDG_TOPLEVEL_RESIZE_EDGE_RIGHT; }
  if(on_top)                { return XDG_TOPLEVEL_RESIZE_EDGE_TOP; }
  if(on_bottom)             { return XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM; }
  if(title > 0 && p.y < title) { return 0; }
  return -1;
}

internal WM_Cursor
wl_cursor_from_moveresize(S32 dir)
{
  WM_Cursor result = WM_Cursor_COUNT;
  switch(dir)
  {
    default:{}break;
    case XDG_TOPLEVEL_RESIZE_EDGE_LEFT:
    case XDG_TOPLEVEL_RESIZE_EDGE_RIGHT:        {result = WM_Cursor_LeftRight;}break;
    case XDG_TOPLEVEL_RESIZE_EDGE_TOP:
    case XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM:       {result = WM_Cursor_UpDown;}break;
    case XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT:
    case XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT: {result = WM_Cursor_DownRight;}break;
    case XDG_TOPLEVEL_RESIZE_EDGE_TOP_RIGHT:
    case XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_LEFT:  {result = WM_Cursor_UpRight;}break;
  }
  return result;
}

internal WM_Key
wl_wm_key_from_keysym(xkb_keysym_t keysym, B32 *is_right_sided_out)
{
  B32 is_right_sided = 0;
  WM_Key key = WM_Key_Null;
  switch(keysym)
  {
    default:
    {
      if(0){}
      else if(XKB_KEY_F1 <= keysym && keysym <= XKB_KEY_F24) { key = (WM_Key)(WM_Key_F1 + (keysym - XKB_KEY_F1)); }
      else if('0' <= keysym && keysym <= '9')                { key = WM_Key_0 + (keysym-'0'); }
      else if('a' <= keysym && keysym <= 'z')                { key = WM_Key_A + (keysym-'a'); }
      else if('A' <= keysym && keysym <= 'Z')                { key = WM_Key_A + (keysym-'A'); }
    }break;
    case XKB_KEY_Escape:{key = WM_Key_Esc;}break;
    case XKB_KEY_BackSpace:{key = WM_Key_Backspace;}break;
    case XKB_KEY_Delete:{key = WM_Key_Delete;}break;
    case XKB_KEY_Return:{key = WM_Key_Return;}break;
    case XKB_KEY_Pause:{key = WM_Key_Pause;}break;
    case XKB_KEY_Tab:{key = WM_Key_Tab;}break;
    case XKB_KEY_Left:{key = WM_Key_Left;}break;
    case XKB_KEY_Right:{key = WM_Key_Right;}break;
    case XKB_KEY_Up:{key = WM_Key_Up;}break;
    case XKB_KEY_Down:{key = WM_Key_Down;}break;
    case XKB_KEY_Home:{key = WM_Key_Home;}break;
    case XKB_KEY_End:{key = WM_Key_End;}break;
    case XKB_KEY_Page_Up:{key = WM_Key_PageUp;}break;
    case XKB_KEY_Page_Down:{key = WM_Key_PageDown;}break;
    case XKB_KEY_Alt_L:{key = WM_Key_Alt;}break;
    case XKB_KEY_Alt_R:{key = WM_Key_Alt; is_right_sided = 1;}break;
    case XKB_KEY_Shift_L:{key = WM_Key_Shift;}break;
    case XKB_KEY_Shift_R:{key = WM_Key_Shift; is_right_sided = 1;}break;
    case XKB_KEY_Control_L:{key = WM_Key_Ctrl;}break;
    case XKB_KEY_Control_R:{key = WM_Key_Ctrl; is_right_sided = 1;}break;
    case '-':{key = WM_Key_Minus;}break;
    case '=':{key = WM_Key_Equal;}break;
    case '[':{key = WM_Key_LeftBracket;}break;
    case ']':{key = WM_Key_RightBracket;}break;
    case ';':{key = WM_Key_Semicolon;}break;
    case '\'':{key = WM_Key_Quote;}break;
    case '.':{key = WM_Key_Period;}break;
    case ',':{key = WM_Key_Comma;}break;
    case '/':{key = WM_Key_Slash;}break;
    case '\\':{key = WM_Key_BackSlash;}break;
    case ' ':{key = WM_Key_Space;}break;
  }
  if(is_right_sided_out) { *is_right_sided_out = is_right_sided; }
  return key;
}

internal void
wl_apply_cursor(WM_Cursor cursor)
{
  if(wl_wm_state->pointer == 0 || wl_wm_state->cursor_theme == 0 || wl_wm_state->cursor_surface == 0) {return;}
  local_persist const char *cursor_names[WM_Cursor_COUNT] =
  {
    "left_ptr",
    "xterm",
    "sb_h_double_arrow",
    "sb_v_double_arrow",
    "bottom_right_corner",
    "top_right_corner",
    "fleur",
    "hand1",
    "crossed_circle",
  };
  struct wl_cursor *wc = wl_cursor_theme_get_cursor(wl_wm_state->cursor_theme, cursor_names[cursor]);
  if(wc == 0 || wc->image_count == 0) { wc = wl_cursor_theme_get_cursor(wl_wm_state->cursor_theme, "left_ptr"); }
  if(wc == 0 || wc->image_count == 0) {return;}
  struct wl_cursor_image *image = wc->images[0];
  struct wl_buffer *buffer = wl_cursor_image_get_buffer(image);
  wl_pointer_set_cursor(wl_wm_state->pointer, wl_wm_state->pointer_enter_serial,
                        wl_wm_state->cursor_surface, image->hotspot_x, image->hotspot_y);
  wl_surface_attach(wl_wm_state->cursor_surface, buffer, 0, 0);
  wl_surface_damage(wl_wm_state->cursor_surface, 0, 0, image->width, image->height);
  wl_surface_commit(wl_wm_state->cursor_surface);
  wl_wm_state->applied_cursor = cursor;
}

internal void
wl_window_update_opaque_region(WL_WM_Window *w)
{
  if(wl_wm_state->compositor == 0 || w->surface == 0) {return;}
  struct wl_region *region = wl_compositor_create_region(wl_wm_state->compositor);
  wl_region_add(region, 0, 0, w->size.x, w->size.y);
  wl_surface_set_opaque_region(w->surface, region);
  wl_region_destroy(region);
}

internal void
wl_wm_base_ping(void *data, struct xdg_wm_base *wm_base, uint32_t serial)
{
  xdg_wm_base_pong(wm_base, serial);
}
local_persist const struct xdg_wm_base_listener wl_wm_base_listener = { wl_wm_base_ping };

internal void
wl_xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial)
{
  WL_WM_Window *w = (WL_WM_Window *)data;

  if(w->pending_size.x > 0 && w->pending_size.y > 0 &&
     (w->pending_size.x != w->size.x || w->pending_size.y != w->size.y))
  {
    w->size = w->pending_size;
    if(w->egl_window != 0)
    {
      wl_egl_window_resize(w->egl_window, w->size.x, w->size.y, 0, 0);
    }
    wl_window_update_opaque_region(w);
  }

  xdg_surface_ack_configure(xdg_surface, serial);
  w->configured = 1;

  WM_Event *e = wl_push_event(WM_EventKind_Wakeup);
  if(e != 0) { e->window.u64[0] = (U64)w; }
}
local_persist const struct xdg_surface_listener wl_xdg_surface_listener = { wl_xdg_surface_configure };

internal void
wl_xdg_toplevel_configure(void *data, struct xdg_toplevel *toplevel, int32_t width, int32_t height, struct wl_array *states)
{
  WL_WM_Window *w = (WL_WM_Window *)data;
  if(width > 0 && height > 0)
  {
    w->pending_size = v2s32(width, height);
  }
  w->fullscreen = 0;
  w->maximized = 0;
  w->activated = 0;
  uint32_t *state = 0;
  for(state = (uint32_t *)states->data;
      (char *)state < ((char *)states->data + states->size);
      state += 1)
  {
    switch(*state)
    {
      default:{}break;
      case XDG_TOPLEVEL_STATE_FULLSCREEN:{w->fullscreen = 1;}break;
      case XDG_TOPLEVEL_STATE_MAXIMIZED:{w->maximized = 1;}break;
      case XDG_TOPLEVEL_STATE_ACTIVATED:{w->activated = 1;}break;
    }
  }
}

internal void
wl_xdg_toplevel_close(void *data, struct xdg_toplevel *toplevel)
{
  WL_WM_Window *w = (WL_WM_Window *)data;
  w->wants_close = 1;
  WM_Event *e = wl_push_event(WM_EventKind_WindowClose);
  if(e != 0) { e->window.u64[0] = (U64)w; }
}
local_persist const struct xdg_toplevel_listener wl_xdg_toplevel_listener =
{
  wl_xdg_toplevel_configure,
  wl_xdg_toplevel_close,
};

internal void
wl_pointer_enter(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface, wl_fixed_t sx, wl_fixed_t sy)
{
  wl_wm_state->pointer_focus = wl_window_from_surface(surface);
  wl_wm_state->pointer_enter_serial = serial;
  wl_wm_state->last_serial = serial;
  wl_wm_state->pointer_pos = v2f32((F32)wl_fixed_to_double(sx), (F32)wl_fixed_to_double(sy));
  wl_apply_cursor(wl_wm_state->last_set_cursor);
}

internal void
wl_pointer_leave(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface)
{
  wl_wm_state->last_serial = serial;
  if(wl_wm_state->pointer_focus == wl_window_from_surface(surface))
  {
    wl_wm_state->pointer_focus = 0;
  }
}

internal void
wl_pointer_motion(void *data, struct wl_pointer *pointer, uint32_t time, wl_fixed_t sx, wl_fixed_t sy)
{
  WL_WM_Window *w = wl_wm_state->pointer_focus;
  Vec2F32 p = v2f32((F32)wl_fixed_to_double(sx), (F32)wl_fixed_to_double(sy));
  wl_wm_state->pointer_pos = p;
  WM_Event *e = wl_push_event(WM_EventKind_MouseMove);
  if(e != 0)
  {
    e->window.u64[0] = (U64)w;
    e->pos = p;
  }

  WM_Cursor cursor = wl_wm_state->last_set_cursor;
  if(w != 0)
  {
    S32 dir = wl_window_moveresize_from_point(w, p);
    WM_Cursor override = wl_cursor_from_moveresize(dir);
    if(override != WM_Cursor_COUNT) { cursor = override; }
  }
  if(cursor != wl_wm_state->applied_cursor) { wl_apply_cursor(cursor); }
}

internal void
wl_pointer_button(void *data, struct wl_pointer *pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state)
{
  wl_wm_state->last_serial = serial;
  wl_wm_state->last_pointer_button_serial = serial;
  WL_WM_Window *w = wl_wm_state->pointer_focus;
  B32 is_press = (state == WL_POINTER_BUTTON_STATE_PRESSED);

  WM_Key key = WM_Key_Null;
  switch(button)
  {
    default:{}break;
    case 0x110:{key = WM_Key_LeftMouseButton;}break;
    case 0x112:{key = WM_Key_MiddleMouseButton;}break;
    case 0x111:{key = WM_Key_RightMouseButton;}break;
  }

  if(is_press && button == 0x110 && w != 0 && w->xdg_toplevel != 0)
  {
    S32 dir = wl_window_moveresize_from_point(w, wl_wm_state->pointer_pos);
    if(dir == 0)
    {
      xdg_toplevel_move(w->xdg_toplevel, wl_wm_state->seat, serial);
      return;
    }
    else if(dir > 0)
    {
      xdg_toplevel_resize(w->xdg_toplevel, wl_wm_state->seat, serial, (uint32_t)dir);
      return;
    }
  }

  if(key != WM_Key_Null)
  {
    WM_Event *e = wl_push_event(is_press ? WM_EventKind_Press : WM_EventKind_Release);
    if(e != 0)
    {
      e->window.u64[0] = (U64)w;
      e->modifiers = wl_wm_state->modifiers;
      e->key = key;
      e->pos = wl_wm_state->pointer_pos;
    }
  }
}

internal void
wl_pointer_axis(void *data, struct wl_pointer *pointer, uint32_t time, uint32_t axis, wl_fixed_t value)
{
  WL_WM_Window *w = wl_wm_state->pointer_focus;
  WM_Event *e = wl_push_event(WM_EventKind_Scroll);
  if(e != 0)
  {
    e->window.u64[0] = (U64)w;
    e->modifiers = wl_wm_state->modifiers;
    F32 v = (F32)wl_fixed_to_double(value);

    if(axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL) { e->delta = v2f32(v/10.f, 0); }
    else                                          { e->delta = v2f32(0, v/10.f); }
    e->pos = wl_wm_state->pointer_pos;
  }
}

internal void wl_pointer_frame(void *data, struct wl_pointer *p){}
internal void wl_pointer_axis_source(void *data, struct wl_pointer *p, uint32_t s){}
internal void wl_pointer_axis_stop(void *data, struct wl_pointer *p, uint32_t t, uint32_t a){}
internal void wl_pointer_axis_discrete(void *data, struct wl_pointer *p, uint32_t a, int32_t d){}
internal void wl_pointer_axis_value120(void *data, struct wl_pointer *p, uint32_t a, int32_t v){}
internal void wl_pointer_axis_relative_direction(void *data, struct wl_pointer *p, uint32_t a, uint32_t d){}
local_persist const struct wl_pointer_listener wl_pointer_listener =
{
  wl_pointer_enter,
  wl_pointer_leave,
  wl_pointer_motion,
  wl_pointer_button,
  wl_pointer_axis,
  wl_pointer_frame,
  wl_pointer_axis_source,
  wl_pointer_axis_stop,
  wl_pointer_axis_discrete,
  wl_pointer_axis_value120,
  wl_pointer_axis_relative_direction,
};

internal void
wl_keyboard_keymap(void *data, struct wl_keyboard *kb, uint32_t format, int32_t fd, uint32_t size)
{
  if(format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) { close(fd); return; }
  char *map_str = (char *)mmap(0, size, PROT_READ, MAP_PRIVATE, fd, 0);
  if(map_str == MAP_FAILED) { close(fd); return; }
  struct xkb_keymap *keymap = xkb_keymap_new_from_string(wl_wm_state->xkb_context, map_str, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
  munmap(map_str, size);
  close(fd);
  if(keymap == 0) { return; }
  struct xkb_state *state = xkb_state_new(keymap);
  if(wl_wm_state->xkb_state != 0) { xkb_state_unref(wl_wm_state->xkb_state); }
  if(wl_wm_state->xkb_keymap != 0) { xkb_keymap_unref(wl_wm_state->xkb_keymap); }
  wl_wm_state->xkb_keymap = keymap;
  wl_wm_state->xkb_state = state;
}

internal void
wl_keyboard_enter(void *data, struct wl_keyboard *kb, uint32_t serial, struct wl_surface *surface, struct wl_array *keys)
{
  wl_wm_state->keyboard_focus = wl_window_from_surface(surface);
  wl_wm_state->last_serial = serial;
}

internal void
wl_keyboard_leave(void *data, struct wl_keyboard *kb, uint32_t serial, struct wl_surface *surface)
{
  wl_wm_state->last_serial = serial;
  wl_wm_state->repeat_key = 0;
  struct itimerspec disarm = {0};
  timerfd_settime(wl_wm_state->repeat_fd, 0, &disarm, 0);
  if(wl_wm_state->keyboard_focus == wl_window_from_surface(surface))
  {
    WM_Event *e = wl_push_event(WM_EventKind_WindowLoseFocus);
    if(e != 0) { e->window.u64[0] = (U64)wl_wm_state->keyboard_focus; }
    wl_wm_state->keyboard_focus = 0;
  }
}

internal void
wl_emit_key(uint32_t keycode, B32 is_press, B32 is_repeat)
{
  if(wl_wm_state->xkb_state == 0) {return;}
  xkb_keysym_t keysym = xkb_state_key_get_one_sym(wl_wm_state->xkb_state, keycode);
  B32 is_right_sided = 0;
  WM_Key key = wl_wm_key_from_keysym(keysym, &is_right_sided);

  U32 codepoint = 0;
  if(is_press)
  {
    char buf[64];
    int n = xkb_state_key_get_utf8(wl_wm_state->xkb_state, keycode, buf, sizeof(buf));
    for(int off = 0; off < n;)
    {
      UnicodeDecode decode = utf8_decode((U8 *)buf+off, n-off);
      if(decode.codepoint != 0 && decode.codepoint != 127 && (decode.codepoint >= 32 || decode.codepoint == '\t'))
      {
        WM_Event *e = wl_push_event(WM_EventKind_Text);
        if(e != 0)
        {
          e->window.u64[0] = (U64)wl_wm_state->keyboard_focus;
          e->character = decode.codepoint;
          e->is_repeat = is_repeat;
        }
        if(codepoint == 0) { codepoint = decode.codepoint; }
      }
      if(decode.inc == 0) { break; }
      off += decode.inc;
    }
  }

  {
    WM_Event *e = wl_push_event(is_press ? WM_EventKind_Press : WM_EventKind_Release);
    if(e != 0)
    {
      e->window.u64[0] = (U64)wl_wm_state->keyboard_focus;
      e->modifiers = wl_wm_state->modifiers;
      e->key = key;
      e->right_sided = is_right_sided;
      e->is_repeat = is_repeat;
    }
  }

  if(is_press && wl_wm_state->repeat_rate > 0 && wl_wm_state->xkb_keymap != 0 &&
     xkb_keymap_key_repeats(wl_wm_state->xkb_keymap, keycode))
  {
    wl_wm_state->repeat_key = keycode;
    wl_wm_state->repeat_wm_key = key;
    wl_wm_state->repeat_codepoint = codepoint;
    struct itimerspec its = {0};
    its.it_value.tv_sec = wl_wm_state->repeat_delay_ms / 1000;
    its.it_value.tv_nsec = (wl_wm_state->repeat_delay_ms % 1000) * 1000000;
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = (long)(1000000000ll / wl_wm_state->repeat_rate);
    timerfd_settime(wl_wm_state->repeat_fd, 0, &its, 0);
  }
}

internal void
wl_keyboard_key(void *data, struct wl_keyboard *kb, uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
{
  wl_wm_state->last_serial = serial;
  uint32_t keycode = key + 8;
  B32 is_press = (state == WL_KEYBOARD_KEY_STATE_PRESSED);
  if(!is_press && wl_wm_state->repeat_key == keycode)
  {
    wl_wm_state->repeat_key = 0;
    struct itimerspec disarm = {0};
    timerfd_settime(wl_wm_state->repeat_fd, 0, &disarm, 0);
  }
  wl_emit_key(keycode, is_press, 0);
}

internal void
wl_keyboard_modifiers(void *data, struct wl_keyboard *kb, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group)
{
  if(wl_wm_state->xkb_state == 0) {return;}
  xkb_state_update_mask(wl_wm_state->xkb_state, mods_depressed, mods_latched, mods_locked, 0, 0, group);
  WM_Modifiers m = 0;
  if(xkb_state_mod_name_is_active(wl_wm_state->xkb_state, XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE) > 0)  { m |= WM_Modifier_Ctrl; }
  if(xkb_state_mod_name_is_active(wl_wm_state->xkb_state, XKB_MOD_NAME_SHIFT, XKB_STATE_MODS_EFFECTIVE) > 0) { m |= WM_Modifier_Shift; }
  if(xkb_state_mod_name_is_active(wl_wm_state->xkb_state, XKB_MOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE) > 0)   { m |= WM_Modifier_Alt; }
  wl_wm_state->modifiers = m;
}

internal void
wl_keyboard_repeat_info(void *data, struct wl_keyboard *kb, int32_t rate, int32_t delay)
{
  wl_wm_state->repeat_rate = rate;
  wl_wm_state->repeat_delay_ms = delay;
}
local_persist const struct wl_keyboard_listener wl_keyboard_listener =
{
  wl_keyboard_keymap,
  wl_keyboard_enter,
  wl_keyboard_leave,
  wl_keyboard_key,
  wl_keyboard_modifiers,
  wl_keyboard_repeat_info,
};

internal void
wl_seat_capabilities(void *data, struct wl_seat *seat, uint32_t caps)
{
  if((caps & WL_SEAT_CAPABILITY_POINTER) && wl_wm_state->pointer == 0)
  {
    wl_wm_state->pointer = wl_seat_get_pointer(seat);
    wl_pointer_add_listener(wl_wm_state->pointer, &wl_pointer_listener, 0);
  }
  if((caps & WL_SEAT_CAPABILITY_KEYBOARD) && wl_wm_state->keyboard == 0)
  {
    wl_wm_state->keyboard = wl_seat_get_keyboard(seat);
    wl_keyboard_add_listener(wl_wm_state->keyboard, &wl_keyboard_listener, 0);
  }
}
internal void wl_seat_name(void *data, struct wl_seat *seat, const char *name){}
local_persist const struct wl_seat_listener wl_seat_listener = { wl_seat_capabilities, wl_seat_name };

internal void
wl_registry_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
  if(strcmp(interface, wl_compositor_interface.name) == 0)
  {
    wl_wm_state->compositor = (struct wl_compositor *)wl_registry_bind(registry, name, &wl_compositor_interface, 4);
  }
  else if(strcmp(interface, wl_shm_interface.name) == 0)
  {
    wl_wm_state->shm = (struct wl_shm *)wl_registry_bind(registry, name, &wl_shm_interface, 1);
  }
  else if(strcmp(interface, xdg_wm_base_interface.name) == 0)
  {
    wl_wm_state->wm_base = (struct xdg_wm_base *)wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
    xdg_wm_base_add_listener(wl_wm_state->wm_base, &wl_wm_base_listener, 0);
  }
  else if(strcmp(interface, wl_seat_interface.name) == 0)
  {
    wl_wm_state->seat = (struct wl_seat *)wl_registry_bind(registry, name, &wl_seat_interface, 5);
    wl_seat_add_listener(wl_wm_state->seat, &wl_seat_listener, 0);
  }
  else if(strcmp(interface, zxdg_decoration_manager_v1_interface.name) == 0)
  {
    wl_wm_state->decoration_manager = (struct zxdg_decoration_manager_v1 *)wl_registry_bind(registry, name, &zxdg_decoration_manager_v1_interface, 1);
  }
}
internal void wl_registry_global_remove(void *data, struct wl_registry *registry, uint32_t name){}
local_persist const struct wl_registry_listener wl_registry_listener = { wl_registry_global, wl_registry_global_remove };

internal B32
wm_wl_init(void)
{
  Arena *arena = arena_alloc();
  wl_wm_state = push_array(arena, WL_WM_State, 1);
  wl_wm_state->arena = arena;
  wl_wm_state->applied_cursor = WM_Cursor_COUNT;
  wl_wm_state->repeat_rate = 25;
  wl_wm_state->repeat_delay_ms = 400;

  wl_wm_state->display = wl_display_connect(0);
  if(wl_wm_state->display == 0)
  {
    return 0;
  }
  wl_wm_state->registry = wl_display_get_registry(wl_wm_state->display);
  wl_registry_add_listener(wl_wm_state->registry, &wl_registry_listener, 0);
  wl_display_roundtrip(wl_wm_state->display);
  wl_display_roundtrip(wl_wm_state->display);

  if(wl_wm_state->compositor == 0 || wl_wm_state->wm_base == 0)
  {
    wl_display_disconnect(wl_wm_state->display);
    wl_wm_state->display = 0;
    return 0;
  }

  wl_wm_state->xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  if(wl_wm_state->shm != 0)
  {
    wl_wm_state->cursor_theme = wl_cursor_theme_load(0, 24, wl_wm_state->shm);
    wl_wm_state->cursor_surface = wl_compositor_create_surface(wl_wm_state->compositor);
  }

  wl_wm_state->gfx_info.double_click_time = 0.5f;
  wl_wm_state->gfx_info.caret_blink_time = 0.5f;
  wl_wm_state->gfx_info.default_refresh_rate = 60.f;

  wl_wm_state->wakeup_fd = eventfd(0, EFD_CLOEXEC);
  Assert(wl_wm_state->wakeup_fd > 0);
  wl_wm_state->repeat_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC|TFD_NONBLOCK);
  Assert(wl_wm_state->repeat_fd > 0);
  return 1;
}

internal WM_SystemInfo *
wm_wl_get_system_info(void)
{
  return &wl_wm_state->gfx_info;
}

internal void    wm_wl_set_clipboard_text(String8 string){}
internal String8 wm_wl_get_clipboard_text(Arena *arena){ String8 r = {0}; return r; }

internal WM_Window
wm_wl_window_open(Rng2F32 rect, WM_WindowFlags flags, String8 title)
{
  Vec2F32 resolution = dim_2f32(rect);
  Vec2S32 size = v2s32((S32)resolution.x, (S32)resolution.y);
  if(size.x <= 0) { size.x = 1280; }
  if(size.y <= 0) { size.y = 720; }

  WL_WM_Window *w = wl_wm_state->free_window;
  if(w) { SLLStackPop(wl_wm_state->free_window); }
  else  { w = push_array_no_zero(wl_wm_state->arena, WL_WM_Window, 1); }
  MemoryZeroStruct(w);
  DLLPushBack(wl_wm_state->first_window, wl_wm_state->last_window, w);
  w->size = size;
  w->pending_size = size;

  w->surface = wl_compositor_create_surface(wl_wm_state->compositor);
  w->xdg_surface = xdg_wm_base_get_xdg_surface(wl_wm_state->wm_base, w->surface);
  xdg_surface_add_listener(w->xdg_surface, &wl_xdg_surface_listener, w);
  w->xdg_toplevel = xdg_surface_get_toplevel(w->xdg_surface);
  xdg_toplevel_add_listener(w->xdg_toplevel, &wl_xdg_toplevel_listener, w);

  Temp scratch = scratch_begin(0, 0);
  String8 title_copy = push_str8_copy(scratch.arena, title);
  xdg_toplevel_set_title(w->xdg_toplevel, (char *)title_copy.str);
  xdg_toplevel_set_app_id(w->xdg_toplevel, "raddbg");
  scratch_end(scratch);

  if(wl_wm_state->decoration_manager != 0)
  {
    w->decoration = zxdg_decoration_manager_v1_get_toplevel_decoration(wl_wm_state->decoration_manager, w->xdg_toplevel);
    zxdg_toplevel_decoration_v1_set_mode(w->decoration, ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
  }

  w->egl_window = wl_egl_window_create(w->surface, w->size.x, w->size.y);
  wl_window_update_opaque_region(w);

  wl_surface_commit(w->surface);
  wl_display_roundtrip(wl_wm_state->display);

  WM_Window handle = {(U64)w};
  return handle;
}

internal void
wm_wl_window_close(WM_Window handle)
{
  if(wm_window_match(handle, wm_window_zero())) {return;}
  WL_WM_Window *w = (WL_WM_Window *)handle.u64[0];
  if(w->egl_window)   { wl_egl_window_destroy(w->egl_window); }
  if(w->decoration)   { zxdg_toplevel_decoration_v1_destroy(w->decoration); }
  if(w->xdg_toplevel) { xdg_toplevel_destroy(w->xdg_toplevel); }
  if(w->xdg_surface)  { xdg_surface_destroy(w->xdg_surface); }
  if(w->surface)      { wl_surface_destroy(w->surface); }
  DLLRemove(wl_wm_state->first_window, wl_wm_state->last_window, w);
  SLLStackPush(wl_wm_state->free_window, w);
  wl_display_flush(wl_wm_state->display);
}

internal void
wm_wl_window_set_title(WM_Window handle, String8 title)
{
  if(wm_window_match(handle, wm_window_zero())) {return;}
  WL_WM_Window *w = (WL_WM_Window *)handle.u64[0];
  Temp scratch = scratch_begin(0, 0);
  String8 title_copy = push_str8_copy(scratch.arena, title);
  xdg_toplevel_set_title(w->xdg_toplevel, (char *)title_copy.str);
  scratch_end(scratch);
}

internal void
wm_wl_window_first_paint(WM_Window handle)
{

}

internal void wm_wl_window_focus(WM_Window handle){  }

internal B32
wm_wl_window_is_focused(WM_Window handle)
{
  if(wm_window_match(handle, wm_window_zero())) {return 0;}
  WL_WM_Window *w = (WL_WM_Window *)handle.u64[0];
  return w->activated;
}

internal B32
wm_wl_window_is_fullscreen(WM_Window handle)
{
  if(wm_window_match(handle, wm_window_zero())) {return 0;}
  return ((WL_WM_Window *)handle.u64[0])->fullscreen;
}

internal void
wm_wl_window_set_fullscreen(WM_Window handle, B32 fullscreen)
{
  if(wm_window_match(handle, wm_window_zero())) {return;}
  WL_WM_Window *w = (WL_WM_Window *)handle.u64[0];
  if(fullscreen) { xdg_toplevel_set_fullscreen(w->xdg_toplevel, 0); }
  else           { xdg_toplevel_unset_fullscreen(w->xdg_toplevel); }
}

internal B32
wm_wl_window_is_maximized(WM_Window handle)
{
  if(wm_window_match(handle, wm_window_zero())) {return 0;}
  return ((WL_WM_Window *)handle.u64[0])->maximized;
}

internal void
wm_wl_window_set_maximized(WM_Window handle, B32 maximized)
{
  if(wm_window_match(handle, wm_window_zero())) {return;}
  WL_WM_Window *w = (WL_WM_Window *)handle.u64[0];
  if(maximized) { xdg_toplevel_set_maximized(w->xdg_toplevel); }
  else          { xdg_toplevel_unset_maximized(w->xdg_toplevel); }
}

internal B32  wm_wl_window_is_minimized(WM_Window handle){ return 0; }

internal void
wm_wl_window_set_minimized(WM_Window handle, B32 minimized)
{
  if(wm_window_match(handle, wm_window_zero())) {return;}
  WL_WM_Window *w = (WL_WM_Window *)handle.u64[0];
  if(minimized) { xdg_toplevel_set_minimized(w->xdg_toplevel); }
}

internal void wm_wl_window_bring_to_front(WM_Window handle){}
internal void wm_wl_window_set_monitor(WM_Window handle, WM_Monitor monitor){}

internal void
wm_wl_window_clear_custom_border_data(WM_Window handle)
{
  if(wm_window_match(handle, wm_window_zero())) {return;}
  WL_WM_Window *w = (WL_WM_Window *)handle.u64[0];
  w->custom_border_title_thickness = 0;
  w->custom_border_edge_thickness = 0;
  w->title_bar_client_area_count = 0;
}

internal void
wm_wl_window_push_custom_title_bar(WM_Window handle, F32 thickness)
{
  if(wm_window_match(handle, wm_window_zero())) {return;}
  ((WL_WM_Window *)handle.u64[0])->custom_border_title_thickness = thickness;
}

internal void
wm_wl_window_push_custom_edges(WM_Window handle, F32 thickness)
{
  if(wm_window_match(handle, wm_window_zero())) {return;}
  ((WL_WM_Window *)handle.u64[0])->custom_border_edge_thickness = thickness;
}

internal void
wm_wl_window_push_custom_title_bar_client_area(WM_Window handle, Rng2F32 rect)
{
  if(wm_window_match(handle, wm_window_zero())) {return;}
  WL_WM_Window *w = (WL_WM_Window *)handle.u64[0];
  if(w->title_bar_client_area_count < WL_WM_MAX_TITLE_BAR_CLIENT_AREAS)
  {
    w->title_bar_client_areas[w->title_bar_client_area_count] = rect;
    w->title_bar_client_area_count += 1;
  }
}

internal Rng2F32
wm_wl_rect_from_window(WM_Window handle)
{

  if(wm_window_match(handle, wm_window_zero())) {return r2f32p(0,0,0,0);}
  WL_WM_Window *w = (WL_WM_Window *)handle.u64[0];
  return r2f32p(0, 0, (F32)w->size.x, (F32)w->size.y);
}

internal Rng2F32
wm_wl_client_rect_from_window(WM_Window handle)
{
  if(wm_window_match(handle, wm_window_zero())) {return r2f32p(0,0,0,0);}
  WL_WM_Window *w = (WL_WM_Window *)handle.u64[0];
  return r2f32p(0, 0, (F32)w->size.x, (F32)w->size.y);
}

internal F32 wm_wl_dpi_from_window(WM_Window handle){ return 96.f; }

internal WM_ExtWindow wm_wl_focused_external_window(void){ WM_ExtWindow r = {0}; return r; }
internal void         wm_wl_focus_external_window(WM_ExtWindow ext_window){}

internal WM_MonitorArray wm_wl_push_monitors_array(Arena *arena){ WM_MonitorArray r = {0}; return r; }
internal WM_Monitor      wm_wl_primary_monitor(void){ WM_Monitor r = {0}; return r; }
internal WM_Monitor      wm_wl_monitor_from_window(WM_Window window){ WM_Monitor r = {0}; return r; }
internal String8         wm_wl_name_from_monitor(Arena *arena, WM_Monitor monitor){ return str8_zero(); }
internal Vec2F32         wm_wl_dim_from_monitor(WM_Monitor monitor){ return v2f32(0,0); }
internal F32             wm_wl_dpi_from_monitor(WM_Monitor monitor){ return 96.f; }

internal void
wm_wl_send_wakeup_event(void)
{
  U64 dummy = 1;
  ssize_t size = LNX_RETRY_ON_EINTR(write(wl_wm_state->wakeup_fd, &dummy, sizeof(dummy)));
  Assert(size == sizeof(dummy));
}

internal WM_EventList
wm_wl_get_events(Arena *arena, B32 wait)
{
  WM_EventList evts = {0};
  struct wl_display *d = wl_wm_state->display;
  wl_wm_state->evt_arena = arena;
  wl_wm_state->evts = &evts;
  for(;;)
  {

    while(wl_display_prepare_read(d) != 0)
    {
      wl_display_dispatch_pending(d);
    }
    wl_display_flush(d);

    struct pollfd fds[3] =
    {
      { .fd = wl_display_get_fd(d),   .events = POLLIN },
      { .fd = wl_wm_state->wakeup_fd, .events = POLLIN },
      { .fd = wl_wm_state->repeat_fd, .events = POLLIN },
    };
    int timeout = (wait && evts.count == 0) ? -1 : 0;
    int poll_status = poll(fds, ArrayCount(fds), timeout);

    if(poll_status > 0 && (fds[0].revents & POLLIN))
    {
      wl_display_read_events(d);
    }
    else
    {
      wl_display_cancel_read(d);
    }
    wl_display_dispatch_pending(d);

    if(fds[1].revents & POLLIN)
    {
      U64 dummy = 0;
      read(wl_wm_state->wakeup_fd, &dummy, sizeof(dummy));
      wait = 0;
    }

    if((fds[2].revents & POLLIN) && wl_wm_state->repeat_key != 0)
    {
      U64 expirations = 0;
      read(wl_wm_state->repeat_fd, &expirations, sizeof(expirations));
      for(U64 i = 0; i < expirations; i += 1)
      {
        wl_emit_key(wl_wm_state->repeat_key, 1, 1);
      }
    }

    if(evts.count > 0 || wait == 0)
    {
      break;
    }
  }
  wl_wm_state->evts = 0;
  wl_wm_state->evt_arena = 0;
  return evts;
}

internal WM_Modifiers wm_wl_get_modifiers(void){ return wl_wm_state->modifiers; }
internal B32          wm_wl_key_is_down(WM_Key key){ return 0; }

internal Vec2F32
wm_wl_mouse_from_window(WM_Window handle)
{
  if(wm_window_match(handle, wm_window_zero())) {return v2f32(0,0);}
  WL_WM_Window *w = (WL_WM_Window *)handle.u64[0];
  if(wl_wm_state->pointer_focus == w) { return wl_wm_state->pointer_pos; }
  return v2f32(0, 0);
}

internal void
wm_wl_set_cursor(WM_Cursor cursor)
{
  wl_wm_state->last_set_cursor = cursor;
}

internal void
wm_wl_graphical_message(B32 error, String8 title, String8 message)
{
  if(error) { fprintf(stderr, "[X] "); }
  fprintf(stderr, "%.*s\n", str8_varg(title));
  fprintf(stderr, "%.*s\n\n", str8_varg(message));
}

internal String8 wm_wl_graphical_pick_file(Arena *arena, String8 title, String8 initial_path){ return str8_zero(); }

internal void wm_wl_show_in_filesystem_ui(String8 path){}
internal void wm_wl_open_in_browser(String8 url){}
