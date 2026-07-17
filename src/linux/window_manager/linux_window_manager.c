// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////
//~ rjf: Helpers

internal LNX_WM_Window *
lnx_window_from_x11window(Window window)
{
  LNX_WM_Window *result = 0;
  for(LNX_WM_Window *w = lnx_wm_state->first_window; w != 0; w = w->next)
  {
    if(w->window == window)
    {
      result = w;
      break;
    }
  }
  return result;
}

internal S32
lnx_window_moveresize_dir_from_point(LNX_WM_Window *w, Vec2F32 p, Vec2F32 dim)
{
  F32 edge = w->custom_border_edge_thickness;
  F32 title = w->custom_border_title_thickness;

  for EachIndex(idx, w->title_bar_client_area_count)
  {
    Rng2F32 rect = w->title_bar_client_areas[idx];
    if(rect.x0 <= p.x && p.x < rect.x1 &&
       rect.y0 <= p.y && p.y < rect.y1)
    {
      return LNX_WM_MOVERESIZE_NONE;
    }
  }

  B32 on_left   = (edge > 0 && p.x <= edge);
  B32 on_right  = (edge > 0 && p.x >= dim.x - edge);
  B32 on_top    = (edge > 0 && p.y <= edge);
  B32 on_bottom = (edge > 0 && p.y >= dim.y - edge);
  if(on_top && on_left)     { return LNX_WM_MOVERESIZE_SIZE_TOPLEFT; }
  if(on_top && on_right)    { return LNX_WM_MOVERESIZE_SIZE_TOPRIGHT; }
  if(on_bottom && on_left)  { return LNX_WM_MOVERESIZE_SIZE_BOTTOMLEFT; }
  if(on_bottom && on_right) { return LNX_WM_MOVERESIZE_SIZE_BOTTOMRIGHT; }
  if(on_left)               { return LNX_WM_MOVERESIZE_SIZE_LEFT; }
  if(on_right)              { return LNX_WM_MOVERESIZE_SIZE_RIGHT; }
  if(on_top)                { return LNX_WM_MOVERESIZE_SIZE_TOP; }
  if(on_bottom)             { return LNX_WM_MOVERESIZE_SIZE_BOTTOM; }

  if(title > 0 && p.y < title) { return LNX_WM_MOVERESIZE_MOVE; }

  return LNX_WM_MOVERESIZE_NONE;
}

internal WM_Cursor
lnx_wm_cursor_from_moveresize_dir(S32 dir)
{
  WM_Cursor result = WM_Cursor_COUNT;
  switch(dir)
  {
    default:{}break;
    case LNX_WM_MOVERESIZE_SIZE_LEFT:
    case LNX_WM_MOVERESIZE_SIZE_RIGHT:       {result = WM_Cursor_LeftRight;}break;
    case LNX_WM_MOVERESIZE_SIZE_TOP:
    case LNX_WM_MOVERESIZE_SIZE_BOTTOM:      {result = WM_Cursor_UpDown;}break;
    case LNX_WM_MOVERESIZE_SIZE_TOPLEFT:
    case LNX_WM_MOVERESIZE_SIZE_BOTTOMRIGHT: {result = WM_Cursor_DownRight;}break;
    case LNX_WM_MOVERESIZE_SIZE_TOPRIGHT:
    case LNX_WM_MOVERESIZE_SIZE_BOTTOMLEFT:  {result = WM_Cursor_UpRight;}break;
  }
  return result;
}

internal void
lnx_window_begin_moveresize(LNX_WM_Window *w, S32 root_x, S32 root_y, S32 dir, U32 button)
{
  XUngrabPointer(lnx_wm_state->display, CurrentTime);
  XFlush(lnx_wm_state->display);
  XClientMessageEvent msg = {0};
  msg.type = ClientMessage;
  msg.window = w->window;
  msg.message_type = lnx_wm_state->net_wm_moveresize_atom;
  msg.format = 32;
  msg.data.l[0] = root_x;
  msg.data.l[1] = root_y;
  msg.data.l[2] = dir;
  msg.data.l[3] = button;
  msg.data.l[4] = 1; // source indication: normal application
  XSendEvent(lnx_wm_state->display, XDefaultRootWindow(lnx_wm_state->display), False,
             SubstructureRedirectMask|SubstructureNotifyMask, (XEvent *)&msg);
  XFlush(lnx_wm_state->display);
}

internal void
lnx_window_finish_frame_sync(WM_Window handle)
{
  if(wm_window_match(handle, wm_window_zero())) {return;}
  LNX_WM_Window *w = (LNX_WM_Window *)handle.u64[0];
  B32 did_set = 0;
  if(w->sync_request_pending)
  {
    w->sync_request_pending = 0;
    XSyncValue value;
    XSyncIntsToValue(&value, (unsigned)(w->counter_value & 0xffffffff), (int)(w->counter_value >> 32));
    XSyncSetCounter(lnx_wm_state->display, w->counter_xid, value);
    did_set = 1;
  }
  if(w->extended_frame_pending)
  {
    w->extended_frame_pending = 0;
    S64 even = w->extended_counter_value + 1;
    w->extended_counter_value = even;
    XSyncValue value;
    XSyncIntsToValue(&value, (unsigned)(even & 0xffffffff), (int)(even >> 32));
    XSyncSetCounter(lnx_wm_state->display, w->extended_counter_xid, value);
    did_set = 1;
  }
  if(did_set)
  {
    XFlush(lnx_wm_state->display);
  }
}

