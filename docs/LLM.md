# Language models

Files: `src/llm/model.c` (the transformer), `src/llm/tokenizer.c` (text to
tokens and back), `src/llm/sampler.c` (picking the next token),
`src/llm/llm.c` (loading, and the one-token-at-a-time API in `llm.h`),
`src/llm/llm_test.c` (the `llm-test` command), `src/lib/math.c` (the x87
math).

## What it does

planckOS can run small language models in the Llama 2 architecture,
stored in the file format of Andrej Karpathy's
[llama2.c](https://github.com/karpathy/llama2.c) (MIT license). The model
code is a port of llama2.c's `run.c`, kept close to the original so the
two can be compared line by line. The model reads a text, one token at a
time, and after each token produces a score for every token that could
come next. Pick one, feed it back in, and repeat: that's how it writes.

## Getting a model onto the disk

The model files aren't in git. `make run` takes care of them: it calls
`make models`, which downloads whatever is missing (Iris from the GitHub
release `iris-v2`, and `stories260K`, a test model with 260 thousand
parameters trained on the TinyStories dataset, from
[karpathy/tinyllamas](https://huggingface.co/karpathy/tinyllamas), MIT
license) and checks each file against `models/MODELS.sha256`. Then it
copies them onto the disk with 8.3 names, only when they changed and never
over a file of yours (`scripts/sync_disk.sh`):

| In `models/` | On the disk |
|--------------|-------------|
| `iris.bin` | `IRIS.BIN` |
| `iris_tok.bin` | `IRISTOK.BIN` |
| `stories260K.bin` | `S260K.BIN` |
| `tok512.bin` | `TOK512.BIN` |

`make disk-models` copies them even over files with the same names, for
when you want to replace an older model on purpose (also `make disk-models
DISK=other.img`). If Iris can't find her model, her window says so and
tells you to run `make models` and restart.

## Checking the port: `llm-test`

```
admin$ llm-test
Once upon a time, there was a little girl named Lily. She loved to play ...
llm-test: 252 tokens in 11670 ms (21 tokens/s)
llm-test: PASS, all 252 tokens match llama2.c
llm-test: heap used 4198776 bytes before, 4198776 after
```

At temperature 0 the model always takes its top-scoring token, so the
same prompt always gives the same text. `llm_test.c` holds what llama2.c's
`run.c` printed on a Mac:

```sh
./run stories260K.bin -z tok512.bin -t 0.0 -n 256 -i "Once upon a time"
```

and planckOS has to produce exactly that. A bug in `forward()`, the
tokenizer or the math would show up within a few tokens. All 252 tokens
match, even though the Mac computes with ARM floats (and fused
multiply-adds) and planckOS with the x87's 80-bit registers. The rounding
differs a little, but never enough to change which token scores highest
in this run. A bigger model or a longer text could hit a near-tie where
the two disagree; from that token on, the texts would differ. That would
be a rounding difference, not a bug.

`llm-test <model> <tokenizer>` runs the same test with other files, which
is how bad files were tested (each one is refused, nothing crashes):

```
admin$ llm-test TRUNC.BIN TOK512.BIN       (the first 500000 bytes of S260K.BIN)
llm-test: the model file is damaged or incomplete
serial: llm: model file is 500000 bytes, its header says 1056540
```

The last line shows that the heap is back where it started, so loading
and freeing a model leaks nothing.

## The file format

A model file (llama2.c's "version 0" format) is a header of seven 32-bit
ints, `model_config_t`, followed by every weight as a 32-bit float:

```
dim hidden_dim n_layers n_heads n_kv_heads vocab_size seq_len
token_embedding_table   vocab_size x dim
rms_att_weight          n_layers x dim
wq, wk, wv, wo          n_layers x dim x dim  (wk, wv: x kv_dim)
rms_ffn_weight          n_layers x dim
w1, w2, w3              n_layers x hidden_dim x dim
rms_final_weight        dim
(old RoPE tables)       seq_len x head_size, skipped
wcls                    vocab_size x dim, only if vocab_size < 0
```

A negative `vocab_size` is llama2.c's way of saying "the output layer has
its own weights"; otherwise it reuses the embedding table.

**Checks before anything is used.** `model_init()` copies the header,
rejects nonsense (zero or negative sizes, heads that don't divide the
width, values far beyond what fits in memory), computes how many bytes the
header implies, in 64-bit so a corrupted header can't wrap around, and
requires the file to be exactly that size. Only then do the weight
pointers go into the buffer. The model is read whole into one `kmalloc`
buffer (`run.c` uses `mmap`, which planckOS doesn't have) and the weights
point straight into it.

A tokenizer file is an int `max_token_length`, then per token a float
score, an int length and that many bytes. `tokenizer_init()` checks every
length against the bytes left and requires the file to end exactly after
the last token. The model's `vocab_size` says how many tokens there are.

## What a transformer does, step by step through `forward()`

`forward(model, token, pos)` takes one token and its position in the text
and returns `logits`: one score per vocabulary token for what comes next.

**1. Embedding.** Every token has a learned row of `dim` numbers in the
embedding table. `x` starts as a copy of the token's row. From here on,
`x` is "what the model currently thinks about this position".

**2. Layers.** `x` goes through `n_layers` identical layers (with
different weights). Each has two halves, and each half *adds* its result
to `x` instead of replacing it (the "residual connection"). That way every
layer only has to learn a correction, which is what makes deep stacks
trainable.

**3. RMSNorm** (`rmsnorm`). Before each half, `x` is scaled so its root
mean square is 1, then multiplied by learned weights. That keeps the
numbers in a sensible range however many layers came before.

**4. Attention** (`layer_attention`, `attention_head`). This is how a
token looks at the tokens before it. Three matrix multiplications turn
the normalized `x` into a *query* `q` ("what am I looking for?"), a *key*
`k` ("what do I contain?") and a *value* `v` ("what do I pass on?"). For
every earlier position `t`, the score is the dot product of this query
with that position's key, divided by `sqrt(head_size)`. Softmax turns the
scores into weights that add up to 1, and the result is the weighted sum
of the values. In "Lily said she was sad", the query of "she" can score
high against the key of "Lily". This is done in `n_heads` independent
*heads* of `head_size` numbers each, so different heads can look for
different things. `wo` mixes their results back into `dim` numbers.

**5. RoPE** (`rope`). Attention by itself doesn't know where tokens are.
Rotary position embeddings rotate each pair of numbers in `q` and `k` by
an angle `pos * freq`, with a different frequency per pair
(`1 / 10000^(i / head_size)`). The dot product of two rotated vectors
depends on the *difference* of their angles, so scores depend on how far
apart two tokens are.

**6. The KV cache.** The keys and values of earlier positions never
change, so they're computed once and stored in `key_cache` and
`value_cache`: `layer_attention` writes this position's `k` and `v`
straight into the cache. Without it, every new token would need a forward
pass over the whole text again. It's also what makes the context length
(`seq_len`) a hard limit: the cache has room for `seq_len` positions.
With `n_kv_heads < n_heads`, several query heads share one key/value head
(`kv_mul`), which makes the cache smaller.

**7. Feed-forward** (`layer_ffn`). The second half of a layer works on
this position alone: `w2(silu(w1(x)) * w3(x))`. `w1` and `w3` widen `x` to
`hidden_dim` numbers, SiLU (`x * sigmoid(x)`) is the non-linearity that
lets the network compute more than a straight multiplication could, and
`w2` brings it back to `dim`. This is where most of the model's "facts"
are thought to live.

**8. Output.** After the last layer, a final RMSNorm, and one more matrix
multiplication with `wcls` gives the logits, one score per token.

**9. Softmax and sampling** (`sampler.c`). Softmax turns the logits into
probabilities: `exp(score)` for each, divided by the total. Subtracting
the largest score first keeps `exp` from overflowing without changing the
result. At temperature 0 the sampler just takes the best token (argmax).
Otherwise the logits are divided by the temperature first (below 1:
sharper, safer; above 1: flatter, wilder) and a random number picks a
token with those probabilities. The random numbers come from `run.c`'s
xorshift generator, seeded from the RTC and the timer.

Almost all of the time is spent in `matmul`: for every output number, a
dot product of a weight row with `x`.

## The tokenizer

Models don't read letters; they read tokens, numbers from 0 to
`vocab_size - 1` that stand for pieces of text (" the", "Lily", "ing",
...). `tokenizer_encode()` is byte pair encoding:

1. Optionally the BOS token (1, "beginning of sequence"), and a leading
   space, because SentencePiece adds one to the start of every text it
   was trained on.
2. One token per character, or one token per byte (tokens 3 to 258) when
   a character isn't in the vocabulary.
3. Repeatedly merge the neighboring pair whose merged string is in the
   vocabulary with the highest score, until no pair is.

`run.c` sorts the vocabulary to binary-search it, and builds each pair's
string with `sprintf`. Here lookups are a linear search, comparing lengths
first, and a pair is compared in place. Each pair's merge result is
remembered, so after a merge only the two new neighbors are looked up
again instead of every pair. `tokenizer_decode()` turns a token back into
text: byte tokens are named `<0x0A>` and become that byte.

## Running it without freezing the desktop

`llm_next()` runs the model once and returns at most one piece of text:

| Returns | Meaning |
|---------|---------|
| `LLM_STEP_PROMPT` | It read one prompt token (no text). |
| `LLM_STEP_TOKEN` | It wrote one token; `piece` is its text. |
| `LLM_STEP_END` | The model ended the text (it chose BOS), or nothing is queued. |
| `LLM_STEP_FULL` | The context is full. |

`llm_start(prompt)` resets everything and queues BOS + the prompt;
`llm_feed(text)` queues more text after what's there (the user's next
message, for example). The caller decides when to take the next step, so a
GUI app can do one step per pass of the main loop.

## Talking to Iris — `llm/chat.c`

Iris is the chat model trained in `training/` (`TRAINING.md`), stored on
the disk as `IRIS.BIN` and `IRISTOK.BIN`. She learned conversations in
this exact format:

```
User: how do i list files
Iris: `ls` lists the files on the disk with their sizes.
User: thanks
Iris: you're welcome! happy to help.
```

`chat.c` keeps a conversation going in that format, one token at a time:

- `chat_send(chat, message)` feeds `User: <message>\nIris:` (the first
  message starts the text, with BOS). Then each `chat_step()` runs the
  model once; `chat_reply()` is the reply so far.
- Her reply ends where she writes a newline, because in the training data
  a newline after her line is always followed by the next `User:`. That
  newline stays in the model, so the next message continues the text just
  like in training. If she starts writing `User:` herself, or goes on for
  96 tokens (her longest trained answer is about 80), the reply is cut
  there.
- Everything she has read stays in the KV cache, so a new message only
  costs its own tokens. The context holds 256 tokens. Before a message
  that wouldn't fit (with 96 tokens kept free for her reply), the
  conversation is rebuilt: the model starts over with as many of the latest
  turns as fit, so the oldest ones are dropped. The same rebuild happens
  after a reply that didn't end cleanly, so the context is always a
  well-formed conversation.

Every reply is logged to serial, which is how the speed was measured:

```
iris: replied in 3170 ms (21 tokens written, 34 of 256 context used)
```

**In the Terminal: `iris` (or `chat`).** `shell_iris.c` sends every typed
line to Iris until an empty line, printing her reply as it grows. The
Terminal window gets idle work (`GUI.md`), so the desktop stays usable
while she writes; in text mode, `shell_run` works on her reply between
key presses.

```
admin$ iris
iris: waking up...
iris> hi! ask me anything about planckOS. an empty line takes you back to the shell.
you> who made you
iris> planckOS was made by ahmad odeh.
you>
iris> bye!
admin$
```

## Fact cards: retrieval in miniature — `apps/iris_facts.c`

A model with a million parameters can say something wrong with complete
confidence: a wrong file name in an example, a command that doesn't
exist. For help with planckOS that's a problem, so Iris's answers come
with **fact cards**: whenever your message names a shell command or an
app, a small card under her reply shows the facts, taken from the source
of truth instead of from the model.

```
you> what does cat do
iris> `cat <file>` prints a file, for example `cat NOTES.TXT`.
  cat <file>         - print file contents
```

For a command, the card is the exact line `help` prints:
`shell_help_line()` looks it up in the same `help_lines[]` table that
`cmd_help` prints from, so the card and `help` can never disagree. For an
app, it's a fixed description in `app_facts[]`. The lookup is plain
keyword matching: split the message into lowercase words and look each
one up. There is no AI in it, so it can't be wrong in an interesting way.

This is *retrieval* in miniature. Big AI assistants answer questions about
things they weren't trained on by first searching documents and putting
what they found next to the model's answer (or into its prompt): the
facts come from the documents, the wording from the model. Here the
"documents" are the help table, the "search" is a word lookup, and the
result is shown next to her reply. Unlike her reply, the card is always
right.

Known limits: it matches words, not meaning, so "how do i write a
program" also gets the card for `write`. At most 3 cards are shown.

## Tools: Iris picks a tool, planckOS does the work

Files: `apps/iris_tools.c` (reading tags, the keyword fallback),
`apps/iris_actions.c` (running the tools), `apps/iris_templates.c` (the
app templates), `training/my_tools.txt` (how she learned them).

**What tool use is.** Ask a language model "how much space is left on my
disk?" and it can only make up a number: it has never seen your disk. Big
assistants solve this with *tool use*: instead of answering, the model
writes a request ("call the disk tool"), a program outside the model runs
it, and the real result is shown. Iris does the same in miniature. For
some requests she answers with a **tag** instead of words:

| Tag | What planckOS does |
|-----|--------------------|
| `[disk]` | used, free and total space (`fat12_space`), and the number of files |
| `[memory]` | RAM in use (`pmm_stats`) and the heap (`heap_stats`) |
| `[time]` | the date and time from the RTC, and the uptime |
| `[files]` | the files with their sizes (the first 10) |
| `[find "x"]` | files whose name contains x, and text files (64 KB at most) that contain x, with the matching line |
| `[open "NAME"]` | opens the file in Notepad |
| `[make-app counter title="Taps" color=red]` | builds a small app from a template (below) |

The Iris app shows the tool's answer in her bubble and the details in a
card under it; the tag itself is never shown (while she writes it, her
bubble shows dots). In the Terminal (`iris`), the same happens in text;
`[open]` suggests `cat` there, since the Terminal can't open Notepad.

**How the tags are trained.** `training/my_tools.txt` maps many ways of
asking to each tag:

```
User: how much storage is left
User: is my disk full
User: am i running out of space
Iris: [disk]

User: find {word}
User: search my files for {word}
Iris: [find "{word}"]
```

`prepare_data.py` fills `{word}`, `{title}`, `{color}`, ... with random
values, a different one for each example, about a third of them made-up
words like "bofira". She can't memorize those, so she has to learn to
*copy* what you typed into the tag, letter by letter. The tool examples
are mixed into the same random conversations as normal chat, so she
learns when a tag is the right answer and when words are.

**How tags are read** (`tool_parse`). A reply is a tag only if it is
exactly one of the forms above: `[`, a known name, the right arguments
(a quoted value for `find`/`open`; a template name and `key=value` or
`key="value"` slots for `make-app`), `]`, and nothing else. Anything else
that starts with `[` is a bad tag, and you get "i didn't get that, try
rephrasing or type `help`." instead.

**Trust, but check** (`tool_choose`). A tiny model makes two kinds of
mistakes with tools: it misses the tag (a chatty answer to "how much space
is left"), or it copies a value wrong (`[find "back"]` for "find banana").
So a tag is only used if every value in it also appears in your message
as whole words (`"Happy Frid"` doesn't count for "Happy Friday"), and a
tag without values (`[disk]`, `[memory]`, `[time]`, `[files]`) only if the
message has a word about it ("disk", "space", "ram", "time", ...).
Without that, "what is the capital of spain" could get the disk space.
Otherwise, or when there's no tag, the **keyword fallback**
(`tool_route`) looks for obvious requests: "find ..."/"search for ...",
"open NAME.EXT", "make/create ... counter/clock/box/button", and words
like *space*, *storage*, *memory*, *ram*, *time*, *date*. Questions
about how something works ("how do i ...", "what does ... do") are left
to Iris. When the fallback steps in, the serial log says so. When
neither finds a tool, her reply is shown as she wrote it, with fact cards.

**Why templates instead of generated code.** A model with a million
parameters can't write working assembly, and even a big model's code
needs checking: generated code can do anything, and planckOS runs programs
in ring 0 with no protection. So Iris never writes code. She picks one of
five tested templates and fills in its slots:

| Template | Slots | The app |
|----------|-------|---------|
| `message` | title, text, color | a window with a message |
| `box` | title, color, size (small/medium/large) | a colored box |
| `counter` | title, color | click the box, the count goes up |
| `clock` | title, color | a digital clock, updated every second |
| `button` | title, text, color | a button that switches color when clicked |

`template_fill` checks every value before it touches the program: the
template and the color must be in its tables, titles must be 1 to 20
printable characters and texts 1 to 32 (20 on a button), and a slot the
template doesn't have is refused. Texts go inside `db "..."`, so a `"`
in a text could end the string early and turn the rest into code. It is
written as its byte value between two strings instead
(`db "say ", 34, "hi", 34`). Only then are the values pasted in, the
program is assembled with the built-in assembler (`asm_assemble`), and
`NAME.ASM` and `NAME.ELF` are saved. `NAME` comes from the title
(`Taps` -> `TAPS`); if `TAPS.ASM` or `TAPS.ELF` already exists, it
becomes `TAPS2`, then `TAPS3`, so she never overwrites your files.

Nothing runs by itself. Her answer shows the start of the code and a
**Run** button; the program starts only when you press it (`exec_file`,
the same as the `exec` command). `NAME.ASM` is ordinary assembly with
comments, so you can open it in Notepad, change it, and rebuild it with
`asm NAME.ASM NAME.ELF`. If a template ever failed to assemble, that
would be a bug in the template: you'd see the assembler's error, and the
serial log would say `iris: template for NAME doesn't assemble`.
`make templates-test` guards against that: it fills in every template
with every color and with titles full of quotes and semicolons,
assembles each with the built-in assembler and with NASM, and compares the
bytes (55 programs).

**Adding a tool.**

1. A `TOOL_*` value in `iris_tools.h` and its name in `kind_of()`
   (`iris_tools.c`). If it takes a value, read it in `read_arguments`.
2. A `run_*` function in `iris_actions.c` that prints a short answer to
   `bubble` and details to `card`, and a `case` in `tool_run`.
3. Examples in `training/my_tools.txt`, many phrasings per tag, then
   `make chat-model` and `make disk-models`.
4. Optionally, keywords in `tool_route` so the fallback catches it too.

**Adding a template.** Write the program as a C string in
`iris_templates.c` with `{TITLE}`, `{TEXT}`, `{COLOR}`, `{INK}`,
`{WIDTH}`, `{HEIGHT}` where the values go (see the existing ones: the
`HEADER` and `OPEN_WINDOW` pieces are shared), add a row to `templates[]`
with its slots and defaults, run `make templates-test`, and add examples
to `my_tools.txt`.

## How the x87 math works

There's no C library, so `lib/math.c` builds the functions the model needs
from x87 instructions. The x87 is a stack of eight 80-bit registers; GCC's
`"t"` constraint puts a value in the top one (`st0`) and `"u"` in the one
below (`st1`).

| Function | How |
|----------|-----|
| `sqrtf(x)` | `fsqrt`. |
| `logf(x)` | `fyl2x` computes `st1 * log2(st0)`. With `st1 = ln 2` (`fldln2`), that's `ln 2 * log2 x = ln x`. |
| `expf(x)` | `e^x = 2^(x * log2 e)` (`fldl2e`). `f2xm1` computes `2^f - 1`, but only for `-1 <= f <= 1`, so split `y = x * log2 e` into a whole number `n` (`frndint`) and `f = y - n`. Then `2^y = (f2xm1(f) + 1) * 2^n`, and "`* 2^n`" is `fscale`. |
| `powf(x, y)` | `2^(y * log2 x)`, the same way. `x = 0` and negative `x` with a whole-number `y` are handled separately, since `log2` needs `x > 0`. |
| `sinf`, `cosf` | `fsin`, `fcos`. |

`math_self_test()` checks them all against known values at boot. Floats
must never be used in IRQ handlers, because the interrupt stubs don't save
the FPU's registers (see "The x87 FPU" in `INTERRUPTS.md`).

## Speed

In QEMU on an Apple M1, `stories260K` writes about 20-30 tokens per
second, and Iris (4 times as many parameters) about 7-10 tokens per
second, counting the tokens of your message she reads and the ones she
writes: a short answer takes 2-4 seconds, her longest about 9. QEMU emulates every x87 instruction in software, so each `fmul`
and `fadd` is a function call; a model with a million parameters does
about two million of them per token.

## Credits

- `run.c` from [llama2.c](https://github.com/karpathy/llama2.c) by Andrej
  Karpathy, MIT license, ported in `src/llm/` (license in
  `src/llm/LICENSE.llama2c`).
- The `stories260K` model and `tok512` tokenizer from
  [karpathy/tinyllamas](https://huggingface.co/karpathy/tinyllamas), MIT
  license. It was trained on the
  [TinyStories](https://huggingface.co/datasets/roneneldan/TinyStories)
  dataset by Ronen Eldan and Yuanzhi Li (CDLA-Sharing-1.0).

## Known limits

- Tools can only do what the tags describe: no arguments beyond the
  slots, one tool per reply.
- No top-p sampling (it needs a sort); argmax and plain temperature only.
- The model must fit in the 16 MB heap, next to the 3 MB screen buffer and
  the windows: the file is loaded whole, plus the KV cache.
- One CPU and x87 only: slow, as described under "Speed". SSE would make
  `matmul` faster in QEMU.
- The tokenizer's merging is `run.c`'s, which approximates SentencePiece;
  rare strings can be split differently than during training.
