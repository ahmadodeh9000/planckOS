"""
tokenize_data.py - turns the training text into token numbers.

Step 3 of `make chat-model` (see docs/TRAINING.md). Every conversation in
work/val.txt and work/train.txt becomes BOS + its tokens, exactly how
planckOS feeds a conversation to the model. llama2.c's train.py reads them
from LLAMA2C_DIR/data/tok<N>/ as 16-bit numbers: the first file (data00)
is the validation set, the rest is training data.

Usage: python tokenize_data.py LLAMA2C_DIR VOCAB_SIZE [TOKENIZER.bin]

With a third argument, the text is tokenized with that llama2.c tokenizer
file (models/iris_tok.bin) instead of the SentencePiece model, the same
way planckOS does it (src/llm/tokenizer.c): characters first (bytes for
unknown ones), then the best-scoring neighbors merged until none can be.
Fine-tuning needs this: it must use the tokenizer Iris was trained with,
and only iris_tok.bin is kept (`make chat-model` makes a new SentencePiece
model every time).
"""
import os
import re
import struct
import sys

import numpy as np
import sentencepiece as spm

HERE = os.path.dirname(os.path.abspath(__file__))
BOS = 1


BYTE_OFFSET = 3     # <0x00> is token 3: unknown bytes become 3 + byte
CHUNK = re.compile(r"\n| [^ \n]*|[^ \n]+")


class LlamaTokenizer:
    """llama2.c's encoder, reading a tokenizer .bin file."""

    def __init__(self, path):
        data = open(path, "rb").read()
        self.ids, self.scores = {}, {}
        offset = 4                      # after max_token_length
        while offset < len(data):
            score, length = struct.unpack_from("fi", data, offset)
            piece = data[offset + 8:offset + 8 + length].decode("utf-8", "replace")
            self.ids.setdefault(piece, len(self.scores))
            self.scores[len(self.scores)] = score
            offset += 8 + length
        self.pieces = {token: piece for piece, token in self.ids.items()}
        self.cache = {}

    def encode_chunk(self, chunk):
        """No piece spans a space (except a leading one) or a newline, so
        each word with its leading space can be encoded on its own."""
        tokens = []
        for ch in chunk:
            if ch in self.ids:
                tokens.append(self.ids[ch])
            else:
                tokens += [BYTE_OFFSET + b for b in ch.encode("utf-8")]
        pieces = self.pieces
        while True:
            best, best_at = None, -1
            for i in range(len(tokens) - 1):
                merged = self.ids.get(pieces.get(tokens[i], "") + pieces.get(tokens[i + 1], ""))
                if merged is not None and (best is None or self.scores[merged] > self.scores[best]):
                    best, best_at = merged, i
            if best is None:
                return tokens
            tokens[best_at:best_at + 2] = [best]

    def encode(self, text):
        tokens = []
        for chunk in CHUNK.findall(" " + text):     # " ": the dummy prefix
            if chunk not in self.cache:
                self.cache[chunk] = self.encode_chunk(chunk)
            tokens += self.cache[chunk]
        return tokens


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
    if len(sys.argv) > 3:
        sp = LlamaTokenizer(sys.argv[3])
    else:
        sp = spm.SentencePieceProcessor(model_file=os.path.join(llama2c, "data", f"tok{vocab_size}.model"))
    out_dir = os.path.join(llama2c, "data", f"tok{vocab_size}")
    os.makedirs(out_dir, exist_ok=True)
    for name, source in (("data00.bin", "val.txt"), ("data01.bin", "train.txt")):
        tokens = tokenize(sp, os.path.join(HERE, "work", source))
        tokens.tofile(os.path.join(out_dir, name))
        print(f"tokenize_data: {source}: {len(tokens):,} tokens -> {name}")


if __name__ == "__main__":
    main()