////////////////////////////////
//~ rjf: @per_os_impl Main Initialization API (Implemented Per-OS)

internal void
wm_x11_init(void)
{
  //- rjf: initialize basics
  Arena *arena = arena_alloc();
  lnx_wm_state = push_array(arena, LNX_WM_State, 1);
  lnx_wm_state->arena = arena;
  lnx_wm_state->display = XOpenDisplay(0);
  
  //- rjf: calculate atoms
  lnx_wm_state->wm_delete_window_atom        = XInternAtom(lnx_wm_state->display, "WM_DELETE_WINDOW", 0);
  lnx_wm_state->wm_sync_request_atom         = XInternAtom(lnx_wm_state->display, "_NET_WM_SYNC_REQUEST", 0);
  lnx_wm_state->wm_sync_request_counter_atom = XInternAtom(lnx_wm_state->display, "_NET_WM_SYNC_REQUEST_COUNTER", 0);
  lnx_wm_state->motif_wm_hints_atom          = XInternAtom(lnx_wm_state->display, "_MOTIF_WM_HINTS", 0);
  lnx_wm_state->net_wm_moveresize_atom       = XInternAtom(lnx_wm_state->display, "_NET_WM_MOVERESIZE", 0);
  
  //- rjf: open im
  lnx_wm_state->xim = XOpenIM(lnx_wm_state->display, 0, 0, 0);
  
  //- rjf: fill out gfx info
  lnx_wm_state->gfx_info.double_click_time = 0.5f;
  lnx_wm_state->gfx_info.caret_blink_time = 0.5f;
  lnx_wm_state->gfx_info.default_refresh_rate = 60.f;
  
  //- rjf: fill out cursors
  {
    struct
    {
      WM_Cursor cursor;
      unsigned int id;
    }
    map[] =
    {
      {WM_Cursor_Pointer,         XC_left_ptr},
      {WM_Cursor_IBar,            XC_xterm},
      {WM_Cursor_LeftRight,       XC_sb_h_double_arrow},
      {WM_Cursor_UpDown,          XC_sb_v_double_arrow},
      {WM_Cursor_DownRight,       XC_bottom_right_corner},
      {WM_Cursor_UpRight,         XC_top_right_corner},
      {WM_Cursor_UpDownLeftRight, XC_fleur},
      {WM_Cursor_HandPoint,       XC_hand1},
      {WM_Cursor_Disabled,        XC_X_cursor},
    };
    for EachElement(idx, map)
    {
      lnx_wm_state->cursors[map[idx].cursor] = XCreateFontCursor(lnx_wm_state->display, map[idx].id);
    }
  }
  
  // create wakeup event for polling
  lnx_wm_state->wakeup_fd = eventfd(0, EFD_CLOEXEC);
  Assert(lnx_wm_state->wakeup_fd > 0);
}

////////////////////////////////
//~ rjf: @per_os_impl Graphics System Info (Implemented Per-OS)

internal WM_SystemInfo *
wm_x11_get_system_info(void)
{
  return &lnx_wm_state->gfx_info;
}

////////////////////////////////
//~ rjf: @per_os_impl Clipboards (Implemented Per-OS)

internal void
wm_x11_set_clipboard_text(String8 string)
{
  
}

internal String8
wm_x11_get_clipboard_text(Arena *arena)
{
  String8 result = {0};
  return result;
}

////////////////////////////////
//~ rjf: @per_os_impl Windows (Implemented Per-OS)

internal WM_Window
wm_x11_window_open(Rng2F32 rect, WM_WindowFlags flags, String8 title)
{
  Vec2F32 resolution = dim_2f32(rect);
  
  //- rjf: allocate window
  LNX_WM_Window *w = lnx_wm_state->free_window;
  if(w)
  {
    SLLStackPop(lnx_wm_state->free_window);
  }
  else
  {
    w = push_array_no_zero(lnx_wm_state->arena, LNX_WM_Window, 1);
  }
  MemoryZeroStruct(w);
  DLLPushBack(lnx_wm_state->first_window, lnx_wm_state->last_window, w);
  
  //- rjf: create window & equip with x11 info
  Visual *visual = lnx_wm_state->window_visual;
  int depth = lnx_wm_state->window_depth;
  Colormap colormap = lnx_wm_state->window_colormap;
  unsigned long attr_mask = 0;
  XSetWindowAttributes window_attribs = {0};
  if(visual == 0)
  {
    visual = (Visual *)CopyFromParent;
    depth = CopyFromParent;
  }
  else
  {
    window_attribs.background_pixmap = None;
    window_attribs.border_pixel = 0;
    window_attribs.colormap = colormap;
    attr_mask = CWBackPixmap|CWBorderPixel|CWColormap;
  }
  w->window = XCreateWindow(lnx_wm_state->display,
                            XDefaultRootWindow(lnx_wm_state->display),
                            0, 0, resolution.x, resolution.y,
                            0,
                            depth,
                            InputOutput,
                            visual,
                            attr_mask,
                            &window_attribs);
  XSelectInput(lnx_wm_state->display, w->window,
               ExposureMask|
               PointerMotionMask|
               ButtonPressMask|
               ButtonReleaseMask|
               KeyPressMask|
               KeyReleaseMask|
               StructureNotifyMask|
               FocusChangeMask);
  Atom protocols[] =
  {
    lnx_wm_state->wm_delete_window_atom,
    lnx_wm_state->wm_sync_request_atom,
  };
  XSetWMProtocols(lnx_wm_state->display, w->window, protocols, ArrayCount(protocols));
  {
    XSyncValue initial_value;
    XSyncIntToValue(&initial_value, 0);
    w->counter_xid = XSyncCreateCounter(lnx_wm_state->display, initial_value);
    w->extended_counter_xid = XSyncCreateCounter(lnx_wm_state->display, initial_value);
  }
  {
    XID counters[2] = {w->counter_xid, w->extended_counter_xid};
    XChangeProperty(lnx_wm_state->display, w->window, lnx_wm_state->wm_sync_request_counter_atom, XA_CARDINAL, 32, PropModeReplace, (U8 *)counters, 2);
  }

  {
    LNX_MotifWMHints hints = {0};
    hints.flags = MWM_HINTS_DECORATIONS;
    hints.decorations = MWM_DECOR_NONE;
    XChangeProperty(lnx_wm_state->display, w->window,
                    lnx_wm_state->motif_wm_hints_atom, lnx_wm_state->motif_wm_hints_atom,
                    32, PropModeReplace, (U8 *)&hints, sizeof(hints)/sizeof(long));
  }

  //- rjf: create xic
  w->xic = XCreateIC(lnx_wm_state->xim,
                     XNInputStyle, XIMPreeditNothing|XIMStatusNothing,
                     XNClientWindow, w->window,
                     XNFocusWindow, w->window,
                     NULL);
  
  //- rjf: attach name
  Temp scratch = scratch_begin(0, 0);
  String8 title_copy = push_str8_copy(scratch.arena, title);
  XStoreName(lnx_wm_state->display, w->window, (char *)title_copy.str);
  scratch_end(scratch);
  
  //- rjf: convert to handle & return
  WM_Window handle = {(U64)w};
  return handle;
}

