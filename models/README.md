# models/

Language model files for planckOS (see `docs/LLM.md`). The `.bin` files
are not in git: `make models` (and `make run`, which calls it) downloads
whichever are missing, and checks every one against `MODELS.sha256`. A
file with the wrong checksum is never used.

| File | What it is | Where it comes from | License |
|------|------------|---------------------|---------|
| `iris.bin`, `iris_tok.bin` | Iris, planckOS's assistant: a 1.07M-parameter model and its 512-token tokenizer | assets of the GitHub release [`iris-v2`](https://github.com/ahmadodeh9000/planckOS/releases/tag/iris-v2) | GNU GPL v3, like planckOS |
| `stories260K.bin`, `tok512.bin` | the test model for `llm-test` | [karpathy/tinyllamas](https://huggingface.co/karpathy/tinyllamas) | MIT |

**Iris's training data and what it allows.** Iris was trained only on
`training/my_chats.txt`, `more_chats.txt` and `my_tools.txt`, conversations written
for planckOS and part of this GPL-licensed repository, so the model is
published under the same license. The tools used to train her don't limit
how the model may be shared: llama2.c's training code (MIT), SentencePiece
(Apache 2.0) and PyTorch (BSD-style) are licenses for their code, not for
what's trained with it.

**The test model.** `stories260K` is Andrej Karpathy's, MIT licensed, and
was trained on the [TinyStories](https://huggingface.co/datasets/roneneldan/TinyStories)
dataset (CDLA-Sharing-1.0, which puts no conditions on models trained on
it). It's downloaded from its original place, not re-published here.

`make run` copies the models onto `disk.img` with 8.3 names (`IRIS.BIN`,
`IRISTOK.BIN`, `S260K.BIN`, `TOK512.BIN`), only when they changed, and
never over a file of yours. `make disk-models` replaces them on purpose.

**After retraining** (`make chat-model`, `docs/TRAINING.md`), the
maintainer runs `make publish-model`: it uploads the new files to the
release and updates `MODELS.sha256`, which then has to be committed.
