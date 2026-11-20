# models/

Language model files for planckOS (see `docs/LLM.md`). The `.bin` files
are not in git.

| File | Where it comes from | License |
|------|---------------------|---------|
| `stories260K.bin`, `tok512.bin` | `make models` downloads them from [karpathy/tinyllamas](https://huggingface.co/karpathy/tinyllamas) and checks them against `SHA256SUMS` | MIT. Trained on [TinyStories](https://huggingface.co/datasets/roneneldan/TinyStories) (CDLA-Sharing-1.0) |

`make disk-models` copies them onto `disk.img` with 8.3 names.
