# Apps

Apps live in `src/apps/`, one `.c`/`.h` pair each. The built-in apps are
Terminal, Files, Notepad and About, plus a "Shut down" launcher entry. At
the end of this page there's a step-by-step tutorial for writing your own.

## Anatomy of an app

An app is plain kernel C code. There are no processes, so "running an app"
means "a window whose callbacks point into the app's file". Every app
follows the same shape:

1. **State struct.** Everything one window of the app needs, allocated with
   `kmalloc` in `<name>_open()` and stored in `win->data`. Because the state
   lives per window, several windows of the same app don't interfere.
2. **`<name>_open()`.** Calls `window_create(title, width, height)`, sets
   `win->data` and the callbacks it needs. This is the only public function,
   declared in `<name>.h`.
3. **Callbacks** (see `GUI.md` for the full list):
   - `on_paint` draws the whole content into `win->surface`. It starts
     cleared to the window background color.
   - `on_key` and `on_mouse` change the state, then call
     `window_invalidate(win)` so `on_paint` runs on the next frame.
   - `on_timer` runs once a second, if you need it.
   - `on_close` frees `win->data`.
4. **One line in `apps/apps.c`** puts it in the launcher menu.

Apps never draw outside their own surface, never touch the screen directly,
and never block. A callback must return quickly, because the whole desktop
waits for it.

## Terminal — `apps/terminal.c`

**What it does.** Runs the command shell (`shell/shell.c`) in a window.
It's the same shell as text mode, with the same commands (`help`, `ls`,
`cat`, `write`, `del`, `info`, `clear`, `echo`, `date`, `exec`, `quit`).

**How it works.** The window keeps an 80x24 grid of characters and colors
plus a cursor. That grid is a `console_t` sink (`lib/console.h`):

- `grid_put_char` writes a cell, wraps at column 80, handles `\n`
  and `\b`, and scrolls the grid up when the cursor falls off the bottom,
- `grid_clear` blanks the grid.

`terminal_open` creates the sink and then a shell on it
(`shell_create(&t->console)`). Each typed key goes to `shell_key()`, which
may print through the sink, and the window is then invalidated, so
`terminal_paint` redraws the grid. Colors are VGA palette indexes, mapped
to RGB with the standard 16-color VGA palette, so the rainbow banner looks
the same as in text mode.

Each Terminal window has its own `shell_t`, so several can run side by
side.

**The logo.** When a shell starts (a new Terminal window, or the text-mode
shell at boot), `shell_create` prints the planckOS logo, an infinity sign
wearing a fedora, before the banner. `clear` doesn't print it again. The
art lives in `shell/logo.c` as a table of `{ text, color }` rows, one per
line, so you can edit the picture or recolor a row right there. The rows
are centered on each other, and `logo_print()` adds the left margin that
centers the 50-column logo on an 80-column screen. With the logo, banner
and prompt, a new Terminal fills its 24 rows exactly. If you make the logo
taller, the top will scroll off.

**Extending it.** New shell commands go in the `commands` table in
`shell/shell.c`: write `cmd_name(shell_t* sh, const char* args)` that
prints with `console_printf(sh->con, ...)`, and add a row. It then works in
both text mode and every Terminal window. Scrollback would mean keeping more
rows than `TERM_ROWS` and drawing a window onto them.

**Known limits.** No scrollback. Fixed 80x24. No cursor keys.

## Notepad — `apps/notepad.c`

**What it does.** Edits a text file of up to 8 KB and saves it to the FAT12
disk. Launching it from the menu gives an empty, unnamed document. The
Files app opens existing files in it (`notepad_open_file(name)`).

**How it works.**

- *State.* The text lives in a fixed buffer in the app's `notepad_state`,
  with its length. A `textbox_t` holds the file name, and `focus` says
  whether typing goes to the name or the text. Clicking either one moves
  the focus there.
- *Editing.* Typing appends at the end. Enter adds `'\n'` and backspace
  removes the last character. In the name box, Enter saves.
- *Drawing.* `text_layout()` walks the text the way it appears on screen,
  starting a new row on `'\n'` or when a row is full (wrapping). It runs
  twice: once only to count rows, then to draw. If there are more rows than
  fit, drawing starts further down so the end of the text (where you type)
  stays visible.
- *Loading.* `fat12_read` is asked for one byte more than the 8 KB buffer.
  If it gets that byte, the file was too big, and Notepad says only the
  first 8 KB were loaded.
- *Saving.* `fat12_write(name, text, length)`. The result is shown next to
  the Save button, and the window title changes to the file name.

**Extending it.** An insertion point (an index into `text`) plus arrow key
events would allow editing in the middle. Insert and delete would then
shift the rest of the buffer with a loop or `memmove`.