internal void
wm_x11_window_close(WM_Window handle)
{
  if(wm_window_match(handle, wm_window_zero())) {return;}
  LNX_WM_Window *w = (LNX_WM_Window *)handle.u64[0];
  XDestroyWindow(lnx_wm_state->display, w->window);
}

internal void
wm_x11_window_set_title(WM_Window handle, String8 title)
{
  if(wm_window_match(handle, wm_window_zero())) {return;}
  Temp scratch = scratch_begin(0, 0);
  LNX_WM_Window *w = (LNX_WM_Window *)handle.u64[0];
  String8 title_copy = push_str8_copy(scratch.arena, title);
  XStoreName(lnx_wm_state->display, w->window, (char *)title_copy.str);
  scratch_end(scratch);
}

internal void
wm_x11_window_first_paint(WM_Window handle)
{
  if(wm_window_match(handle, wm_window_zero())) {return;}
  LNX_WM_Window *w = (LNX_WM_Window *)handle.u64[0];
  XMapWindow(lnx_wm_state->display, w->window);
  XFlush(lnx_wm_state->display);
}

internal void
wm_x11_window_focus(WM_Window handle)
{
  if(wm_window_match(handle, wm_window_zero())) {return;}
  LNX_WM_Window *w = (LNX_WM_Window *)handle.u64[0];
  // 
  // TODO: if the target is lightweight, the debugger may launch it even before the first frame is drawn;
  //       this is a problem because XSetInputFocus is now called before XMapWindow, we need a guard
  //       to prevent an X server error:
  //
  //         X Error of failed request:  BadMatch (invalid parameter attributes)
  //            Major opcode of failed request:  42 (X_SetInputFocus)
  //            Serial number of failed request:  373
  //            Current serial number in output stream:  374
  //        
  XSetInputFocus(lnx_wm_state->display, w->window, RevertToNone, CurrentTime);
}

internal B32
wm_x11_window_is_focused(WM_Window handle)
{
  if(wm_window_match(handle, wm_window_zero())) {return 0;}
  LNX_WM_Window *w = (LNX_WM_Window *)handle.u64[0];
  Window focused_window = 0;
  int revert_to = 0;
  XGetInputFocus(lnx_wm_state->display, &focused_window, &revert_to);
  B32 result = (w->window == focused_window);
  return result;
}

internal B32
wm_x11_window_is_fullscreen(WM_Window handle)
{
  if(wm_window_match(handle, wm_window_zero())) {return 0;}
  // TODO(rjf)
  return 0;
}

internal void
wm_x11_window_set_fullscreen(WM_Window handle, B32 fullscreen)
{
  if(wm_window_match(handle, wm_window_zero())) {return;}
  // TODO(rjf)
}

internal B32
wm_x11_window_is_maximized(WM_Window handle)
{
  if(wm_window_match(handle, wm_window_zero())) {return 0;}
  // TODO(rjf)
  return 0;
}

internal void
wm_x11_window_set_maximized(WM_Window handle, B32 maximized)
{
  if(wm_window_match(handle, wm_window_zero())) {return;}
  // TODO(rjf)
}

internal B32
wm_x11_window_is_minimized(WM_Window handle)
{
  if(wm_window_match(handle, wm_window_zero())) {return 0;}
  // TODO(rjf)
  return 0;
}

internal void
wm_x11_window_set_minimized(WM_Window handle, B32 minimized)
{
  if(wm_window_match(handle, wm_window_zero())) {return;}
  // TODO(rjf)
}

internal void
wm_x11_window_bring_to_front(WM_Window handle)
{
  if(wm_window_match(handle, wm_window_zero())) {return;}
  // TODO(rjf)
}

