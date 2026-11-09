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

## Surfaces — `gfx/surface.c`

**What it does.** A `surface_t` is a block of 32-bit `0x00RRGGBB` pixels
with a width and height. Every drawing function takes the surface to draw
on. The screen's back buffer is a surface, and so is each window's own
buffer (in the GUI phase), so the same code draws everywhere.

**How it works.** Pixel `(x, y)` is `pixels[y * width + x]`. `clip_rect()`
cuts every rectangle down to the surface. For blits it also shifts the
source position, so drawing an image at a negative `x` shows the right part
of it. Rows are filled with `memset32` (`rep stosl`) and copied with
`memcpy` (`rep movsl`).

**Key functions.**

| Function | Purpose |
|----------|---------|
| `RGB(r, g, b)` | Build a color. |
| `surface_put_pixel(s, x, y, color)` | One pixel. |
| `surface_fill_rect(s, x, y, w, h, color)` | Solid rectangle. |
| `surface_blit(dst, x, y, src)` | Copy all of `src` onto `dst` at `(x, y)`. |

**Extending it.** A blit with a "transparent" color key would let you draw
shaped images such as the mouse cursor. Lines and outlines can be built
from `surface_fill_rect` with a width or height of 1.

## Framebuffer — `gfx/fb.c`

**What it does.** Maps video memory, owns the back buffer and shows it.

**How it works.**

- *Mapping.* QEMU puts the framebuffer at `0xFD000000`, far outside the
  64 MB identity map. `fb_init()` identity-maps `pitch * height` bytes
  with `paging_map()`, one page at a time and write-through, so writes go
  straight to the card.
- *Back buffer.* A `width * height` surface on the heap (3 MB at
  1024x768). All drawing goes there. `fb_present()` copies it to video
  memory row by row, because a screen row is `pitch` bytes, which can be
  more than `width * 4`. Drawing off-screen means the user never sees a
  half-drawn frame, and RAM is much faster than video memory.

**Key functions.**

| Function | Purpose |
|----------|---------|
| `fb_init(mbi)` | Check the mode, map it, allocate the back buffer. |
| `fb_back_buffer()` | The surface to draw the next frame on. |
| `fb_present()` | Show the back buffer. |

**Extending it.** `fb_present_rect()` could copy only the part that changed,
which is a big win when just the mouse cursor moves. Supporting other pixel
formats means converting colors in `fb_present()`.

**Known limits.** 32-bit RGB modes only. A full `fb_present()` copies 3 MB
every time.

## Font — `gfx/font.c`

**What it does.** Draws text with an 8x16 bitmap font (printable ASCII,
characters 32-126).

**Where the font comes from.** It's the classic 8x16 VGA font as shipped in
SeaBIOS's VGA BIOS (`vgasrc/vgafonts.c`, GNU LGPL v3), which QEMU includes
as `vgabios-stdvga.bin`. The 95 glyphs were copied out of that ROM into a C
array. LGPL v3 data can be used in this GPL v3 project. The credit sits at
the top of `font.c`.

**How it works.** Each glyph is 16 bytes, one per pixel row from top to
bottom. In each byte bit 7 is the leftmost pixel. `font_draw_char()` walks
the 16 x 8 bits and calls `surface_put_pixel()` for every 1 bit, so the
background shows through and text can be drawn on any color. Characters
outside 32-126 are drawn as `?`.

**Key functions.**

| Function | Purpose |
|----------|---------|
| `font_draw_char(s, x, y, c, color)` | One character, top-left at `(x, y)`. |
| `font_draw_text(s, x, y, text, color)` | A string on one line, `FONT_WIDTH` (8) pixels per character. |

**Extending it.** For box-drawing or accented characters, extend the table
past 126 (the ROM has all 256) and change `LAST_GLYPH`. A text-with-
background variant would fill an `8 x 16` cell first, which is useful for a
terminal.

**Known limits.** One font and one size. No `\n` handling in
`font_draw_text`.
