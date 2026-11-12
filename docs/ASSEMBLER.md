# The assembler

Files: `src/asm/` (`lexer.c`, `parser.c`, `encoder.c`, `assembler.c`,
`error.c`), and the encoding test in `tests/asm/`.

## What it does

Turns NASM-style 32-bit x86 assembly into a planckOS program: an ELF file
that `exec` can run (see `EXEC.md`). It runs inside planckOS, so you can
write a program in Notepad, assemble it and run it without leaving the OS.

```
asm_assemble(source, length, out, capacity)
    source text  ->  ELF file:  [ELF header | program header | code ... ]
                                 ^ PROGRAM_BASE (0x2000000)   ^ ASM_ORIGIN (0x2000054)
```

The output is one `PT_LOAD` segment covering the whole file, loaded at
`PROGRAM_BASE`. The 84 bytes of headers come first, so the first
instruction lands at `ASM_ORIGIN` = `0x2000054`. Execution starts at the
label `_start` if the file has one, otherwise at the first instruction.

## Using it in planckOS

In a Terminal:

```
admin$ asm LOOP.ASM LOOP.ELF
asm: wrote LOOP.ELF (128 bytes)
admin$ exec LOOP.ELF
1
2
...
```

`asm <source> <program>` (in `shell/shell.c`) reads the source file (up
to 64 KB), assembles it into a 64 KB buffer, and writes the program with
`fat12_write`. If something is wrong, nothing is written, and you get the
line and the reason:

```
admin$ asm NOTES.TXT X.ELF
asm: NOTES.TXT line 1: unexpected character: !
```

Two examples come with planckOS in `programs/`, and `make programs`
copies them onto the disk as `HELLO.ASM` and `LOOP.ASM`:

- **`hello.asm`** prints a line with `SYS_PRINT` and returns.
- **`loop.asm`** prints 1 to 10 with `SYS_PRINT_NUMBER`, using `inc`,
  `cmp` and `jle` for the loop. `int 0x80` keeps every register except
  `eax` (when a call returns a value), so `ecx` survives the calls.

Programs talk to the kernel with `int 0x80`: the call number in `eax`,
the argument in `ebx` (see the table in `INTERRUPTS.md`). A program ends
with `ret`, and `eax` is its exit code.

## Syntax

```nasm
; comments start with a semicolon
SYS_PRINT equ 0                 ; a named constant

_start:                         ; a label (the colon is optional before db/dw/dd)
    mov eax, SYS_PRINT
    mov ebx, message            ; a label as an immediate = its address
    int 0x80
    mov ecx, [table + 4]        ; memory: [reg], [reg+imm], [reg-imm], [label], [label+imm]
    mov byte [ebx], 'A'         ; byte/dword says the size when no register does
    ret

message: db "Hello", 10, 0      ; strings and numbers
table:   dd 1, 2, 3, message    ; 32-bit values (labels allowed)
         dw 0x1234              ; 16-bit values
         times 4 db 0           ; repeat a line
length   equ $ - message        ; $ is the address where the line starts
```

- **Numbers:** decimal (`42`, `-5`), hex (`0x2A`), or characters (`'A'`, up
  to 4 characters, stored little-endian like NASM).
- **Expressions:** numbers, labels, `equ` names and `$`, joined with `+`
  and `-`.
- **Case:** instruction and register names can be written in any case.
  Labels are case-sensitive.
- **`strict` and `near`** are accepted and ignored, so NASM code like
  `je strict near done` works. Jumps here are always the long form anyway.

### Supported instructions

| Instructions | Operand forms |
|---|---|
| `mov` | reg, reg / reg, imm / reg, [mem] / [mem], reg / [mem], imm |
| `add or and sub xor cmp` | same as `mov` |
| `test` | reg, reg / reg, imm / [mem], reg / reg, [mem] / [mem], imm |
| `inc dec not neg mul div` | reg / [mem] |
| `shl shr` | reg or [mem], then `1`, a number, or `cl` |
| `push` | reg32 / imm / label / [mem] (32-bit) |
| `pop` | reg32 / [mem] (32-bit) |
| `lea` | reg32, [mem] |
| `jmp call` | label / reg32 / [mem] |
| `je jne jz jnz jl jle jg jge ja jb` | label |
| `int` | number 0-255 |
| `ret nop` | none |

