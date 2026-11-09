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

**Known limits.** You can only edit at the end (no cursor movement: the
keyboard driver doesn't report arrow keys yet). No scrolling back to
earlier lines in long files. 8 KB maximum.