internal void
wm_x11_window_set_monitor(WM_Window handle, WM_Monitor monitor)
{
  if(wm_window_match(handle, wm_window_zero())) {return;}
  // TODO(rjf)
}

internal void
wm_x11_window_clear_custom_border_data(WM_Window handle)
{
  if(wm_window_match(handle, wm_window_zero())) {return;}
  LNX_WM_Window *w = (LNX_WM_Window *)handle.u64[0];
  w->custom_border_title_thickness = 0;
  w->custom_border_edge_thickness = 0;
  w->title_bar_client_area_count = 0;
}

internal void
wm_x11_window_push_custom_title_bar(WM_Window handle, F32 thickness)
{
  if(wm_window_match(handle, wm_window_zero())) {return;}
  LNX_WM_Window *w = (LNX_WM_Window *)handle.u64[0];
  w->custom_border_title_thickness = thickness;
}

internal void
wm_x11_window_push_custom_edges(WM_Window handle, F32 thickness)
{
  if(wm_window_match(handle, wm_window_zero())) {return;}
  LNX_WM_Window *w = (LNX_WM_Window *)handle.u64[0];
  w->custom_border_edge_thickness = thickness;
}

internal void
wm_x11_window_push_custom_title_bar_client_area(WM_Window handle, Rng2F32 rect)
{
  if(wm_window_match(handle, wm_window_zero())) {return;}
  LNX_WM_Window *w = (LNX_WM_Window *)handle.u64[0];
  if(w->title_bar_client_area_count < LNX_WM_MAX_TITLE_BAR_CLIENT_AREAS)
  {
    w->title_bar_client_areas[w->title_bar_client_area_count] = rect;
    w->title_bar_client_area_count += 1;
  }
}

internal Rng2F32
wm_x11_rect_from_window(WM_Window handle)
{
  if(wm_window_match(handle, wm_window_zero())) {return r2f32p(0, 0, 0, 0);}
  LNX_WM_Window *w = (LNX_WM_Window *)handle.u64[0];
  XWindowAttributes atts = {0};
  Status s = XGetWindowAttributes(lnx_wm_state->display, w->window, &atts);
  Rng2F32 result = r2f32p((F32)atts.x, (F32)atts.y, (F32)atts.x + (F32)atts.width, (F32)atts.y + (F32)atts.height);
  return result;
}

internal Rng2F32
wm_x11_client_rect_from_window(WM_Window handle)
{
  LNX_WM_Window *w = (LNX_WM_Window *)handle.u64[0];
  XWindowAttributes atts = {0};
  Status s = XGetWindowAttributes(lnx_wm_state->display, w->window, &atts);
  Rng2F32 result = r2f32p(0, 0, (F32)atts.width, (F32)atts.height);
  return result;
}

internal F32
wm_x11_dpi_from_window(WM_Window handle)
{
  // TODO(rjf)
  return 96.f;
}

////////////////////////////////
//~ rjf: @per_os_impl External Windows (Implemented Per-OS)

internal WM_ExtWindow
wm_x11_focused_external_window(void)
{
  WM_ExtWindow result = {0};
  // TODO(rjf)
  return result;
}

internal void
wm_x11_focus_external_window(WM_ExtWindow handle)
{
  // TODO(rjf)
}

////////////////////////////////
//~ rjf: @per_os_impl Monitors (Implemented Per-OS)

internal WM_MonitorArray
wm_x11_push_monitors_array(Arena *arena)
{
  WM_MonitorArray result = {0};
  // TODO(rjf)
  return result;
}

internal WM_Monitor
wm_x11_primary_monitor(void)
{
  WM_Monitor result = {0};
  // TODO(rjf)
  return result;
}

internal WM_Monitor
wm_x11_monitor_from_window(WM_Window window)
{
  WM_Monitor result = {0};
  // TODO(rjf)
  return result;
}

internal String8
wm_x11_name_from_monitor(Arena *arena, WM_Monitor monitor)
{
  // TODO(rjf)
  return str8_zero();
}

internal Vec2F32
wm_x11_dim_from_monitor(WM_Monitor monitor)
{
  // TODO(rjf)
  return v2f32(0, 0);
}

internal F32
wm_x11_dpi_from_monitor(WM_Monitor monitor)
{
  // TODO(rjf)
  return 96.f;
}

////////////////////////////////
//~ rjf: @per_os_impl Events (Implemented Per-OS)

internal void
wm_x11_send_wakeup_event(void)
{
  U64 dummy = 1;
  ssize_t size = LNX_RETRY_ON_EINTR(write(lnx_wm_state->wakeup_fd, &dummy, sizeof(dummy)));
  Assert(size == sizeof(dummy));
}

