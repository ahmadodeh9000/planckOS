# Windows for programs

Files: `src/exec/progwin.c` (the window table and the callbacks),
`src/exec/program.c` + `program.s` (calling into a program),
`src/arch/syscall.c` (the system calls), `programs/planck.h` (the same
calls for C programs).

## What it does

A program started with `exec` can open windows, draw in them, and react to
keys, clicks and a once-a-second timer. Its windows look and behave like
the built-in apps: they can be dragged, raised and closed, and they appear
on the taskbar.

## Calling the kernel: the register convention

Every request goes through `int 0x80`:

| Register | Holds |
|----------|-------|
| `eax` | the call number (in), the result if there is one (out) |
| `ebx`, `ecx`, `edx` | arguments 1, 2, 3 |
| `esi`, `edi` | arguments 4, 5 |

Every register except `eax` keeps its value across the call. The
interrupt stub saves them all with `pusha` and restores them with `popa`.
Coordinates and sizes come in pairs, packed into one register as two
16-bit halves: `x|y` means `(x << 16) | (y & 0xFFFF)`. Each half is a signed
16-bit number, so `x = -5` works (C programs use `PACK16(x, y)` from
`planck.h`).

## The system calls

| # | Name | Arguments | Result |
|---|------|-----------|--------|
| 0 | `SYS_PRINT` | `ebx` = text | |
| 1 | `SYS_MALLOC` | `ebx` = size | `eax` = pointer |
| 2 | `SYS_FREE` | `ebx` = pointer | |
| 3 | `SYS_EXIT` | `ebx` = exit code | doesn't return |
| 4 | `SYS_PRINT_HEX` | `ebx` = value | |
| 5 | `SYS_PRINT_NUMBER` | `ebx` = value (signed decimal) | |
| 6 | `SYS_WIN_CREATE` | `ebx` = title, `ecx` = width, `edx` = height | `eax` = window id, or 0 |
| 7 | `SYS_WIN_ON` | `ebx` = id, `ecx` = event, `edx` = function | `eax` = 1 ok, 0 refused |
| 8 | `SYS_WIN_RECT` | `ebx` = id, `ecx` = x\|y, `edx` = w\|h, `esi` = color | |
| 9 | `SYS_WIN_TEXT` | `ebx` = id, `ecx` = x\|y, `edx` = text, `esi` = color | |
| 10 | `SYS_WIN_INVALIDATE` | `ebx` = id | |
| 11 | `SYS_WIN_CLOSE` | `ebx` = id | |
| 12 | `SYS_WIN_LINE` | `ebx` = id, `ecx` = x0\|y0, `edx` = x1\|y1, `esi` = color | |
| 13 | `SYS_TIME` | none | `eax` = `(hour << 16) \| (minute << 8) \| second` |

Colors are `0x00RRGGBB`. Windows are at most 800x600. A program can have
at most 4 windows (`PROGWIN_MAX`).

**Window ids.** A program never sees a `window_t*`. It gets an id from 1 to
4, an index into `progwin.c`'s `slots[]` table, and every call checks it
(`progwin_get`). A wrong or stale id is simply ignored, so a buggy program
can't make the kernel follow a random pointer.

## The callback model

`SYS_WIN_ON` registers one of the program's functions for an event:

| Event | # | The function is called as | When |
|-------|---|---------------------------|------|
| `WIN_EVENT_PAINT` | 0 | `paint(id)` | the window needs to be drawn |
| `WIN_EVENT_KEY` | 1 | `key(id, key)` | a key is typed while it has focus |
| `WIN_EVENT_MOUSE` | 2 | `mouse(id, x, y)` | the left button is pressed in it (window coordinates) |
| `WIN_EVENT_TIMER` | 3 | `timer(id)` | once a second |

The function must be inside the program area, or `SYS_WIN_ON` refuses it.
Arguments arrive like a normal C call: the first at `[esp+4]`, the second
at `[esp+8]`, and so on. A callback returns with `ret`.

