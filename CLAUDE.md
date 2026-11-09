# planckOS — rules for working in this repo

planckOS is a hobby 32-bit x86 OS (kernel: Iris) written in C and NASM. It boots
with GRUB/Multiboot 1 and runs in QEMU. Ahmad Odeh is the only author.

## Hard rules

1. **No AI attribution anywhere.** Commits, PRs, code comments, docs and the README
   must not contain "Co-Authored-By" trailers for an AI, "Generated with ..." lines,
   session links, or any mention of an AI assistant or its vendor. Ahmad Odeh is the
   only author. `.claude/settings.json` turns the automatic trailers off; keep it.
2. **Never rewrite or force-push history.** `v1.0-text` (tag) and `legacy/text-shell`
   (branch) preserve the original text-mode version.
3. **Small steps.** One focused commit per working change. Every commit must build
   with no warnings and boot in QEMU.
4. **Ask before deleting or untracking files**, especially `disk.img` (it may hold
   user data).
5. **Keep it minimal.** No outside libraries, no features beyond the agreed plan
   without asking first.
6. **Docs move with code.** Update the matching `docs/` page in the same commit as the
   code it describes.

## Code style

- Names like `fat12_read`, `pmm_alloc_page`, `window_raise`: `subsystem_verb_noun`,
  clear over clever.
- 4-space indents, braces on the same line, `/* */` comments.
- No magic numbers: use named `#define`s (e.g. `#define PS2_MOUSE_IRQ 12`).
- Short functions that do one thing; split anything over ~40 lines.
- Every file starts with a comment block saying what it does and how it fits in.
- Comments explain *why* and call out tricky hardware details; skip obvious ones.
- One subsystem per folder (`arch/`, `mm/`, `drivers/`, `fs/`, `lib/`, `gfx/`, `gui/`,
  `apps/`). Headers expose only what other parts need; helpers are `static`.
- No dead code, commented-out blocks or leftover debug prints. Use `serial_printf`
  for debugging output.
- Interrupt handlers never draw; they only record state or push events.

## Building

See `docs/BUILDING.md`. Short version: `make` builds `build/planckOS.iso`,
`make run` boots it in QEMU, `make run-headless` boots with serial on stdout.
