# GUI

The GUI code lives in `src/gui/`. It runs entirely in normal kernel
context. Interrupt handlers only feed it events (see `INPUT.md`).

## Main loop — `gui/gui.c`

**What it does.** Owns the screen once graphics are up: waits for input,
hands it out, and draws new frames.

**How it works.** `kmain` calls `gui_main_loop()` when `fb_init()` found a
framebuffer. The loop never returns:

```
for (;;) {
    event_wait(&event);                 // sleep (hlt) until input arrives
    changed = dispatch(event);          // and every other queued event
    if (changed) {
        compose();                      // desktop, windows, taskbar, menu, cursor
        fb_present();                   // show the new frame
    }
}
```

Taking all queued events before drawing means a fast mouse that queued 20
moves causes one redraw, not 20. A frame is composed back to front: the
desktop color, every window from bottom to top (`wm_draw`), the taskbar,
the launcher menu, and finally the mouse cursor.

`gui_dispatch` decides who gets each event:

| Event | Goes to |
|-------|---------|
| `EVENT_MOUSE_DOWN` | the launcher menu, else the taskbar, else the window manager |
| `EVENT_MOUSE_MOVE` / `_UP` | the menu (for hover) and the window manager |
| `EVENT_KEY` | the window manager, which passes it to the focused window |
| `EVENT_TIMER` | the taskbar clock (`taskbar_update_clock`) and the window manager (every window's `on_timer`); the screen always redraws |

**Key functions.** `gui_main_loop()`. Everything else is static.

**Extending it.** A desktop background image would be a `surface_blit`
instead of the first `surface_fill_rect` in `gui_compose`. Keyboard
shortcuts (e.g. a key that cycles windows) would be checked in
`gui_dispatch` before the event reaches the window manager.

**Known limits.** Every change redraws and presents the whole screen.

## Window manager — `gui/wm.c`

**What it does.** Keeps the open windows in z-order, draws their frames,
and handles focus, raising, dragging by the title bar and the close button.

**A window** (`window_t` in `wm.h`) has:

| Field | Meaning |
|-------|---------|
| `title` | Shown in the title bar and on the taskbar. |
| `x`, `y` | Top-left of the frame on screen. |
| `surface` | The content pixels. Its width/height is the content size. |
| `dirty` | The content needs repainting. Set it with `window_invalidate()`. |
| `data` | The app's own state, which the app allocates and frees. |
| `on_paint(win)` | Draw the content into `win->surface`. It starts cleared to the window background. |
| `on_key(win, key)` | A key was typed while this window had focus. |
| `on_mouse(win, e)` | Press/release in the content, and moves while the button is held. `e->x/y` are relative to the content's top-left. |
| `on_close(win)` | The window is closing: free `win->data`. |
| `on_timer(win)` | Called once a second (on `EVENT_TIMER`). Call `window_invalidate` if the content should change. |

All callbacks are optional.

**Frame layout.**

```
(x, y) +-----------------------------------+
       | title bar (22 px)            [x]  |
       +-----------------------------------+
       | content = win->surface            |
       +-----------------------------------+   1 px border all round
```

**How it works.**

- *Z-order.* `windows[]` runs bottom to top. The last entry is on top and
  has keyboard focus. `window_raise()` moves a window to the end.
- *Own buffers.* Each window paints into its own surface. `wm_draw()` calls
  `on_paint` only for dirty windows, then copies every buffer onto the
  screen. Moving a window or the mouse never makes an app repaint, which
  is how "redraw only on change" works at the app level.
- *Mouse.* A left press raises the topmost window under the pointer. Then:
  on the close button it closes the window; on the title bar it starts a
  drag (remembering where in the frame you grabbed it); in the content it
  goes to `on_mouse`, and that window "owns" the mouse until the button is
  released, so drags inside an app keep working when the pointer leaves it.
  While dragging, the window follows the pointer but is clamped so its title
  bar stays reachable.
- *Keys* go to the focused window's `on_key`.
- New windows are placed in a cascade (each 30 px further down and right).

**Key functions.**

| Function | Purpose |
|----------|---------|
| `window_create(title, w, h)` | Open a window with a `w x h` content area. Fill in the callbacks afterwards. |
| `window_close(win)` | Call `on_close`, free it, remove it. Safe to call from the window's own callbacks. |
| `window_raise(win)` | Bring to front and focus. |
| `window_set_title(win, title)` | Change the title bar text. |
| `window_invalidate(win)` | Repaint the content on the next frame. |
| `wm_handle_event(e)` | Route an input event; returns whether the screen changed. |
| `wm_draw(screen)` | Paint dirty windows and draw all of them. |

**Extending it.** Minimizing would mean a `hidden` flag that `wm_draw` and
`window_at_point` skip, with the taskbar button un-hiding it. Resizing
would reallocate `win->surface` and mark it dirty.

**Known limits.** At most 16 windows. Fixed size. No minimize/maximize.
The whole screen is recomposed whenever anything changes.

## Mouse cursor — `gui/cursor.c`

The arrow is stored as ASCII art (`X` outline, `.` fill, space =
transparent), so you can edit its shape directly in the source.
`cursor_draw(surface, x, y)` puts the tip at `(x, y)` and is called last
in every redraw, so it always sits on top. There's no "save what was under
the cursor" trick: the whole frame is redrawn from scratch, so the old
cursor position simply gets painted over. `surface_put_pixel` clips, so the
cursor can hang off the right or bottom edge.

**Extending it.** Different cursor shapes (a text I-beam over text boxes,
say) would be more ASCII-art tables plus a way for windows to choose one.

## Taskbar — `gui/taskbar.c`

**What it does.** The 28 px bar at the bottom of the screen, from left to
right: the launcher button, one button per open window, and a clock
showing the time of day (`HH:MM`).

**How it works.**

- *Window buttons* are listed in the order the windows were opened. Each
  window gets an increasing `id` in `window_create`, and the taskbar sorts
  by it. That keeps buttons from jumping around when z-order changes. The
  focused window's button is highlighted, and clicking a button raises its
  window.
- *Clicks.* `gui_dispatch` offers every mouse press to
  `taskbar_mouse_down()` first. If the press was on the bar, the taskbar
  keeps it; otherwise it goes to the window manager. Windows can't be
  dragged under the bar (`wm_init` gets the screen height minus the bar).
- *The clock.* The PIT handler pushes an `EVENT_TIMER` once a second. On
  that event `gui_dispatch` calls `taskbar_update_clock()`, which reads the
  CMOS real-time clock (`drivers/rtc.c`) and keeps the `"HH:MM"` text.
  `taskbar_draw` only draws that cached text. The taskbar is redrawn on
  every mouse move, and reading the RTC each time would mean slow port I/O
  per frame.

**Extending it.** A minimize feature would hide windows and use the taskbar
buttons to bring them back. Clicking the clock could open a small calendar
window built from `rtc_read()`.

**Known limits.** Buttons don't shrink, so only about five window buttons
fit at 1024 px.

## Launcher menu — `gui/menu.c` and `apps/apps.c`

**What it does.** Clicking the `planckOS` button on the taskbar pops up a
menu listing every registered app. Clicking an entry opens that app.

**How it works.** The app list is a plain array in `apps/apps.c`:

```c
const app_entry app_list[] = {
    { "Terminal",  terminal_open },
    { "Files",     files_open },
    { "Notepad",   notepad_open },
    { "About",     about_open },
    { "Shut down", power_off },
};
```

An entry is just a name and a `void (*)(void)`, so "Shut down" can call
`power_off()` directly.

The menu draws one 24 px row per entry, right above the launcher button,
and highlights the row under the pointer (`menu_mouse_move`). Every mouse
press goes to `menu_mouse_down()` first:

- on the launcher button: toggle the menu,
- on a row: close the menu and call that app's `open()`,
- anywhere else: close the menu and let the taskbar or windows handle the
  press as usual.

The menu owns the launcher button's clicks (the taskbar only draws it).
Otherwise one click would close the menu and the taskbar would reopen it
straight away.

**Extending it.** Add one line to `app_list`. `APPS.md` walks through
writing a new app.

## Widgets — `gui/widget.c`

**What it does.** Small helpers for drawing common controls inside a
window. It's deliberately *not* a toolkit: there are no widget objects, no
layout, no event routing. An app decides where each control goes (a
`rect_t` in content coordinates), keeps its own state, draws the controls
in `on_paint`, and checks clicks itself in `on_mouse`.

| Helper | Draws / does |
|--------|--------------|
| `widget_label(s, x, y, text)` | Black text. |
| `widget_button(s, rect, label)` | A grey button with centered text. Check clicks with `rect_contains(rect, e->x, e->y)`. |
| `widget_textbox(s, rect, &box, focused)` | A one-line text field with a caret. If the text is too long, its end is shown. |
| `widget_textbox_key(&box, key)` | Appends printable characters, handles backspace; returns `true` if the text changed. |
| `widget_list(s, rect, items, count, first, selected)` | Rows of strings, 20 px each, starting at item `first`, with `selected` highlighted. |
| `widget_list_hit(rect, count, first, x, y)` | Which item is at a click position (`-1` for none). |

A typical pattern (simplified from `apps/files.c`):

```c
static void files_mouse(window_t* win, const event_t* e) {
    files_state* f = win->data;
    if (e->type != EVENT_MOUSE_DOWN) {
        return;
    }
    int index = widget_list_hit(list_rect, f->count, f->first, e->x, e->y);
    if (index >= 0) {
        f->selected = index;
    } else if (rect_contains(delete_rect, e->x, e->y)) {
        files_delete_selected(f);
    }
    window_invalidate(win);     /* repaint with the new state */
}
```

**Extending it.** Keep new helpers in the same style: a draw function taking
a surface and a `rect_t`, plus, if needed, a function that interprets a
click or key and returns what changed. A checkbox, for example, would be
`widget_checkbox(s, r, label, checked)` with clicks checked through
`rect_contains`.

**Known limits.** No pressed/hover look for buttons. Text boxes only edit at
the end (no cursor movement or selection). Lists don't draw a scrollbar;
the app passes `first` to scroll.
