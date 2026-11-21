"""
finetune.py - teaches an already trained Iris more, instead of starting over.

Used by `make chat-finetune` (see docs/TRAINING.md). It loads her
checkpoint (training/out/ckpt.pt from `make chat-model`), keeps the model's
size and tokenizer, and trains it further on the current work/ data with a
smaller learning rate, so she keeps what she knew and adds the new
conversations. Every EVAL_INTERVAL steps it measures the validation loss,
and saves the checkpoint and model.bin only when that loss is the best so
far: the result is the best Iris of the run, not the last one.

It runs on the Mac's GPU (--device mps). llama2.c's train.py gets stuck
there because its data loader copies batches to the GPU with
non_blocking=True from pinned memory, which on MPS can hand the model a
batch before the copy has finished. Here every copy is a plain blocking
one.

Usage: python finetune.py LLAMA2C_DIR BASE_CKPT OUT_DIR [--device mps]
"""
import argparse
import math
import os
import sys
import time

import numpy as np
import torch

BATCH_SIZE = 32
LEARNING_RATE = 3e-4    # a third of the first training's: nudge, don't reset
MIN_LR = 3e-5           # where the cosine schedule ends
WARMUP_ITERS = 100
MAX_ITERS = 1500
EVAL_INTERVAL = 100
EVAL_ITERS = 50         # batches per loss estimate
WEIGHT_DECAY = 0.1
BETAS = (0.9, 0.95)
GRAD_CLIP = 1.0
SEED = 1337


def batches(path, seq_len, device, rng):
    """Random windows of seq_len + 1 tokens: x is the window, y the same
    window one token later (what the model should predict)."""
    data = np.memmap(path, dtype=np.uint16, mode="r")
    while True:
        starts = rng.integers(0, len(data) - seq_len - 1, BATCH_SIZE)
        chunk = np.stack([data[s:s + seq_len + 1] for s in starts]).astype(np.int64)
        chunk = torch.from_numpy(chunk).to(device)      # blocking copy
        yield chunk[:, :-1].contiguous(), chunk[:, 1:].contiguous()


def learning_rate(step):
    """Linear warmup, then a cosine curve down to MIN_LR."""
    if step < WARMUP_ITERS:
        return LEARNING_RATE * (step + 1) / WARMUP_ITERS
    progress = (step - WARMUP_ITERS) / max(1, MAX_ITERS - WARMUP_ITERS)
    return MIN_LR + 0.5 * (1 + math.cos(math.pi * min(1.0, progress))) * (LEARNING_RATE - MIN_LR)


@torch.no_grad()
def estimate_loss(model, sources):
    model.eval()
    losses = {}
    for name, source in sources.items():
        total = 0.0
        for _ in range(EVAL_ITERS):
            x, y = next(source)
            model(x, y)
            total += model.last_loss.item()
        losses[name] = total / EVAL_ITERS
    model.train()
    return losses


def load_model(base_ckpt, device):
    from model import ModelArgs, Transformer
    checkpoint = torch.load(base_ckpt, map_location="cpu", weights_only=False)
    model = Transformer(ModelArgs(**checkpoint["model_args"]))
    state = {k.removeprefix("_orig_mod."): v for k, v in checkpoint["model"].items()}
    model.load_state_dict(state)
    return model.to(device), checkpoint["model_args"]


def save(model, model_args, out_dir, step, val_loss):
    from export import model_export
    torch.save({"model": model.state_dict(), "model_args": model_args,
                "iter_num": step, "best_val_loss": val_loss},
               os.path.join(out_dir, "ckpt.pt"))
    model_export(model, os.path.join(out_dir, "model.bin"), version=0)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("llama2c")
    parser.add_argument("base_ckpt")
    parser.add_argument("out_dir")
    parser.add_argument("--device", default="mps")
    args = parser.parse_args()
    sys.path.insert(0, args.llama2c)        # llama2.c's model.py and export.py
    torch.manual_seed(SEED)
    rng = np.random.default_rng(SEED)
    os.makedirs(args.out_dir, exist_ok=True)

    model, model_args = load_model(args.base_ckpt, args.device)
    data = os.path.join(args.llama2c, "data", f"tok{model_args['vocab_size']}")
    seq_len = model_args["max_seq_len"]
    train = batches(os.path.join(data, "data01.bin"), seq_len, args.device, rng)
    sources = {"train": batches(os.path.join(data, "data01.bin"), seq_len, args.device, rng),
               "val": batches(os.path.join(data, "data00.bin"), seq_len, args.device, rng)}
    optimizer = model.configure_optimizers(WEIGHT_DECAY, LEARNING_RATE, BETAS, "cpu")

    best = float("inf")
    start = time.time()
    for step in range(MAX_ITERS + 1):
        if step % EVAL_INTERVAL == 0:
            losses = estimate_loss(model, sources)
            note = ""
            if losses["val"] < best:
                best = losses["val"]
                save(model, model_args, args.out_dir, step, best)
                note = "  (best so far: saved)"
            print(f"step {step}: train loss {losses['train']:.4f}, val loss {losses['val']:.4f}"
                  f"{note}  [{time.time() - start:.0f}s]", flush=True)
        if step == MAX_ITERS:
            break
        for group in optimizer.param_groups:
            group["lr"] = learning_rate(step)
        x, y = next(train)
        model(x, y)
        optimizer.zero_grad(set_to_none=True)
        model.last_loss.backward()
        torch.nn.utils.clip_grad_norm_(model.parameters(), GRAD_CLIP)
        optimizer.step()
    print(f"finetune: best val loss {best:.4f}, saved in {args.out_dir}")


if __name__ == "__main__":
    main()
