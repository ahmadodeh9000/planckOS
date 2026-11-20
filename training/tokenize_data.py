"""
tokenize_data.py - turns the training text into token numbers.

Step 3 of `make chat-model` (see docs/TRAINING.md). Every conversation in
work/val.txt and work/train.txt becomes BOS + its tokens, exactly how
planckOS feeds a conversation to the model. llama2.c's train.py reads them
from LLAMA2C_DIR/data/tok<N>/ as 16-bit numbers: the first file (data00)
is the validation set, the rest is training data.

Usage: python tokenize_data.py LLAMA2C_DIR VOCAB_SIZE
"""
import os
import sys

import numpy as np
import sentencepiece as spm

HERE = os.path.dirname(os.path.abspath(__file__))
BOS = 1


def tokenize(sp, path):
    text = open(path, encoding="utf-8").read()
    tokens = []
    for conversation in text.split("\n\n"):
        if conversation.strip():
            tokens.append(BOS)
            tokens.extend(sp.encode(conversation.strip()))
    return np.array(tokens, dtype=np.uint16)


def main():
    llama2c, vocab_size = sys.argv[1], int(sys.argv[2])
    sp = spm.SentencePieceProcessor(model_file=os.path.join(llama2c, "data", f"tok{vocab_size}.model"))
    out_dir = os.path.join(llama2c, "data", f"tok{vocab_size}")
    os.makedirs(out_dir, exist_ok=True)
    for name, source in (("data00.bin", "val.txt"), ("data01.bin", "train.txt")):
        tokens = tokenize(sp, os.path.join(HERE, "work", source))
        tokens.tofile(os.path.join(out_dir, name))
        print(f"tokenize_data: {source}: {len(tokens):,} tokens -> {name}")


if __name__ == "__main__":
    main()
