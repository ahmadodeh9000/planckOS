# Training Iris

Files: `training/` (`my_chats.txt`, `more_chats.txt`, `my_tools.txt`,
`prepare_data.py`, `train_tokenizer.py`, `tokenize_data.py`,
`finetune.py`, `tool_test.txt`, `requirements.txt`), the `chat-model` and
`chat-finetune` targets in the `Makefile`. Training
uses Andrej Karpathy's [llama2.c](https://github.com/karpathy/llama2.c)
(MIT license), which `make chat-model` downloads into `training/llama2.c/`.

This page is for someone who has never trained a model. `LLM.md` explains
what the model does once it's trained.

## The short version

```sh
# edit training/my_chats.txt (and training/my_tools.txt), then:
make chat-model     # about 45 minutes on an M1 Mac
make disk-models    # replace the model on disk.img with your new one
make iris-eval      # optional: sample chats and the tool score, on the Mac
make run            # open Iris from the planckOS menu
```

The first `make chat-model` also creates a Python environment in
`training/.venv` and installs PyTorch and SentencePiece into it (see
"Setting up" at the end).

## What training is

**Predicting the next token.** A language model does one thing: given the
tokens so far, it gives every token in its vocabulary a score for "this
comes next". Iris's text is turned into tokens (pieces of words, 512
different ones), and training shows her real text one token at a time:
after `User: who made` the next token was ` you`, after that `\n`, then
`Iris`, and so on. Every one of those is a small exam.

**The model is a function with a million knobs.** The transformer in
`LLM.md` turns tokens into scores using about 1.07 million numbers, its
*parameters* (the weights). At the start they're random, so the scores are
random. Training is the search for numbers that give the right next token
high scores.

**Loss: how wrong the model is.** After each prediction we look at the
probability the model gave the token that really came next. If it said
90%, it was nearly right; if it said 1%, it was badly surprised. The loss
is `-ln(probability)`, averaged over many predictions: 0.1 for 90%, 4.6
for 1%. A model that guesses blindly among 512 tokens gives each 1/512,
a loss of `ln(512) = 6.24`. That's where Iris starts. Lower is better.

**Gradients: which way to turn each knob.** For every parameter, calculus
(the chain rule, done automatically by PyTorch) tells us how the loss
would change if that parameter went up a little. That's its *gradient*.
Moving every parameter a small step against its gradient makes the loss a
little smaller. Repeat that many times and the model gets good.

**The optimizer: how big the steps are.** llama2.c uses AdamW, which
keeps a running average of each parameter's gradients so it can take
steady steps where the gradient is consistent and careful ones where it
jumps around. The *learning rate* is the base step size. It starts small
(the warmup: random weights give wild gradients), rises to its maximum,
then slowly falls (cosine decay) so the model can settle into a good spot.

**Batches, iterations and epochs.** One step (an *iteration*) uses a
*batch*: 32 random snippets of 256 tokens, 8,192 predictions averaged into
one loss. An *epoch* is one pass over all the training data. Iris's data
is about 2.9 million tokens, so 4,000 iterations of 8,192 tokens are
about 11 epochs: the model sees each conversation about 11 times.

**Training and validation loss.** 5% of the conversations are kept aside
(`work/val.txt`) and never trained on. Every 500 iterations the loss is
measured on both. If the model only memorized the training text, its
training loss would keep falling while the validation loss stopped
falling or rose: that's **overfitting**. While the two stay close, the
model is learning things that carry over to text it hasn't seen.

## What `make chat-model` does, and why

