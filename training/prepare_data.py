"""
prepare_data.py - turns my_chats.txt and my_tools.txt into training text.

Step 1 of `make chat-model` (see docs/TRAINING.md).

Reads the two files, expands every "several User lines -> one Iris line"
block into separate examples, fills the {placeholders} in tool examples
with random values, and writes many short random conversations (1 to 3
turns each) to work/train.txt and work/val.txt, one conversation per
paragraph:

    User: hi
    Iris: hi! i'm iris, planckOS's assistant. what can i help you with?
    User: is my disk full
    Iris: [disk]

Mixing examples into random conversations teaches Iris the turn format
and that earlier turns don't change the answer to the current one.

Usage: python prepare_data.py [--seed N]
"""
import argparse
import os
import random
import re

HERE = os.path.dirname(os.path.abspath(__file__))
WORK = os.path.join(HERE, "work")

CONVERSATIONS = 40000   # how many conversations to write
VAL_FRACTION = 0.05     # share kept apart to measure overfitting
MAX_TURNS = 3           # turns per conversation (1..MAX_TURNS)
TOOL_REPEATS = 6        # tool blocks are sampled this many times more,
                        # because each one needs many different fills
STORY_CHANCE = 0.08     # chance to use a fixed multi-turn conversation
FILL_FROM_CHATS = 0.35  # chance a fill comes from my_chats.txt's words
FILL_MADE_UP = 0.35     # chance a fill is a made-up word ("bofira"): Iris
                        # can't memorize those, so she learns to copy what
                        # you typed letter by letter, even names she's
                        # never seen
CONSONANTS = "bcdfghjklmnprstvwz"
VOWELS = "aeiou"

PLACEHOLDER = re.compile(r"\{(\w+)\}")


def read_blocks(path):
    """Split a file into blocks of (speaker, text) lines; '@' lines go to lists."""
    blocks, lists, block = [], {}, []
    for raw in open(path, encoding="utf-8"):
        line = raw.rstrip("\n")
        if line.startswith("#"):
            continue
        if line.startswith("@"):
            name, _, values = line[1:].partition(":")
            lists[name.strip()] = [v.strip() for v in values.split("|") if v.strip()]
            continue
        if not line.strip():
            if block:
                blocks.append(block)
            block = []
            continue
        speaker, _, text = line.partition(":")
        if speaker not in ("User", "Iris"):
            raise SystemExit(f"{path}: lines must start with User: or Iris:\n  {line}")
        block.append((speaker, text.strip()))
    if block:
        blocks.append(block)
    return blocks, lists


def split_block(block):
    """A block is either alternatives (User, User, ..., Iris) or a story
    (User, Iris, User, Iris, ...). Returns ('pairs', [(q, a)...]) or
    ('story', [(q, a)...])."""
    speakers = [s for s, _ in block]
    if speakers[-1] != "Iris":
        raise SystemExit(f"block must end with an Iris line: {block[0][1]}")
    if speakers.count("Iris") == 1:
        answer = block[-1][1]
        return "pairs", [(q, answer) for s, q in block[:-1]]
    turns = []
    for i in range(0, len(block), 2):
        if block[i][0] != "User" or block[i + 1][0] != "Iris":
            raise SystemExit(f"conversation must alternate User/Iris: {block[0][1]}")
        turns.append((block[i][1], block[i + 1][1]))
    return "story", turns


def made_up_word(rng):
    """A pronounceable nonsense word, 3 to 8 letters."""
    length = rng.randint(3, 8)
    start = rng.randint(0, 1)
    return "".join(rng.choice(CONSONANTS if (i + start) % 2 == 0 else VOWELS)
                   for i in range(length))