internal WM_EventList
wm_x11_get_events(Arena *arena, B32 wait)
{
  WM_EventList evts = {0};
  for(;;)
  {
    if(XPending(lnx_wm_state->display) == 0)
    {
      struct pollfd poll_fds[2] =
      {
        { .fd = ConnectionNumber(lnx_wm_state->display), .events = POLLIN },
        { .fd = lnx_wm_state->wakeup_fd,                 .events = POLLIN },
      };
      int timeout = wait && evts.count == 0 ? -1 : 0;
      int poll_status = poll(poll_fds, ArrayCount(poll_fds), timeout);
      Assert(poll_status >= 0);
      if(poll_fds[1].revents & POLLIN)
      {
        U64 dummy = 0;
        read(lnx_wm_state->wakeup_fd, &dummy, sizeof(dummy));
        wait = 0;
      }
    }
    while(XPending(lnx_wm_state->display))
    {
      XEvent evt = {0};
      XNextEvent(lnx_wm_state->display, &evt);
      B32 set_mouse_cursor = 0;
      WM_Cursor cursor_override = WM_Cursor_COUNT;
      switch(evt.type)
      {
        default:{}break;
        
        //- rjf: key presses/releases
        case KeyPress:
        case KeyRelease:
        {
          // rjf: determine flags
          WM_Modifiers modifiers = 0;
          if(evt.xkey.state & ShiftMask)   { modifiers |= WM_Modifier_Shift; }
          if(evt.xkey.state & ControlMask) { modifiers |= WM_Modifier_Ctrl; }
          if(evt.xkey.state & Mod1Mask)    { modifiers |= WM_Modifier_Alt; }
          
          // rjf: map keycode -> keysym & codepoint
          LNX_WM_Window *window = lnx_window_from_x11window(evt.xkey.window);
          KeySym keysym = 0;
          U8 text[256] = {0};
          U64 text_size = Xutf8LookupString(window->xic, &evt.xkey, (char *)text, sizeof(text), &keysym, 0);
          
          // rjf: map keysym -> WM_Key
          B32 is_right_sided = 0;
          WM_Key key = WM_Key_Null;
          switch(keysym)
          {
            default:
            {
              if(0){}
              else if(XK_F1 <= keysym && keysym <= XK_F24) { key = (WM_Key)(WM_Key_F1 + (keysym - XK_F1)); }
              else if('0' <= keysym && keysym <= '9')      { key = WM_Key_0 + (keysym-'0'); }
            }break;
            case XK_Escape:{key = WM_Key_Esc;};break;
            case XK_BackSpace:{key = WM_Key_Backspace;}break;
            case XK_Delete:{key = WM_Key_Delete;}break;
            case XK_Return:{key = WM_Key_Return;}break;
            case XK_Pause:{key = WM_Key_Pause;}break;
            case XK_Tab:{key = WM_Key_Tab;}break;
            case XK_Left:{key = WM_Key_Left;}break;
            case XK_Right:{key = WM_Key_Right;}break;
            case XK_Up:{key = WM_Key_Up;}break;
            case XK_Down:{key = WM_Key_Down;}break;
            case XK_Home:{key = WM_Key_Home;}break;
            case XK_End:{key = WM_Key_End;}break;
            case XK_Page_Up:{key = WM_Key_PageUp;}break;
            case XK_Page_Down:{key = WM_Key_PageDown;}break;
            case XK_Alt_L:{ key = WM_Key_Alt; }break;
            case XK_Alt_R:{ key = WM_Key_Alt; is_right_sided = 1;}break;
            case XK_Shift_L:{ key = WM_Key_Shift; }break;
            case XK_Shift_R:{ key = WM_Key_Shift; is_right_sided = 1;}break;
            case XK_Control_L:{ key = WM_Key_Ctrl; }break;
            case XK_Control_R:{ key = WM_Key_Ctrl; is_right_sided = 1;}break;
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
            case '\t':{key = WM_Key_Tab;}break;
            case 'a':case 'A':{key = WM_Key_A;}break;
            case 'b':case 'B':{key = WM_Key_B;}break;
            case 'c':case 'C':{key = WM_Key_C;}break;
            case 'd':case 'D':{key = WM_Key_D;}break;
            case 'e':case 'E':{key = WM_Key_E;}break;
            case 'f':case 'F':{key = WM_Key_F;}break;
            case 'g':case 'G':{key = WM_Key_G;}break;
            case 'h':case 'H':{key = WM_Key_H;}break;
            case 'i':case 'I':{key = WM_Key_I;}break;
            case 'j':case 'J':{key = WM_Key_J;}break;
            case 'k':case 'K':{key = WM_Key_K;}break;
            case 'l':case 'L':{key = WM_Key_L;}break;
            case 'm':case 'M':{key = WM_Key_M;}break;
            case 'n':case 'N':{key = WM_Key_N;}break;
            case 'o':case 'O':{key = WM_Key_O;}break;
            case 'p':case 'P':{key = WM_Key_P;}break;
            case 'q':case 'Q':{key = WM_Key_Q;}break;
            case 'r':case 'R':{key = WM_Key_R;}break;
            case 's':case 'S':{key = WM_Key_S;}break;
            case 't':case 'T':{key = WM_Key_T;}break;
            case 'u':case 'U':{key = WM_Key_U;}break;
            case 'v':case 'V':{key = WM_Key_V;}break;
            case 'w':case 'W':{key = WM_Key_W;}break;
            case 'x':case 'X':{key = WM_Key_X;}break;
            case 'y':case 'Y':{key = WM_Key_Y;}break;
            case 'z':case 'Z':{key = WM_Key_Z;}break;
            case ' ':{key = WM_Key_Space;}break;
          }
          
          // rjf: push text event
          if(evt.type == KeyPress && text_size != 0)
          {
            for(U64 off = 0; off < text_size;)
            {
              UnicodeDecode decode = utf8_decode(text+off, text_size-off);
              if(decode.codepoint != 0 && decode.codepoint != 127 && (decode.codepoint >= 32 || decode.codepoint == '\t'))
              {
                WM_Event *e = wm_event_list_push_new(arena, &evts, WM_EventKind_Text);
                e->window.u64[0] = (U64)window;
                e->character = decode.codepoint;
              }
              if(decode.inc == 0)
              {
                break;
              }
              off += decode.inc;
            }
          }
          
          // rjf: push key event
          {
            WM_Event *e = wm_event_list_push_new(arena, &evts, evt.type == KeyPress ? WM_EventKind_Press : WM_EventKind_Release);
            e->window.u64[0] = (U64)window;
            e->modifiers = modifiers;
            e->key = key;
            e->right_sided = is_right_sided;
          }
        }break;
        
        //- rjf: mouse button presses/releases
        case ButtonPress:
        case ButtonRelease:
        {
          // rjf: determine flags
          WM_Modifiers modifiers = 0;
          if(evt.xbutton.state & ShiftMask)   { modifiers |= WM_Modifier_Shift; }
          if(evt.xbutton.state & ControlMask) { modifiers |= WM_Modifier_Ctrl; }
          if(evt.xbutton.state & Mod1Mask)    { modifiers |= WM_Modifier_Alt; }
          
          // rjf: map button -> WM_Key
          WM_Key key = WM_Key_Null;
          switch(evt.xbutton.button)
          {
            default:{}break;
            case Button1:{key = WM_Key_LeftMouseButton;}break;
            case Button2:{key = WM_Key_MiddleMouseButton;}break;
            case Button3:{key = WM_Key_RightMouseButton;}break;
          }
          
          // rjf: push event
          LNX_WM_Window *window = lnx_window_from_x11window(evt.xbutton.window);

          if(evt.type == ButtonPress && evt.xbutton.button == Button1 && window != 0)
          {
            XWindowAttributes atts = {0};
            XGetWindowAttributes(lnx_wm_state->display, window->window, &atts);
            Vec2F32 dim = v2f32((F32)atts.width, (F32)atts.height);
            Vec2F32 p = v2f32((F32)evt.xbutton.x, (F32)evt.xbutton.y);
            S32 dir = lnx_window_moveresize_dir_from_point(window, p, dim);
            if(dir != LNX_WM_MOVERESIZE_NONE)
            {
              lnx_window_begin_moveresize(window, evt.xbutton.x_root, evt.xbutton.y_root, dir, evt.xbutton.button);
              break;
            }
          }

          if(key != WM_Key_Null)
          {
            WM_Event *e = wm_event_list_push_new(arena, &evts, evt.type == ButtonPress ? WM_EventKind_Press : WM_EventKind_Release);
            e->window.u64[0] = (U64)window;
            e->modifiers = modifiers;
            e->key = key;
            e->pos = v2f32((F32)evt.xbutton.x, (F32)evt.xbutton.y);
          }
          else if(evt.xbutton.button == Button4 ||
                  evt.xbutton.button == Button5)
          {
            WM_Event *e = wm_event_list_push_new(arena, &evts, WM_EventKind_Scroll);
            e->window.u64[0] = (U64)window;
            e->modifiers = modifiers;
            e->delta = v2f32(0, evt.xbutton.button == Button4 ? -1.f : +1.f);
            e->pos = v2f32((F32)evt.xbutton.x, (F32)evt.xbutton.y);
          }
        }break;
        
        //- rjf: mouse motion
        case MotionNotify:
        {
          LNX_WM_Window *window = lnx_window_from_x11window(evt.xmotion.window);
          WM_Event *e = wm_event_list_push_new(arena, &evts, WM_EventKind_MouseMove);
          e->window.u64[0] = (U64)window;
          e->pos.x = (F32)evt.xmotion.x;
          e->pos.y = (F32)evt.xmotion.y;
          set_mouse_cursor = 1;

          if(window != 0)
          {
            XWindowAttributes atts = {0};
            XGetWindowAttributes(lnx_wm_state->display, window->window, &atts);
            Vec2F32 dim = v2f32((F32)atts.width, (F32)atts.height);
            Vec2F32 p = v2f32((F32)evt.xmotion.x, (F32)evt.xmotion.y);
            S32 dir = lnx_window_moveresize_dir_from_point(window, p, dim);
            cursor_override = lnx_wm_cursor_from_moveresize_dir(dir);
          }
        }break;
        
        //- rjf: window focus/unfocus
        case FocusIn:
        {
        }break;
        case FocusOut:
        {
          LNX_WM_Window *window = lnx_window_from_x11window(evt.xfocus.window);
          WM_Event *e = wm_event_list_push_new(arena, &evts, WM_EventKind_WindowLoseFocus);
          e->window.u64[0] = (U64)window;
        }break;
        
        case ConfigureNotify:
        case Expose:
        {
          LNX_WM_Window *window = lnx_window_from_x11window(evt.xany.window);
          WM_Event *e = wm_event_list_push_new(arena, &evts, WM_EventKind_Wakeup);
          e->window.u64[0] = (U64)window;
        }break;

        //- rjf: client messages
        case ClientMessage:
        {
          if((Atom)evt.xclient.data.l[0] == lnx_wm_state->wm_delete_window_atom)
          {
            LNX_WM_Window *window = lnx_window_from_x11window(evt.xclient.window);
            WM_Event *e = wm_event_list_push_new(arena, &evts, WM_EventKind_WindowClose);
            e->window.u64[0] = (U64)window;
          }
          else if((Atom)evt.xclient.data.l[0] == lnx_wm_state->wm_sync_request_atom)
          {
            LNX_WM_Window *window = lnx_window_from_x11window(evt.xclient.window);
            if(window != 0)
            {
              S64 value = (S64)(((U64)(U32)evt.xclient.data.l[2]) | ((U64)(U32)evt.xclient.data.l[3] << 32));
              window->counter_value = (U64)value;
              window->sync_request_pending = 1;
              S64 odd = value + (value & 1) + 1;
              window->extended_counter_value = odd;
              window->extended_frame_pending = 1;
              XSyncValue odd_value;
              XSyncIntsToValue(&odd_value, (unsigned)(odd & 0xffffffff), (int)(odd >> 32));
              XSyncSetCounter(lnx_wm_state->display, window->extended_counter_xid, odd_value);
              WM_Event *e = wm_event_list_push_new(arena, &evts, WM_EventKind_Wakeup);
              e->window.u64[0] = (U64)window;
            }
          }
        }break;
      }
      
      if(set_mouse_cursor)
      {
        WM_Cursor cursor = (cursor_override != WM_Cursor_COUNT) ? cursor_override : lnx_wm_state->last_set_cursor;
        Window root_window = 0;
        Window child_window = 0;
        int root_rel_x = 0;
        int root_rel_y = 0;
        int child_rel_x = 0;
        int child_rel_y = 0;
        unsigned int mask = 0;
        if(XQueryPointer(lnx_wm_state->display, XDefaultRootWindow(lnx_wm_state->display), &root_window, &child_window, &root_rel_x, &root_rel_y, &child_rel_x, &child_rel_y, &mask))
        {
          XDefineCursor(lnx_wm_state->display, root_window, lnx_wm_state->cursors[cursor]);
          XFlush(lnx_wm_state->display);
        }
      }
    }
    if(evts.count > 0 || (wait == 0 && evts.count == 0))
    {
      break;
    }
  }
  return evts;
}

