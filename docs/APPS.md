# Apps

Apps live in `src/apps/`, one file each. An app is a function
`<name>_open()` that creates a window and fills in its callbacks (see
`GUI.md` for `window_t`). It is registered with one line in
`apps/apps.c`, which puts it in the launcher menu.

## Terminal — `apps/terminal.c`

**What it does.** Runs the command shell (`shell/shell.c`) in a window.
It's the same shell as text mode, with the same commands (`help`, `ls`,
`cat`, `write`, `del`, `info`, `clear`, `echo`, `quit`).

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
