"""
train_tokenizer.py - learns Iris's vocabulary from the training text.

Step 2 of `make chat-model` (see docs/TRAINING.md). Trains a SentencePiece
byte pair encoding with VOCAB_SIZE tokens on work/train.txt, using the
same settings as llama2.c's tinystories.py, then converts it to the
tokenizer.bin format that planckOS reads (llama2.c's tokenizer.py).

Usage: python train_tokenizer.py LLAMA2C_DIR VOCAB_SIZE
"""
import os
import sys

import sentencepiece as spm

HERE = os.path.dirname(os.path.abspath(__file__))


def main():
    llama2c, vocab_size = sys.argv[1], int(sys.argv[2])
    data_dir = os.path.join(llama2c, "data")
    os.makedirs(data_dir, exist_ok=True)
    prefix = os.path.join(data_dir, f"tok{vocab_size}")

    # Byte fallback: characters that didn't make it into the vocabulary
    # (like '\n') become one token per byte, so any text can be encoded.
    # identity: no Unicode normalization, the text is kept as it is.
    spm.SentencePieceTrainer.train(input=os.path.join(HERE, "work", "train.txt"),
                                   model_prefix=prefix,
                                   model_type="bpe",
                                   vocab_size=vocab_size,
                                   self_test_sample_size=0,
                                   input_format="text",
                                   character_coverage=1.0,
                                   num_threads=os.cpu_count(),
                                   split_digits=True,
                                   allow_whitespace_only_pieces=True,
                                   byte_fallback=True,
                                   unk_surface=r" \342\201\207 ",
                                   normalization_rule_name="identity",
                                   minloglevel=2)

    sys.path.insert(0, llama2c)
    from tokenizer import Tokenizer
    Tokenizer(prefix + ".model").export()     # writes tok<N>.bin next to it
    print(f"train_tokenizer: {prefix}.model and {prefix}.bin")


if __name__ == "__main__":
    main()