internal WM_Modifiers
wm_x11_get_modifiers(void)
{
  // TODO(rjf)
  return 0;
}

internal B32
wm_x11_key_is_down(WM_Key key)
{
  // TODO(rjf)
  return 0;
}

internal Vec2F32
wm_x11_mouse_from_window(WM_Window handle)
{
  if(wm_window_match(handle, wm_window_zero())) {return v2f32(0, 0);}
  LNX_WM_Window *w = (LNX_WM_Window *)handle.u64[0];
  Vec2F32 result = {0};
  {
    Window root_window = 0;
    Window child_window = 0;
    int root_rel_x = 0;
    int root_rel_y = 0;
    int child_rel_x = 0;
    int child_rel_y = 0;
    unsigned int mask = 0;
    if(XQueryPointer(lnx_wm_state->display, w->window, &root_window, &child_window, &root_rel_x, &root_rel_y, &child_rel_x, &child_rel_y, &mask))
    {
      result.x = child_rel_x;
      result.y = child_rel_y;
    }
  }
  return result;
}

////////////////////////////////
//~ rjf: @per_os_impl Cursors (Implemented Per-OS)

internal void
wm_x11_set_cursor(WM_Cursor cursor)
{
  lnx_wm_state->last_set_cursor = cursor;
}

////////////////////////////////
//~ rjf: @per_os_impl Native User-Facing Graphical Messages (Implemented Per-OS)