**Known limits.** You can only edit at the end (no cursor movement: the
keyboard driver doesn't report arrow keys yet). No scrolling back to
earlier lines in long files. 8 KB maximum.

## Files — `apps/files.c`

**What it does.** Lists the files on the FAT12 disk with their sizes.
Click to select, double-click or press **Open** to edit in Notepad,
**Delete** to remove the selected file, and **Up/Down** to page through
long lists.

**How it works.**

- *The list.* `files_reload()` calls `fat12_list()` into a temporary
  buffer and compares it with what's shown. Only if something changed does
  it copy it over and rebuild the display lines (`"NAME.TXT      123 B"`).
  It runs once a second from `on_timer`, so files written from Notepad or a
  Terminal appear by themselves, and the window only repaints when the
  directory actually changed.
- *Double-click.* There's no double-click event. Files remembers which row
  was clicked last and at which `timer_ticks()`. A second click on the same
  row within half a second (`DOUBLE_CLICK_TICKS`) counts as a double-click.
- *Opening* calls `notepad_open_file(name)`. That's how apps use each
  other: through the other app's public header.

**Extending it.** A "New" button could call `notepad_open()`. A rename
would need a new `fat12_rename` that only rewrites the 8.3 name in the
directory entry.

**Known limits.** Up to 128 files are shown. Delete doesn't ask for
confirmation.

## About — `apps/about.c`

**What it does.** Shows the OS name and version, the kernel name, the
author, the uptime, and how much memory is used and free.

**How it works.** `about_paint` draws a column of labels and values. The
numbers come from `timer_format_uptime()`, `pmm_stats()` (physical pages,
shown in MB) and `heap_stats()` (heap chunks, shown in KB). The app has no
state of its own, so it needs no `on_close`. Its `on_timer` just
invalidates the window, so the figures refresh every second.

Note that "RAM used" includes the whole 16 MB heap, which is reserved in
the PMM up front, and "Heap used" includes the 3 MB screen back buffer.

**Extending it.** Add a row: one more `about_row(s, 6, "Name:", value)`, and
make the window taller.

## Shut down

The last launcher entry calls `power_off()` (`drivers/power.c`), which is
also what the shell's `quit` command does. It writes the ACPI "sleep type
5, enable" value `0x2000` to the I/O port that QEMU (`0x604`) and Bochs
(`0xB004`) treat as their power button. Real hardware would need the port
from the ACPI tables, so there the machine just halts.

## Tutorial: add your own app

We'll build **Hello**: a window with a greeting, a button and a click
counter. It takes about 70 lines and touches three files.

### Step 1: the header

Create `src/apps/hello.h`. It exposes one function, the one the launcher
calls:

```c
/*
 * hello.h - a tiny example app.
 */
#ifndef hello_h__
#define hello_h__

void hello_open(void);

#endif
```

### Step 2: the app

Create `src/apps/hello.c`:

```c
/*
 * hello.c - a tiny example app: a greeting, a button and a click counter.
 */
#include "apps/hello.h"
#include "gui/wm.h"
#include "gui/widget.h"
#include "mm/kmalloc.h"
#include "lib/string.h"

#define HELLO_WIDTH     260
#define HELLO_HEIGHT    110
#define NUMBER_MAX      12
#define LINE_MAX        32

static const rect_t button_rect = { 10, 70, 100, 26 };

typedef struct {
    uint32_t clicks;
} hello_state;

static void hello_paint(window_t* win) {
    hello_state* state = win->data;
    char line[LINE_MAX];
    char number[NUMBER_MAX];

    widget_label(&win->surface, 10, 10, "Hello from " OS_NAME "!");
    strcpy(line, "Clicks: ");
    strcat(line, utoa(state->clicks, number, 10));
    widget_label(&win->surface, 10, 36, line);
    widget_button(&win->surface, button_rect, "Click me");
}

static void hello_mouse(window_t* win, const event_t* e) {
    hello_state* state = win->data;

    if (e->type == EVENT_MOUSE_DOWN && rect_contains(button_rect, e->x, e->y)) {
        state->clicks++;
        window_invalidate(win);
    }
}

static void hello_close(window_t* win) {
    kfree(win->data);
}

void hello_open(void) {
    hello_state* state = kmalloc(sizeof(hello_state));
    if (!state) {
        return;
    }
    window_t* win = window_create("Hello", HELLO_WIDTH, HELLO_HEIGHT);
    if (!win) {
        kfree(state);
        return;
    }
    state->clicks = 0;

    win->data = state;
    win->on_paint = hello_paint;
    win->on_mouse = hello_mouse;
    win->on_close = hello_close;
}
```

Walking through it:

- **`hello_state`** holds the click count. `hello_open` allocates it,
  stores it in `win->data`, and `hello_close` frees it when the window
  closes.
- **`button_rect`** is where the button sits, in *content* coordinates
  (`(0, 0)` is the top-left of the area under the title bar). The same
  rectangle is used to draw the button and to test clicks, so they can't
  drift apart.
- **`hello_paint`** draws everything from scratch each time. You never
  erase anything yourself: the window manager clears the surface before
  calling `on_paint`.
- **`hello_mouse`** gets mouse events in content coordinates too. On a
  press inside the button it changes the state and calls
  `window_invalidate()`. That's the only way to get repainted. Changing the
  state alone shows nothing.
- **Error handling.** If `kmalloc` or `window_create` fails, `hello_open`
  cleans up and returns. Nothing else needs to know.

### Step 3: register it

In `src/apps/apps.c`, include the header and add one line to `app_list`:

```c
#include "apps/hello.h"
...
const app_entry app_list[] = {
    { "Terminal",  terminal_open },
    { "Files",     files_open },
    { "Notepad",   notepad_open },
    { "About",     about_open },
    { "Hello",     hello_open },     /* <- new */
    { "Shut down", power_off },
};
```

You don't need to touch the Makefile: it compiles every `.c` file under
`src/`.

### Step 4: build and run

```sh
make run
```

Click **planckOS** on the taskbar, choose **Hello**, and click the button.
The counter goes up, and the taskbar shows a "Hello" button. You can open
several Hello windows: each has its own counter, because each has its own
`hello_state`.

### Ideas to try next

- **React to keys:** add `win->on_key = hello_key;` and reset the counter
  when `key == 'r'`.
- **Show the time:** set `win->on_timer` to a function that only calls
  `window_invalidate(win)`, and draw `timer_format_uptime()` in
  `hello_paint` (see `apps/about.c`).
- **Use the disk:** save the count with `fat12_write("HELLO.TXT", ...)` and
  load it in `hello_open` with `fat12_read`. Check the `fat12_status` you
  get back.
- **Add a shell command** instead of an app: see the Terminal section
  above.
