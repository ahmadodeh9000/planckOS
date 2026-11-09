# Graphics

The graphics code lives in `src/gfx/`. It draws pixels and text into a
back buffer and copies finished frames to the screen. It knows nothing
about windows; that's `src/gui/`.

## Getting a framebuffer

planckOS doesn't program the video card itself. GRUB does it:

1. `boot/boot.s` sets flag bit 2 in the Multiboot header and appends a video
   request: `mode_type 0` (linear graphics), 1024 x 768, 32 bits per pixel.
   The five address fields in front of it are zero. They only matter for
   non-ELF kernels, but they have to be there so the video fields sit at the
   right offsets.
2. `boot/grub.cfg` loads `all_video` and sets `gfxpayload=1024x768x32` after
   the `multiboot` command. (`multiboot` itself sets `gfxpayload` from the
   header, so anything set before it is overwritten.)
3. GRUB fills in the framebuffer fields of `multiboot_info_t` (flag bit 12):
   physical address, pitch, size, bpp, type, and where the red, green and
   blue bits are.

`fb_init()` only accepts a type-1 (RGB) 32-bit mode with red at bit 16,
green at 8 and blue at 0, i.e. `0x00RRGGBB` pixels. Anything else (for
example the EGA text mode you get with `gfxpayload=text`) makes it return
`false`, and `kmain` falls back to the old text shell.

**Gotcha:** GRUB's C struct isn't packed. The color fields live in a union
with a 32-bit palette address, so they start at offset 112, not 110. Our
packed `multiboot_info_t` has two padding bytes there.

## Framebuffer — `gfx/fb.c`

**What it does.** Maps video memory, keeps a back buffer, and offers basic
drawing: pixels, filled rectangles and blits (copying a block of pixels).

**How it works.**

- *Mapping.* QEMU puts the framebuffer at `0xFD000000`, far outside the
  64 MB identity map. `fb_init()` identity-maps `pitch * height` bytes
  with `paging_map()`, one page at a time and write-through, so writes go
  straight to the card.
- *Back buffer.* A `width * height` array of `uint32_t` on the heap (3 MB
  at 1024x768). All drawing goes there. `fb_present()` copies it to video
  memory row by row, because a screen row is `pitch` bytes, which can be
  more than `width * 4`. Drawing off-screen means the user never sees a
  half-drawn frame, and RAM is much faster than video memory.
- *Clipping.* `fb_clip()` cuts every rectangle down to the screen. For
  blits it also shifts the source position, so drawing at a negative `x`
  shows the right part of the image.
- *Speed.* Rows are filled with `memset32` (`rep stosl`) and copied with
  `memcpy` (`rep movsl`).

**Key functions.**

| Function | Purpose |
|----------|---------|
| `fb_init(mbi)` | Check the mode, map it, allocate the back buffer. |
| `fb_width()`, `fb_height()` | Screen size in pixels. |
| `fb_put_pixel(x, y, color)` | One pixel. |
| `fb_fill_rect(x, y, w, h, color)` | Solid rectangle. |
| `fb_blit(x, y, w, h, pixels, stride)` | Copy a pixel block; `stride` = pixels per source row. |
| `fb_present()` | Show the back buffer. |
| `FB_RGB(r, g, b)` | Build a `0x00RRGGBB` color. |

**Extending it.** `fb_present_rect()` could copy only the part that changed,
which is a big win when just the mouse cursor moves. Supporting other pixel
formats means converting colors in `fb_present()`.

**Known limits.** 32-bit RGB modes only. A full `fb_present()` copies 3 MB
every time.