internal void
wm_x11_graphical_message(B32 error, String8 title, String8 message)
{
  if(error)
  {
    fprintf(stderr, "[X] ");
  }
  fprintf(stderr, "%.*s\n", str8_varg(title));
  fprintf(stderr, "%.*s\n\n", str8_varg(message));
}

internal String8
wm_x11_graphical_pick_file(Arena *arena, String8 title, String8 initial_path)
{
  return str8_zero();
}

////////////////////////////////
//~ rjf: @per_os_impl Shell Operations

internal void
wm_x11_show_in_filesystem_ui(String8 path)
{
  // TODO(rjf)
}

internal void
wm_x11_open_in_browser(String8 url)
{
  // TODO(rjf)
}

////////////////////////////////
//~ rjf: xdg-shell / xdg-decoration Wire Data
//
// Interface message tables the wl_proxy marshalling needs. Transcribed from
// wayland-scanner output. Slots for requests we never send (xdg positioner /
// popup) are left null.

static const struct wl_interface *xdg_shell_types[] =
{
  0, 0, 0, 0,
  0,
  &xdg_surface_interface,
  &wl_surface_interface,
  &xdg_toplevel_interface,
  0,
  &xdg_surface_interface,
  0,
  &xdg_toplevel_interface,
  &wl_seat_interface,
  0, 0, 0,
  &wl_seat_interface,
  0,
  &wl_seat_interface,
  0, 0,
  &wl_output_interface,
  &wl_seat_interface,
  0,
  0,
  0,
};

static const struct wl_message xdg_wm_base_requests[] =
{
  { "destroy", "", xdg_shell_types + 0 },
  { "create_positioner", "n", xdg_shell_types + 4 },
  { "get_xdg_surface", "no", xdg_shell_types + 5 },
  { "pong", "u", xdg_shell_types + 0 },
};
static const struct wl_message xdg_wm_base_events[] =
{
  { "ping", "u", xdg_shell_types + 0 },
};
const struct wl_interface xdg_wm_base_interface = { "xdg_wm_base", 7, 4, xdg_wm_base_requests, 1, xdg_wm_base_events };