Registers: `eax ebx ecx edx esi edi ebp esp` (32-bit) and
`al bl cl dl ah bh ch dh` (8-bit). There is no 16-bit (`ax`) support.

Directives: `db`, `dw`, `dd`, `times`, `equ`.

## How it works

### The pieces

```
source line --lexer--> tokens --parser--> statement --assembler--> instruction --encoder--> bytes
```

| Part | File | Job |
|------|------|-----|
| Lexer | `lexer.c` | Splits a line into tokens: identifiers, numbers, strings, `, : [ ] + - $`. Stops at `;`. |
| Parser | `parser.c` | Finds the label, `times`, and the instruction or directive, and parses operands into register / immediate / memory forms. Values stay unevaluated expressions. |
| Assembler | `assembler.c` | Runs the two passes, keeps the symbol table, evaluates expressions, handles `db/dw/dd/times/equ`, writes the ELF headers. |
| Encoder | `encoder.c` | Turns one instruction with evaluated operands into bytes. Knows nothing about text or labels. |

### Two passes

A jump can point at a label further down the file, whose address isn't
known yet when the jump is reached. So the assembler reads the file
twice:

1. **Pass 1** encodes every line *only to learn its size*, and records
   each label's address (`symbol_define`). Labels not seen yet count as 0.
2. **Pass 2** encodes everything again, now with every address known, and
   writes the bytes. It checks that every label still has the same address
   as in pass 1.

This only works if each instruction has **the same size in both passes**,
which is why:

- jumps and calls always use 32-bit offsets (`E9 rel32`, `0F 84 rel32`),
  never the 2-byte short form, whose use would depend on the distance,
- a label used as an immediate is always encoded as 32 bits,
- values that *do* change an instruction's size must be known when the
  line is read: `times` counts, `[reg+offset]` offsets (8-bit vs 32-bit
  displacement), shift counts (shift-by-1 has its own opcode), and `equ`
  values. Using a label defined further down in those places is an error.

`evaluate()` returns a `known` flag for exactly this. A symbol is "known"
if it was defined *above* the current line in the current pass. In pass 2,
a label from further down already has a value, but it's still treated as
unknown, so both passes make the same choice.

`$` is the address where the current line starts (like NASM), so
`len equ $ - message` measures everything since `message`.

### How instructions are encoded

An x86 instruction is built from up to five parts:

```
[opcode 1-2 bytes] [ModR/M] [SIB] [displacement 0/1/4] [immediate 0/1/4]
```

**The opcode's low bit (w)** is usually the size: `0x88` is `mov r/m8, r8`
and `0x89` is `mov r/m32, r32`.

**ModR/M** (`mod:2 reg:3 rm:3`) names the operands:

| mod | rm means |
|-----|----------|
| `11` | a register (`rm` is its number) |
| `00` | `[reg]`, except `rm=100` (SIB follows) and `rm=101` (`[disp32]`) |
| `01` | `[reg + disp8]` |
| `10` | `[reg + disp32]` |

Register numbers: eax/al 0, ecx/cl 1, edx/dl 2, ebx/bl 3, esp/ah 4,
ebp/ch 5, esi/dh 6, edi/bh 7. For instructions with only one operand, the
`reg` field holds an *extension* that picks the operation: `81 /5` means
"opcode 81 with reg = 5", which is `sub r/m32, imm32`.

Worked examples (all checked against NASM):

