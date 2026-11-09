# GUI

The GUI code lives in `src/gui/`. It runs entirely in normal kernel
context. Interrupt handlers only feed it events (see `INPUT.md`).

## Main loop — `gui/gui.c`

`kmain` calls `gui_main_loop()` when `fb_init()` found a framebuffer. The
loop never returns:

```
for (;;) {
    event_wait(&event);                 // sleep (hlt) until input arrives
    changed = dispatch(event);          // and every other queued event
    if (changed) {
        compose();                      // desktop, windows, cursor
        fb_present();                   // show the new frame
    }
}
```

Taking all queued events before drawing means a fast mouse that queued 20
moves causes one redraw, not 20. A frame is composed back to front: the
desktop color, then every window from bottom to top (`wm_draw`), then the
mouse cursor.

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

A typical pattern (from `apps/demo.c`):

```c
static void demo_mouse(window_t* win, const event_t* e) {
    demo_state* state = win->data;
    if (e->type != EVENT_MOUSE_DOWN) {
        return;
    }
    int index = widget_list_hit(list_rect, FRUIT_COUNT, 0, e->x, e->y);
    if (index >= 0) {
        state->selected = index;
        window_invalidate(win);     /* repaint with the new selection */
    }
}
```

**Known limits.** No pressed/hover look for buttons. Text boxes only edit at
the end (no cursor movement or selection). Lists don't draw a scrollbar;
the app passes `first` to scroll.