static const struct wl_message xdg_surface_requests[] =
{
  { "destroy", "", xdg_shell_types + 0 },
  { "get_toplevel", "n", xdg_shell_types + 7 },
  { "get_popup", "n?oo", xdg_shell_types + 8 },
  { "set_window_geometry", "iiii", xdg_shell_types + 0 },
  { "ack_configure", "u", xdg_shell_types + 0 },
};
static const struct wl_message xdg_surface_events[] =
{
  { "configure", "u", xdg_shell_types + 0 },
};
const struct wl_interface xdg_surface_interface = { "xdg_surface", 7, 5, xdg_surface_requests, 1, xdg_surface_events };

static const struct wl_message xdg_toplevel_requests[] =
{
  { "destroy", "", xdg_shell_types + 0 },
  { "set_parent", "?o", xdg_shell_types + 11 },
  { "set_title", "s", xdg_shell_types + 0 },
  { "set_app_id", "s", xdg_shell_types + 0 },
  { "show_window_menu", "ouii", xdg_shell_types + 12 },
  { "move", "ou", xdg_shell_types + 16 },
  { "resize", "ouu", xdg_shell_types + 18 },
  { "set_max_size", "ii", xdg_shell_types + 0 },
  { "set_min_size", "ii", xdg_shell_types + 0 },
  { "set_maximized", "", xdg_shell_types + 0 },
  { "unset_maximized", "", xdg_shell_types + 0 },
  { "set_fullscreen", "?o", xdg_shell_types + 21 },
  { "unset_fullscreen", "", xdg_shell_types + 0 },
  { "set_minimized", "", xdg_shell_types + 0 },
};
static const struct wl_message xdg_toplevel_events[] =
{
  { "configure", "iia", xdg_shell_types + 0 },
  { "close", "", xdg_shell_types + 0 },
  { "configure_bounds", "4ii", xdg_shell_types + 0 },
  { "wm_capabilities", "5a", xdg_shell_types + 0 },
};
const struct wl_interface xdg_toplevel_interface = { "xdg_toplevel", 7, 14, xdg_toplevel_requests, 4, xdg_toplevel_events };

static const struct wl_interface *xdg_decoration_types[] =
{
  0,
  &zxdg_toplevel_decoration_v1_interface,
  &xdg_toplevel_interface,
};
static const struct wl_message zxdg_decoration_manager_v1_requests[] =
{
  { "destroy", "", xdg_decoration_types + 0 },
  { "get_toplevel_decoration", "no", xdg_decoration_types + 1 },
};
const struct wl_interface zxdg_decoration_manager_v1_interface = { "zxdg_decoration_manager_v1", 1, 2, zxdg_decoration_manager_v1_requests, 0, 0 };

static const struct wl_message zxdg_toplevel_decoration_v1_requests[] =
{
  { "destroy", "", xdg_decoration_types + 0 },
  { "set_mode", "u", xdg_decoration_types + 0 },
  { "unset_mode", "", xdg_decoration_types + 0 },
};
static const struct wl_message zxdg_toplevel_decoration_v1_events[] =
{
  { "configure", "u", xdg_decoration_types + 0 },
};
const struct wl_interface zxdg_toplevel_decoration_v1_interface = { "zxdg_toplevel_decoration_v1", 1, 3, zxdg_toplevel_decoration_v1_requests, 1, zxdg_toplevel_decoration_v1_events };

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
      else if(XK_F1 <= keysym && keysym <= XK_F24) { key = (WM_Key)(WM_Key_F1 + (keysym - XK_F1)); }
      else if('0' <= keysym && keysym <= '9')                { key = WM_Key_0 + (keysym-'0'); }
      else if('a' <= keysym && keysym <= 'z')                { key = WM_Key_A + (keysym-'a'); }
      else if('A' <= keysym && keysym <= 'Z')                { key = WM_Key_A + (keysym-'A'); }
    }break;
    case XK_Escape:{key = WM_Key_Esc;}break;
    case XK_BackSpace:{key = WM_Key_Backspace;}break;
    case XK_Delete:{key = WM_Key_Delete;}break;
    case XK_Return:{key = WM_Key_Return;}break;
    case XK_Pause:{key = WM_Key_Pause;}break;
    case XK_Tab:{key = WM_Key_Tab;}break;
    case XK_Left:{key = WM_Key_Left;}break;
    case XK_Right:{key = WM_Key_Right;}break;
    case XK_Up:{key = WM_Key_Up;}break;
    case XK_Down:{key = WM_Key_Down;}break;
    case XK_Home:{key = WM_Key_Home;}break;
    case XK_End:{key = WM_Key_End;}break;
    case XK_Page_Up:{key = WM_Key_PageUp;}break;
    case XK_Page_Down:{key = WM_Key_PageDown;}break;
    case XK_Alt_L:{key = WM_Key_Alt;}break;
    case XK_Alt_R:{key = WM_Key_Alt; is_right_sided = 1;}break;
    case XK_Shift_L:{key = WM_Key_Shift;}break;
    case XK_Shift_R:{key = WM_Key_Shift; is_right_sided = 1;}break;
    case XK_Control_L:{key = WM_Key_Ctrl;}break;
    case XK_Control_R:{key = WM_Key_Ctrl; is_right_sided = 1;}break;
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
  wl_wm_state->pointer_pos = v2f32((F32)wl_fixed_to_double(sx), (F32)wl_fixed_to_double(sy));
  wl_apply_cursor(wl_wm_state->last_set_cursor);
}

internal void
wl_pointer_leave(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface)
{
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
}

internal void
wl_keyboard_leave(void *data, struct wl_keyboard *kb, uint32_t serial, struct wl_surface *surface)
{
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