**How a callback reaches the program.** `_start` usually just opens its
windows, registers its callbacks and returns. The program's code stays in
the program area, and from then on the window manager drives it: when a
window event arrives, the `progwin_*` handler (for example
`progwin_mouse`) calls `program_callback(fn, id, a1, a2)`. That uses the
same `program_call` as `exec`, which pushes the three arguments and calls
`fn`. A crash or `SYS_EXIT` inside a callback unwinds back to the window
manager the same way it unwinds back to the shell.

**Drawing only happens in paint.** `SYS_WIN_RECT`, `SYS_WIN_TEXT` and
`SYS_WIN_LINE` only
work while that window's own paint callback is running (`progwin.c`
checks `painting`). Everywhere else they do nothing. To change what's on
screen, a key, mouse or timer callback changes the program's variables
and calls `SYS_WIN_INVALIDATE`. Soon after, paint runs and draws
everything from those variables. That's the same model the built-in apps
use (`GUI.md`), and it's needed because of how windows are drawn:

- Before every paint, the window manager clears the window's pixel buffer.
  Paint must redraw everything, so "draw a bit more" from a click handler
  would be lost at the next repaint anyway.
- A window can be repainted at any time (for example when it's
  uncovered), and only the program knows what it should look like. If its
  state lives in variables and paint draws from them, the result is the
  same every time.
- The window manager composes the screen once per frame. Drawing in the
  middle of handling a click would draw into a buffer that isn't being
  shown.

## Lifetime rules

A program's code has to stay in place while its windows can still call it.
`_start` returning doesn't end a windowed program: its windows keep
calling into the program area. That gives three rules:

1. **One program with windows at a time.** Loading another program would
   overwrite the code the old windows point to. So `exec` checks
   `progwin_owner()` first and refuses with `exec: close click.elf's
   windows first` (the name as it was typed) while any of the old
   program's windows are open. When
   the last one closes (close button or `SYS_WIN_CLOSE`), `exec` works
   again.
2. **A crash ends the whole program.** If any callback crashes (or
   `_start` crashes after opening windows), `progwin_program_crashed()`
   marks the program dead, closes all its windows, and logs it to serial:
   ```
   EXCEPTION 6 (Invalid Opcode) at eip 0x2000080, error 0x0
   progwin: paint callback of window 1 crashed
   progwin: paintbad.elf crashed, closing its 1 window(s)
   ```
   A dead program is never called again, even if one of its windows is
   still waiting for its paint or timer.
3. **Closing is deferred.** Both `SYS_WIN_CLOSE` and the crash cleanup use
   `window_request_close`, which only marks the window and lets the window
   manager close it at the start of the next frame. A direct close would
   be a bug here, because paint and timer callbacks run while the window
   manager is walking its window list (see "Closing safely" in `GUI.md`).
   So it's safe for a program to close a window from inside that window's
   own callbacks, even from paint.

## Strings from the program

`SYS_WIN_CREATE` (the title) and `SYS_WIN_TEXT` (the text) pass a pointer
to a string, and the kernel has to read it. A wrong pointer could make the
kernel read anything: other memory, or, with no `'\0'` in sight, far past
the end of everything. So `progwin_string_ok(s, 256)` checks, one byte at
a time *before* reading it, that every byte up to and including the
`'\0'` is inside the program area, and that the `'\0'` comes within 256
characters. A pointer into the kernel, or a string that runs off the end
of the program area, is refused: the window isn't created (`eax` = 0) or
the text isn't drawn, and the bad address goes to the serial log:

```
progwin: bad string pointer 0x100000 from the program
```

## Walkthrough: CLICK.ASM

`programs/click.asm` opens a window with a red box, and every click
switches the box between red and blue. (`programs/click.c` is the same
program in C.) Build and run it in a Terminal:

```
admin$ asm click.asm click.elf
admin$ exec click.elf
```

**1. Constants.** The call and event numbers from `src/arch/syscall.h`:

