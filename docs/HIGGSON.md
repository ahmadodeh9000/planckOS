# Higgson

Higgson is planckOS's own programming language, named after the Higgs
boson to go with planckOS's physics theme. Its compiler turns a Higgson
program into planckOS assembly, which the built-in assembler (`ASSEMBLER.md`)
turns into a program for `exec`.

The compiler started as `planckBASIC`, written by following Austin
Henley's *Teeny Tiny Compiler* tutorial: a lexer, a recursive-descent
parser and an emitter.

## The language

A Higgson program looks like a small C program without types: every
value is a whole number (a 32-bit signed int).

```
// fib.hgs: the first 10 Fibonacci numbers
let a = 0;
let b = 1;
let n = 0;
while (n < 10) {
    print(a);
    let next = a + b;
    a = b;
    b = next;
    n = n + 1;
}
```

### Statements

| Statement | What it does |
|---|---|
| `let x = 5;` | declares the variable `x` and gives it a value |
| `x = x + 1;` | gives an existing variable a new value |
| `print("hello");` | prints a string and a newline |
| `print(x * 2);` | prints a number and a newline |
| `if (x > 3) { ... }` | runs the block if the condition isn't 0 |
| `if (...) { ... } else { ... }` | one block or the other; `else if (...)` works too |
| `while (n > 0) { ... }` | runs the block again and again while the condition isn't 0 |
| `func name() { ... }` | defines a function (no parameters yet) |
| `name();` | calls a function |
| `return;` | leaves a function early (in the main code: ends the program) |

Every statement ends with `;`, except the ones that end with a `}` block.
Keywords are lowercase. Comments are `// to the end of the line` and
`/* anything, even several lines */`.

**Variables** must be declared with `let` before they're used, further
up in the file. Every variable is global: one variable for the whole
program, even if its `let` is inside a function. A second `let` with the
same name is an error.

**Functions** can be defined anywhere at the top level, before or after
the code that calls them, but not inside another function or inside
`if` or `while`. They take no parameters and give back no value; they
work with the (global) variables.

**The main code** is everything outside functions. It runs from top to
bottom when the program starts; function bodies run only when called.

### Values and operators

Numbers are written in decimal (`42`, up to 2147483647) or hex
(`0xFF8800`, up to `0xFFFFFFFF`). There are no decimals and no strings in
calculations; strings only appear in `print` (and, for apps, `text`).

From loosest to tightest, as in C:

| Operators | Meaning |
|---|---|
| `\|\|` | or: 1 if either side isn't 0 |
| `&&` | and: 1 if both sides aren't 0 |
| `==` `!=` | equal, not equal (1 or 0) |
| `<` `<=` `>` `>=` | comparisons (1 or 0) |
| `+` `-` | add, subtract |
| `*` `/` `%` | multiply, divide, remainder (rounded toward 0, as in C) |
| `-x` `!x` | minus, not (`!x` is 1 when x is 0) |

`&&` and `||` stop early, as in C: in `a && b`, `b` isn't computed if `a`
is 0. Dividing by 0 stops the program (`exec` reports a crash).

### Errors

The compiler stops at the first error and says which line it's on:

```
higgson: FIB.HGS line 4: expected ';' after statement
higgson: FIB.HGS line 7: unknown variable 'cuont'
higgson: FIB.HGS line 9: unknown function 'drwa'
higgson: FIB.HGS line 3: this '{' is never closed with '}'
higgson: FIB.HGS line 2: this string is never closed with "
higgson: FIB.HGS line 5: unknown character '@'
```

A missing `;` is reported on the line of the statement that needs it,
and an unclosed `{` on the line where it opens.

## Using it

In a Terminal:

```
higgson FIB.HGS FIB.ELF     compile to a program, then: exec FIB.ELF
higgson FIB.HGS FIB.ASM     write the assembly instead, to read in Notepad
```

The assembly goes from the compiler to the assembler in memory; only the
finished program is written to the disk. An error stops the compile and
says where:

```
higgson: FIB.HGS line 5: expected ';' after statement
```

and nothing is written. Source files can be up to 32 KB.

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

**Its own stack.** Even so, the parser's recursion needs stack: each
level of parentheses goes through every precedence level (`||`, `&&`,
`==`, `<`, `+`, `*`, unary), about 700 bytes. Fourteen levels measured
7.8 KB, nearly the whole kernel stack. So in the kernel the compile runs
on a 32 KB stack of its own (`compiler_stack` in `platform_kernel.c`):
`call_on_stack` in `arch/jump.s` points esp at it, calls the compiler, and
points esp back. A longjmp from an error restores the old esp anyway.

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
