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