```nasm
SYS_WIN_CREATE     equ 6
SYS_WIN_ON         equ 7
SYS_WIN_RECT       equ 8
SYS_WIN_INVALIDATE equ 10
WIN_EVENT_PAINT    equ 0
WIN_EVENT_MOUSE    equ 2
RED_XOR_BLUE       equ 0x00FF00FF   ; red ^ this = blue, blue ^ this = red
```

**2. `_start` sets everything up and returns.** It opens a 200x100
window, keeps the id in the variable `win`, and registers two callbacks:

```nasm
_start:
    mov eax, SYS_WIN_CREATE
    mov ebx, title          ; "Click me"
    mov ecx, 200            ; width
    mov edx, 100            ; height
    int 0x80
    mov [win], eax          ; the window id (1-4), or 0 on failure

    mov eax, SYS_WIN_ON
    mov ebx, [win]
    mov ecx, WIN_EVENT_PAINT
    mov edx, paint          ; the address of the paint function
    int 0x80
    ; ... the same for WIN_EVENT_MOUSE -> click
    xor eax, eax
    ret                     ; exit code 0, but the window stays open
```

After `ret`, the shell prompt comes back while the window stays: the
program now lives on through its callbacks.

**3. `paint(id)` draws from the program's state.** The window id is the
first argument, at `[esp + 4]`. It draws one rectangle in whatever color
the variable `color` holds:

```nasm
paint:
    mov eax, SYS_WIN_RECT
    mov ebx, [esp + 4]      ; id
    mov ecx, 0x000A000A     ; x = 10, y = 10   (x|y)
    mov edx, 0x00500020     ; w = 80, h = 32   (w|h)
    mov esi, [color]
    int 0x80
    ret
```

**4. `click(id, x, y)` changes the state, not the screen.** It flips the
color with an XOR and asks for a repaint:

```nasm
click:
    mov eax, [color]
    xor eax, RED_XOR_BLUE   ; 0xFF0000 <-> 0x0000FF
    mov [color], eax
    mov eax, SYS_WIN_INVALIDATE
    mov ebx, [esp + 4]
    int 0x80
    ret
```

It doesn't draw, because drawing only works in paint. Right after the
click, the window manager redraws, calls `paint` again, and the box shows
the new color.

**5. Data.** The variables live in the program area, next to the code:

```nasm
title: db "Click me", 0
color: dd 0x00FF0000       ; starts red
win:   dd 0
```

The flow for one click:

```
mouse IRQ -> event queue -> wm: press inside the window
  -> progwin_mouse -> program_callback(click, id, x, y)
       click: color ^= RED_XOR_BLUE; SYS_WIN_INVALIDATE
  -> next frame: wm_draw -> progwin_paint -> program_callback(paint, id)
       paint: SYS_WIN_RECT with the new color
```

## Key functions

| Function | Purpose |
|----------|---------|
| `progwin_create(title, w, h)` | Open a window for the program; returns its id. |
| `progwin_set_callback(id, event, fn)` | Register a program function for an event. |
| `progwin_fill_rect`, `progwin_draw_text`, `progwin_draw_line` | Draw, only inside paint. |
| `progwin_invalidate(id)`, `progwin_close(id)` | Ask for a repaint / a close at the next frame. |
| `progwin_set_owner(name)`, `progwin_owner()` | Who owns the open windows (used by `exec`). |
| `progwin_program_crashed()` | Kill the program and close all its windows. |
| `program_callback(fn, a0, a1, a2)` | Call a program function with crash protection. |

## Known limits

- **Ring 0, no protection.** Programs run with kernel privileges. The
  checks catch mistakes (bad ids, bad callback addresses, bad string
  pointers, crashes), not malice: a program can still overwrite kernel
  memory directly.
- **A callback that never returns freezes the OS.** There's no
  preemption, and the window manager waits for every callback.
- Only one program with windows at a time, with at most 4 windows.
- No mouse release or move events, no right button, no keys other than
  printable characters and the `KEY_*` codes.
- Drawing is limited to rectangles, lines and text.