| Instruction | Bytes | Why |
|-------------|-------|-----|
| `mov eax, ebx` | `89 D8` | 89 = mov r/m32, r32. ModR/M `11 011 000`: mod=register, reg=ebx(3), rm=eax(0) |
| `mov ecx, 12345` | `B9 39 30 00 00` | `B8 + register` (ecx=1), then imm32 little-endian |
| `mov eax, [ebx+4]` | `8B 43 04` | 8B = mov r32, r/m32. ModR/M `01 000 011`: disp8, reg=eax, rm=ebx. Then disp8 = 4 |
| `mov eax, [esi+200]` | `8B 86 C8 00 00 00` | 200 doesn't fit in a signed byte, so mod=10 and a 4-byte displacement |
| `mov eax, [esp]` | `8B 04 24` | rm=100 means "SIB follows". SIB `24` = no index, base esp |
| `mov eax, [ebp]` | `8B 45 00` | mod=00 rm=101 would mean `[disp32]`, so `[ebp]` is written `[ebp+0]` with a disp8 |
| `mov eax, [table]` | `A1 xx xx xx xx` | special short form: mov eax, [address] |
| `sub esi, -5` | `81 EE FB FF FF FF` | 81 /5 id: ModR/M `11 101 110` (extension 5 = sub, rm=esi) |
| `add eax, 7` | `05 07 00 00 00` | "accumulator" form for eax: no ModR/M needed |
| `shl eax, 1` | `D1 E0` | shift-by-one has its own opcode (D1 /4) |
| `inc edi` | `47` | `40 + register` |
| `push 5` | `68 05 00 00 00` | push imm32 |
| `jmp back` | `E9 rel32` | rel32 = target − (address of the *next* instruction) |
| `je strict near done` | `0F 84 rel32` | `0F 80 + condition` (4 = equal) |
| `int 0x80` | `CD 80` | |

The encoder always picks what `nasm -O0` picks. That covers the
accumulator forms for `eax`/`al`, `A0`-`A3` for `mov` between `eax`/`al`
and a fixed address, `D0`/`D1` for shifts by 1, and an 8-bit displacement
whenever it fits.

### Errors

Every part reports problems the same way, through `asm_fail()` in
`error.c`. Assembly stops at the first error, and `asm_result_t` carries
the line number and message, for example:

```
line 12: add: value doesn't fit in a byte
line 3: undefined label: nowhere
line 7: times count must be known here: use only numbers and names defined above
```

Bad input can't crash the kernel:

- the lexer never reads past the end of a line,
- the parser never reads past the `TOKEN_END` the lexer always appends,
- every table (tokens, terms, data items, symbols) has a fixed size that
  is checked before adding,
- every write to the output checks the remaining capacity,
- `times` is capped at 65536.

All state is static (the kernel stack is only 8 KB), so `asm_assemble`
isn't reentrant.

## How it was tested

- **`make asm-test`** builds the assembler for your computer, assembles
  `tests/asm/encoding.asm` (every instruction form listed above, plus
  data directives, `equ`, `$` and `times`), assembles the same file with
  `nasm -O0`, and compares the bytes. They're identical (682 bytes).
  `tests/asm/nasm.asm` tells NASM what planckOS assumes: `bits 32` and
  `org 0x2000054`.
- **Error cases:** about 25 broken lines (unknown instruction, size
  mismatch, value too big, undefined label, forward reference in
  `times`, unterminated string ...) each give the right message.
- **Fuzzing:** 20,000 random files (mixes of real keywords, registers,
  punctuation and random bytes) went through a host build compiled with
  AddressSanitizer and UndefinedBehaviorSanitizer, with no memory errors.

## How to add an instruction

Say you want `adc` (add with carry), which works exactly like `add` with
base opcode `0x10` and extension 2:

1. Look up the encoding in the Intel manual (or assemble a sample with
   `nasm -O0 -l listing.lst`).
2. If it fits an existing group, add one row to `mnemonics[]` in
   `encoder.c`:
   ```c
   { "adc", KIND_ALU, 0x10, 2 },
   ```
   That's all for `adc`: the parser finds mnemonics through
   `encoder_find_mnemonic`.
3. If it's a new shape, add a `KIND_...`, an `encode_...` function that
   uses `put8`/`put32`/`put_modrm`/`put_imm`, and a `case` in
   `encode_by_kind`.
4. Add every form to `tests/asm/encoding.asm` and run `make asm-test`.

## Known limits

- 32-bit and 8-bit registers only. No 16-bit, no segment registers, no
  FPU/SSE.
- No `[base + index*scale]` addressing: one register per address.
- No short jumps: every jump takes 5 or 6 bytes.
- No macros, includes, sections or local labels.
- Assembly stops at the first error.