def random_value(name, lists, extra_words, rng):
    """A value for {name}: from its @ list, my_chats.txt, or made up."""
    roll = rng.random()
    if name in ("word", "title", "file", "text") and roll < FILL_MADE_UP:
        word = made_up_word(rng)
    elif name in ("word", "title", "text") and roll < FILL_MADE_UP + FILL_FROM_CHATS:
        word = rng.choice(extra_words)
    else:
        return rng.choice(lists[name])
    if name == "title":
        return word.capitalize()
    if name == "file":
        ext = rng.choice([".TXT", ".ASM", ".TXT"])
        return (word[:8] + ext).upper() if rng.random() < 0.8 else (word[:8] + ext).lower()
    if name == "text":
        return " ".join([word.capitalize()] + [rng.choice(extra_words)
                                               for _ in range(rng.randint(0, 2))])
    return word


def fill(question, answer, lists, extra_words, rng):
    """Replace each {name} with one random value, the same in both lines."""
    values = {}
    for name in set(PLACEHOLDER.findall(question + answer)):
        if name not in lists:
            raise SystemExit(f"no @{name} list for {{{name}}}")
        values[name] = random_value(name, lists, extra_words, rng)
    expand = lambda text: PLACEHOLDER.sub(lambda m: values[m.group(1)], text)
    return expand(question), expand(answer)


def vary(question, rng):
    """Small changes people make when typing: a capital, a question mark."""
    if rng.random() < 0.3:
        question = question[:1].upper() + question[1:]
    if rng.random() < 0.25 and not question.endswith(("?", "!", ".")):
        question += "?"
    return question


def load_examples():
    chat_blocks, _ = read_blocks(os.path.join(HERE, "my_chats.txt"))
    tool_blocks, lists = read_blocks(os.path.join(HERE, "my_tools.txt"))
    pairs, stories, tool_pairs = [], [], []
    for block in chat_blocks:
        kind, turns = split_block(block)
        (pairs if kind == "pairs" else stories).extend(turns if kind == "pairs" else [turns])
    for block in tool_blocks:
        kind, turns = split_block(block)
        tool_pairs.extend(turns)
    # Plain words (3+ letters) from the chats, for more varied fills.
    text = " ".join(q + " " + a for q, a in pairs)
    extra_words = sorted(set(w for w in re.findall(r"[a-z]{3,}", text)))
    return pairs, stories, tool_pairs, lists, extra_words


def random_conversation(examples, rng):
    pairs, stories, tool_pairs, lists, extra_words = examples
    if rng.random() < STORY_CHANCE:
        return rng.choice(stories)
    turns = []
    for _ in range(rng.randint(1, MAX_TURNS)):
        # Tools are picked TOOL_REPEATS times as often, per example.
        weight_tools = len(tool_pairs) * TOOL_REPEATS
        if rng.random() < weight_tools / (weight_tools + len(pairs)):
            question, answer = fill(*rng.choice(tool_pairs), lists, extra_words, rng)
        else:
            question, answer = rng.choice(pairs)
        turns.append((vary(question, rng), answer))
    return turns


def format_conversation(turns):
    return "\n".join(f"User: {q}\nIris: {a}" for q, a in turns)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--seed", type=int, default=1234)
    args = parser.parse_args()
    rng = random.Random(args.seed)
    examples = load_examples()
    os.makedirs(WORK, exist_ok=True)

    conversations = [format_conversation(random_conversation(examples, rng))
                     for _ in range(CONVERSATIONS)]
    n_val = int(CONVERSATIONS * VAL_FRACTION)
    for name, part in (("val.txt", conversations[:n_val]), ("train.txt", conversations[n_val:])):
        with open(os.path.join(WORK, name), "w", encoding="utf-8") as f:
            f.write("\n\n".join(part) + "\n")
    pairs, stories, tool_pairs = examples[:3]
    print(f"prepare_data: {len(pairs)} chat examples, {len(stories)} conversations, "
          f"{len(tool_pairs)} tool patterns -> {CONVERSATIONS} conversations in work/")


if __name__ == "__main__":
    main()