| Step | Script | What and why |
|------|--------|--------------|
| 0 | | Creates `training/.venv` (Python + PyTorch) and clones llama2.c at a fixed commit, the first time only. |
| 1 | `prepare_data.py` | Reads `my_chats.txt` and `my_tools.txt`, turns every "several questions, one answer" block into separate examples, fills the `{placeholders}` in tool examples with random values, and writes 40,000 random conversations of 1 to 3 turns to `work/train.txt` and `work/val.txt`. Mixing examples into conversations teaches the turn format and that earlier turns don't change the answer to the current question. Small changes people make when typing (a capital letter, a question mark) are added at random, so she doesn't depend on them. |
| 2 | `train_tokenizer.py` | Learns a 512-token vocabulary from the text with SentencePiece's byte pair encoding: start from single characters and keep merging the most frequent pair (`t`+`he` -> `the`). Characters it doesn't have become one token per byte, so any text can be written. Then it's saved in llama2.c's `tok512.bin` format, which planckOS reads. |
| 3 | `tokenize_data.py` | Turns every conversation into token numbers, with the BOS token in front, exactly as planckOS will feed conversations to the model. Saves them as 16-bit numbers in `llama2.c/data/tok512/`. |
| 4 | llama2.c's `train.py` | The training loop described above. Every 500 iterations it prints the losses and, if the validation loss improved, saves `training/out/model.bin` in llama2.c's format. |
| 5 | | Copies `out/model.bin` to `models/iris.bin` and the tokenizer to `models/iris_tok.bin`. |

## The settings (hyperparameters)

They're in `IRIS_TRAIN_FLAGS` in the `Makefile`. To try other values,
edit them there and run `make chat-model` again.

| Setting | Iris | What it means |
|---------|------|---------------|
| `dim` | 128 | How many numbers describe each token inside the model. More = can represent more, but the model grows with `dim²`. |
| `n_layers` | 5 | How many transformer layers are stacked. More = deeper reasoning, linearly bigger and slower. |
| `n_heads` | 4 | Attention heads per layer (each looks for different things). `dim` must divide by it. |
| `n_kv_heads` | 4 | Key/value heads; fewer than `n_heads` shares them and shrinks the KV cache. |
| `max_seq_len` | 256 | The context length: the most tokens the model sees at once (her memory of the conversation). |
| `vocab_size` | 512 | Tokens in the vocabulary. Small = more tokens per word, but a smaller model. |
| `batch_size` | 32 | Snippets per iteration. |
| `learning_rate` | 1e-3 | The biggest step size. Too high and the loss jumps around or explodes; too low and it learns slowly. Small models like a high rate. |
| `max_iters` | 4000 | How many iterations to train. |
| `warmup_iters` | 200 | Iterations to ramp the learning rate up from 0. |
| `dropout` | 0.1 | During training, randomly ignore 10% of the activations, so the model can't depend on any single one. A guard against overfitting. |
| `eval_interval` | 500 | How often to measure the validation loss and save. |

**The parameter count**, from those settings (`hidden_dim` = 352 is
computed by llama2.c from `dim`):

```
embeddings      512 x 128                         =    65,536
per layer       attention 4 x 128 x 128           =    65,536
                feed-forward 3 x 128 x 352        =   135,168
                two RMSNorm weights 2 x 128       =       256
5 layers        5 x 200,960                       = 1,004,800
final RMSNorm                                     =       128
total                                             = 1,070,464  (4.3 MB as floats)
```

**Why so small.** Three limits in planckOS: the model file is loaded
whole into the 16 MB heap, next to the 3 MB screen buffer, the windows and
the KV cache (5 x 256 x 128 x 2 floats = 1.3 MB); the file has to fit on
the FAT12 disk; and QEMU emulates every x87 float instruction in software,
so speed falls with every parameter. At 1 million parameters she writes
about 10 tokens a second, a short answer in 2 to 4 seconds. A model 3
times as big would be roughly 3 times as slow.

## Reading the loss numbers

A run prints a line per iteration and the two losses every 500:

```
step 0: train loss 6.2742, val loss 6.2750       random weights: ln(512)
...
step 500: train loss 0.31, val loss 0.31          the format and most answers learned
step 1000: train loss 0.27, val loss 0.27
...
step 4000: train loss 0.25, val loss 0.25         (the numbers of the real run are under "Results")
1234 | loss 0.2618 | lr 7.0e-04 | 650ms         one iteration: its batch's loss,
                                                  the learning rate, the time it took
```

The loss falls fast at first (the easy patterns: the `User:`/`Iris:`
format, common words) and then slowly. **Why it doesn't go to 0:** a lot
of the text can't be predicted. Which question a user asks next is
random, and so are the made-up words in the tool examples. Even a perfect
model would be surprised by those. What matters is *her* lines, and those
are nearly certain after training; the average just doesn't show it.

**Signs that something is wrong:**

