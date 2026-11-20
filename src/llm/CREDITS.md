# Credits for src/llm/

| What | From | License |
|------|------|---------|
| `model.c`, `tokenizer.c`, `sampler.c` and the generation loop in `llm.c` | a port of `run.c` from [llama2.c](https://github.com/karpathy/llama2.c) by Andrej Karpathy (commit 350e04f) | MIT, see `LICENSE.llama2c` |
| The test model `stories260K` and tokenizer `tok512` (`make models`, used by `llm-test`) | [karpathy/tinyllamas](https://huggingface.co/karpathy/tinyllamas) | MIT |
| The dataset `stories260K` was trained on | [TinyStories](https://huggingface.co/datasets/roneneldan/TinyStories) by Ronen Eldan and Yuanzhi Li | CDLA-Sharing-1.0 |
| Iris's model (`make chat-model`) | trained with llama2.c's `train.py`, `model.py`, `export.py` and `tokenizer.py` (MIT), SentencePiece (Apache 2.0) and PyTorch (BSD-style) | |
| Iris's training data | written for planckOS: `training/my_chats.txt` and `training/my_tools.txt`; no public dataset | GPL v3, like the rest of planckOS |

The model files are downloaded or trained on your computer and are not
stored in this repository (`models/README.md`).
