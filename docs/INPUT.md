# Input

Input flows in one direction:

```
hardware IRQ  ->  driver handler  ->  event queue  ->  main loop / shell
(IRQ1, IRQ12)     (keyboard.c,        (gui/event.c)     (event_wait /
                   mouse.c)                              event_poll)
```

Interrupt handlers only translate hardware bytes into `event_t`s and push
them. They never draw, print or touch windows. All of that happens later in
normal code that reads the queue. Handlers run with interrupts disabled and
can arrive at any moment, so keeping them tiny avoids long stalls and
half-finished updates.

## Event queue — `gui/event.c`

**What it does.** Holds up to 255 pending `event_t`s.

**The event type.**

| `type` | Fields used | Meaning |
|--------|-------------|---------|
| `EVENT_KEY` | `key` | A key producing this ASCII character was pressed (`'\b'`, `'\n'`, `'\t'` included). |
| `EVENT_MOUSE_MOVE` | `x`, `y` | The pointer moved to `(x, y)`. |
| `EVENT_MOUSE_DOWN` | `x`, `y`, `button` | A button (`MOUSE_BUTTON_LEFT/RIGHT/MIDDLE`) was pressed. |
| `EVENT_MOUSE_UP` | `x`, `y`, `button` | A button was released. |

**How it works.** It's a ring buffer: an array plus a `head` (next slot to
write) and a `tail` (next slot to read). Empty means `head == tail`. Full
means advancing `head` would land on `tail`, and then new events are
dropped.

No lock is needed because there is exactly one writer and one reader.
Pushes only happen inside IRQ handlers, which can't interrupt each other,
and only the main loop pops. Two details make that safe with an optimizing
compiler:

- `head` and `tail` are `volatile`, so they're re-read every time.
- A compiler barrier (`asm volatile("" ::: "memory")`) makes sure the event
  is fully written before `head` moves (and fully read before `tail` moves).

`event_wait()` sleeps with `hlt` until something arrives. It checks the
queue with interrupts off and then runs `sti; hlt`. `sti` only takes effect
after the *next* instruction, so an interrupt can't slip in between the
check and the `hlt` and leave us asleep with an event waiting.

**Key functions.**

| Function | Purpose |
|----------|---------|
| `event_push(&e)` | Add an event (from an IRQ handler). |
| `event_poll(&e)` | Take the oldest event; `false` if none. |
| `event_wait(&e)` | Sleep until there is an event, then take it. |

**Extending it.** New input sources (a timer tick for animations, a mouse
wheel) just need a new `event_type` and a push from their handler.

**Known limits.** Fixed size; events are dropped when it's full.

## Keyboard input

`drivers/keyboard.c` pushes one `EVENT_KEY` per key press that maps to a
character. Line editing (echo, backspace, Enter) is the reader's job. The
text shell does it in `shell_readline()`.

## Mouse input

`drivers/mouse.c` keeps the pointer position itself and pushes:

- `EVENT_MOUSE_MOVE` with the new `(x, y)` whenever a packet moves it,
- `EVENT_MOUSE_DOWN` / `EVENT_MOUSE_UP` for each button whose state changed,
  with the position at that moment.

So readers never need to do pointer math. Every mouse event already
carries screen coordinates.

## Testing input without hands

The QEMU monitor can inject input, which is handy for scripted tests:

```
sendkey a                 # press and release a key
mouse_move 100 50         # move right 100, down 50 (relative)
mouse_button 1            # left button down (2 = right, 4 = middle)
mouse_button 0            # all buttons up
```