- The loss stays near 6 or stops around 4: the model isn't learning (see
  "Why the CPU").
- The loss jumps around or becomes `nan`: the learning rate is too high.
- The training loss keeps falling while the validation loss rises:
  overfitting. Train for fewer iterations, add dropout, or add more
  varied examples.

**Overfitting, Iris's way.** Some memorization is the point here: her
answers *should* come out word for word, because they're the facts about
planckOS. What must not happen is that she only understands the exact
questions in the file. That's why every answer has several phrasings,
why the tool values are random, and why `tool_test.txt` holds 50
requests that are deliberately *not* in the training data. `make
iris-eval` measures her on those. Two more held-out files test how she
chats: `tests/iris/eval_questions.txt` (30 single messages: planckOS,
small talk, feelings, things she can't know) and
`tests/iris/chatty_chats.txt` (8 short conversations, where she has to
remember what was just said). None of their lines is in the training
data, and they must stay out of it.

## Why the CPU

On an M1 Mac with PyTorch 2.5.1, `make chat-model` on the GPU
(`IRIS_DEVICE=mps`) often doesn't learn: the loss stalls at about 4
instead of going below 0.5, while the same data on the CPU learns every
time. The cause is llama2.c's data loader: it copies each batch to the
GPU with `non_blocking=True` from pinned memory, and on MPS the model can
start reading a batch before that copy has finished. So `make chat-model`
uses the CPU, about 0.6 seconds per iteration.

`make chat-finetune` (below) has its own small training loop in
`training/finetune.py`, whose copies are ordinary blocking ones, and runs
on the GPU without that problem.

## Fine-tuning: teaching the trained Iris more

`make chat-model` starts from random numbers every time. To teach the Iris
you already have something new, without starting over:

```sh
make chat-finetune      # about 8 minutes on an M1 Mac's GPU
make disk-models
```

It loads her checkpoint (`training/out/ckpt.pt`, which `make chat-model`
writes; it isn't in git), keeps her size and tokenizer, and trains her
further on the current data with a third of the learning rate: she keeps
what she knew and adds the new examples. Every 100 steps it measures the
validation loss and saves only when it's the best so far
(`training/out/finetune/`), so the result is the best Iris of the run,
not the last one.

The data must be tokenized with *her* tokenizer: a model only understands
the token numbers it was trained on. `make chat-model` makes a new
SentencePiece tokenizer every time, so `chat-finetune` uses
`models/iris_tok.bin` itself, with the same encoder planckOS runs
(`tokenize_data.py`, checked against `src/llm/tokenizer.c`).

## Changing what Iris says

`training/my_chats.txt` is yours to edit. Its format is described at the
top of the file: `User:` lines are ways of asking, the `Iris:` line after
them is her answer (one line: she stops writing at the end of a line).
To teach her something new, add a block with several phrasings:

```
User: what is the best os
User: which os is the best
User: is planckOS good
Iris: planckOS, of course! but i might be a little biased.
```

Tools are in `training/my_tools.txt` (see "Tools" in `LLM.md`). Then
retrain and copy the model onto the disk:

```sh
make chat-model
make disk-models
```

`make chat-model` starts from scratch every time and takes about the same
time. (`make disk-models` is needed because `make run` never overwrites
an `IRIS.BIN` it didn't put on the disk itself, and a model you trained
is different from the published one.)

**Sharing a retrained Iris.** Everyone else gets the model from the GitHub
release `iris-v2` (`make models`; the first version, `iris-v1`, stays
available). To publish a new one (maintainer only):

```sh
make publish-model                       # uploads models/iris*.bin to the release
git add models/MODELS.sha256 && git commit -m "models: publish the retrained Iris"
```

`make publish-model` uses the GitHub CLI (`gh`, logged in), replaces the
release's assets and writes the new checksums into `models/MODELS.sha256`;
`make models` refuses files that don't match it, so commit it right away. Then `make iris-eval` shows her answers to the sample conversations
in `tests/iris/sample_chats.txt` and scores the tools, before you boot.

**Tips.** More phrasings help more than more facts. Keep answers short:
she has 256 tokens of context and stops at 96 tokens per answer (about 70
words; each letter of a name like PROG.ASM costs a token). Every answer
you write is what she'll say, so check the facts in it. Commands she
mentions should match `help`, since the fact cards will show the real line
next to her answer anyway.

## The data

All of Iris's training data is written for planckOS in `my_chats.txt`,
`more_chats.txt` and `my_tools.txt`; no public dataset is used.

- `my_chats.txt` is the main file: questions about planckOS and her own
  answers. It counts three times as much as `more_chats.txt`.
- `more_chats.txt` is everyday conversation: small talk, feelings ("i'm
  tired", "this is hard"), good news, jokes, saying "i'm not sure" instead
  of guessing, the same questions typed in slang or with typos, and short
  conversations where she has to remember what was said ("call me Sam" ...
  "who am i"). `{name}`, `{color}` and `{thing}` get a random value per
  conversation, often a made-up name, so she learns to copy rather than to
  memorize one name.
- `prepare_data.py` drops near-duplicate questions (the same once case,
  punctuation and spaces are ignored; `my_chats.txt` wins) and any line of
  `more_chats.txt` within 85% similarity of a held-out test message.
  Tool requests are 70% of the single turns, so more chat doesn't crowd
  out the tools.

The facts in it come from the code: the commands and their usage from `help`, the apps, Notepad's keys,
and how to write and run a program.

## Results

### iris-v2: the chattier Iris (`make chat-finetune`)

The current `models/iris.bin`: iris-v1 (below) fine-tuned with
`more_chats.txt`, the same size and tokenizer.

- **Data:** 1,113 chat examples (`my_chats.txt` counted three times), 45
  conversations, 113 tool patterns, after dropping 29 near-duplicates and
  17 lines too close to the tests: 40,000 conversations, 3.3 million
  training tokens.
- **Training:** 1,500 steps on an M1 Mac's GPU in 8 minutes 43 seconds.
  The validation loss went from 1.649 (iris-v1 on the new data) to a best
  of **0.2548** at step 1400, which is the step that was kept.
- **Tools, on the 50 held-out requests:** her tag was right 34 times
  (68%, iris-v1: 66%), and the right tool ran all 50 times, as before.
  A first run with tool requests at 60% of turns scored 60%; at 70% they
  held.
- **Speed in QEMU:** the same as iris-v1 (same size, so the same work per
  token).
- **Chatting** (`tests/iris/eval_questions.txt`, `chatty_chats.txt`, never
  trained on): "im so tired today", "this is really hard", "i finally got
  my program working!!", "what is 17 times 23", "is it going to rain
  tomorrow" and "i'm new here" now get warm or honest answers where iris-v1
  answered with a command list or a random tool tag; "another one" after a
  joke gets a different joke. Still weak: remembering a name told in a way
  the data doesn't use ("my name is dana" ... "what's my name"), some small
  talk still triggers a tool tag, and "and how do i write one" after "how
  do i run a program" now gets the Notepad answer instead of the assembly
  one.

### iris-v1: trained from scratch (`make chat-model`)

The first release, and the starting point for iris-v2:

- **Model:** 1,070,464 parameters (`dim` 128, 5 layers, 4 heads,
  vocabulary 512, context 256), 4.3 MB.
- **Data:** 288 chat examples, 4 longer conversations and 113 tool
  patterns, expanded into 40,000 conversations: 2.9 million training
  tokens and 155,000 validation tokens.
- **Training time:** 37 minutes 35 seconds for `make chat-model` on an
  Apple M1 (CPU), 4,000 iterations at about 0.55 seconds each.
- **Loss:** from 6.274 (train) / 6.275 (validation) at the start to
  **0.284 / 0.304** at the end. The two stayed close the whole time, so
  she isn't overfitting:

  ```
  step 0:    train loss 6.2742, val loss 6.2750
  step 500:  train loss 0.4360, val loss 0.4535
  step 1000: train loss 0.3427, val loss 0.3622
  step 2000: train loss 0.3006, val loss 0.3185
  step 3000: train loss 0.2878, val loss 0.3077
  step 4000: train loss 0.2836, val loss 0.3042
  ```

- **Tools, on the 50 held-out requests** (`make iris-eval`):

  | | |
  |---|---|
  | her tag was exactly right | 37 (74%) |
  | the keyword fallback stepped in | 12 times, right all 12 |
  | the right tool ran in the end | 49 (98%) |

  Most of her mistakes are copying names she has never seen
  (`[open "JORIRAL.TXT"]` for JOURNAL.TXT); those values aren't in the
  message, so the fallback takes over. A few are the wrong tool
  (`[memory]` for "how much storage is used"); a tag without values is
  only believed if the message has a word about it, so the fallback
  takes those too. The one miss: "is there room for more files" got
  `[find "more"]`, and "more" *is* in the message. Honest note: the
  whole-word and keyword checks were added after looking at this test
  set's failures (before them, the right tool ran 45 times out of 50), so
  the 98% is a little optimistic for new requests.
- **Speed in QEMU:** 7 to 10 tokens per second (read and written), measured
  from the `iris: replied in ...` lines in the serial log: "what time is
  it" takes 1.4 seconds, "how do i list files" 3.7, her longest answers
  about 9.

**Ten sample conversations** (`tests/iris/sample_chats.txt`, run on the
Mac by `make iris-eval`; the tags are what she wrote, which the app turns
into the tool's answer):

```
User: hi
Iris: hi! i'm iris, planckOS's assistant. what can i help you with?
User: who are you
Iris: i'm iris, planckOS's assistant. i help you find your way around planckOS.
User: what can you do
Iris: i can explain commands and apps, check your disk, memory and the time,
      find and open files, and even make small apps. just ask!

User: how do i make a text file
Iris: Notepad is the text editor. type your text, put a name like NOTES.TXT in
      the name box and press Save.
User: how do i save it
Iris: [disk]                      (no disk word in the message: the app says
                                   "i didn't get that, try rephrasing ...")
User: thanks
Iris: you're welcome! happy to help.

User: who made planckOS
Iris: planckOS was made by ahmad odeh.
User: what language is it written in
Iris: planckOS is written in c and nasm assembly.

User: how do i run a program
Iris: `exec <file>` runs a program, for example `exec HELLO.ELF`.
User: and how do i write one
Iris: write assembly in Notepad and save it as PROG.ASM, then in the Terminal
      type `asm PROG.ASM PROG.ELF` and `exec PROG.ELF`.

User: is my disk full
Iris: [disk]
User: what time is it
Iris: [time]

User: find recipe
Iris: [find "recipe"]
User: open NOTES.TXT
Iris: [open "NOTES.TXT"]

User: make a blue counter called Taps
Iris: [make-app counter title="Taps" color=blue]
User: make a window that says Good luck
Iris: [make-app message text="Good morning"]   (not in the message: the
                                               fallback makes "Good luck")

User: what is the capital of spain
Iris: [disk]                      (no disk word: "i didn't get that ...")
User: tell me a joke
Iris: why did the kernel stay calm? it knew how to handle every interrupt.

User: how do i shut down
Iris: click the planckOS button and choose Shut down, or type `quit` in the
      Terminal.
User: bye
Iris: bye! come back anytime.
```

Two of her weak spots show here: a follow-up that depends on the
previous answer ("how do i save it") and a question far from her training
data ("the capital of spain") sometimes get a random tag instead of
words. More examples of both in `my_chats.txt` would help.

## Setting up

`make chat-model` does this the first time; to do it by hand:

```sh
python3 -m venv training/.venv
training/.venv/bin/pip install -r training/requirements.txt
git clone https://github.com/karpathy/llama2.c.git training/llama2.c
git -C training/llama2.c checkout 350e04fe35433e6d2941dce5a1f53308f87058eb
```

`requirements.txt` pins PyTorch 2.5.1, NumPy, SentencePiece, tqdm and
requests (llama2.c's scripts import them). `training/.venv`,
`training/llama2.c`, `training/work` and `training/out` are not in git.

## Credits

- [llama2.c](https://github.com/karpathy/llama2.c) by Andrej Karpathy
  (MIT license): `train.py`, `model.py`, `export.py` and `tokenizer.py`
  do the training and save the model in the format `src/llm/` reads.
  `train_tokenizer.py` uses the same SentencePiece settings as llama2.c's
  `tinystories.py`.
- [SentencePiece](https://github.com/google/sentencepiece) (Apache 2.0)
  and [PyTorch](https://pytorch.org) (BSD-style license).
