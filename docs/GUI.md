# GUI

The GUI code lives in `src/gui/`. It runs entirely in normal kernel
context. Interrupt handlers only feed it events (see `INPUT.md`).

## Main loop — `gui/gui.c`

`kmain` calls `gui_main_loop()` when `fb_init()` found a framebuffer. The
loop never returns:

```
for (;;) {
    event_wait(&event);          // sleep (hlt) until input arrives
    apply it, then every other queued event
    redraw into the back buffer
    fb_present();                // show the new frame
}
```

Taking all queued events before drawing means a fast mouse that queued 20
moves causes one redraw, not 20.

For now the screen is a placeholder that shows the live pointer position,
held buttons and the last key. The window manager replaces it.
