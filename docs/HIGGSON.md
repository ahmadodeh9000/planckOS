# Higgson

Higgson is planckOS's own programming language, named after the Higgs
boson to go with planckOS's physics theme. Its compiler turns a Higgson
program into planckOS assembly, which the built-in assembler (`ASSEMBLER.md`)
turns into a program for `exec`.

The compiler started as `planckBASIC`, written by following Austin
Henley's *Teeny Tiny Compiler* tutorial: a lexer, a recursive-descent
parser and an emitter.

## One source, two builds

```
src/higgson/            the compiler core: no C library, no kernel code
  lex.c  parser.c  emit.c  util.c    the original compiler (pcl_ -> hg_)
  compiler.c            hg_compile: sets up and runs one compilation
  arena.c               all of one compilation's memory, in one block
  platform.h            the only thing the core needs from its system
  platform_kernel.c     ... inside planckOS
tools/higgson-host/
  platform_host.c       ... on a normal computer (the C library)
  main.c                higgson IN.HGS OUT.ASM, for the Mac or Linux
```

`make higgson-host` builds `build/higgson-host/higgson` from the same core
files the kernel uses. The platform layer is three things:

| | Kernel | Host |
|---|---|---|
| Memory | `kmalloc` / `kfree` | `malloc` / `free` |
| Output text | `kvsnprintf` (`lib/format.c`) | `vsnprintf` |
| Errors | `setjmp` / `longjmp` (`arch/jump.s`) | `setjmp` / `longjmp` from the C library |

## How the port works

**Memory: an arena.** A compilation allocates many small pieces: the
text of every token, the parser's tables, the growing assembly buffers.
In the original each was a `malloc`, and errors called `exit()`, leaving
the operating system to clean up. A kernel has no one to clean up after
it. So `hg_compile` takes one 512 KB block from the heap and hands it out
in pieces (`arena.c`), and `hg_result_free` gives the whole block back
at once. Nothing is freed on its own, so nothing can be forgotten, even
when an error stops the compiler halfway through.

**Errors: setjmp and longjmp.** The parser is recursive: an error can
be ten function calls deep. Instead of `exit()`, `hg_error` stores the
message and line number and calls `hg_platform_fail`, which `longjmp`s
back to the `setjmp` in `hg_platform_try`, at the top of the compile.
Every frame in between is thrown away in one step, and `hg_compile`
returns the error. In the kernel, `setjmp` and `longjmp` are a few lines
of assembly (`src/arch/jump.s`): `setjmp` saves the registers a C
function has to preserve (ebx, esi, edi, ebp), the stack pointer and its
return address; `longjmp` puts them back and jumps to that address, so
`setjmp` seems to return a second time.

**State: never on the stack.** The kernel stack is 8 KB. The lexer,
parser (with its 256-name tables) and emitter are allocated in the
arena, not as local variables.

## What changed from planckBASIC

The import commit has the original exactly as it was. Besides the
`pcl_` to `hg_` rename:

- `parser.c` had two versions of every parsing function: the emitting
  one and, below it, the tutorial's earlier one that only printed what
  it parsed. C doesn't allow both, so the second is gone.
- Errors carry a line number (each token knows its line) and go through
  `hg_error` instead of `fprintf` + `exit`.
- No C library: `strcmp`, `isdigit` and friends are in `util.c`, and
  `strtol` is replaced by a small loop that also catches overflow.
- The lexer counts the source's length once; it used to call `strlen`
  for every character, which made lexing slow for long programs.
- `pcl_write_file` became `hg_emitter_text`, which hands the assembly to
  the caller: the core can't open files.
- The emitter formats straight into its buffer, since a temporary copy
  can't be freed in an arena.
