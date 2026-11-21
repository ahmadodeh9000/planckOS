# planckOS build
#
# Compiles every .c and .s file under src/ (plus boot/boot.s) into build/,
# links the kernel with link.ld and wraps it in a GRUB rescue ISO.
#
#   make              build build/planckOS.iso (does not start QEMU)
#   make iso          same as make
#   make run          everything: check the tools, build, create disk.img if
#                     needed, get the models, copy what changed onto the disk,
#                     and boot it in QEMU
#   make run-headless the same without a window; serial output goes to stdout
#   make disk         create a blank FAT12 disk.img if it does not exist
#   make programs     build programs/*.c and copy them, plus the programs/*.asm
#                     examples, onto disk.img (mtools)
#   make asm-test     check the built-in assembler's bytes against NASM
#   make higgson-host build the Higgson compiler for this computer
#   make models       download the models that are missing or wrong (checksummed)
#   make disk-models  copy the models onto disk.img, replacing what's there
#   make chat-model   train Iris, the chat model, on this computer (training/)
#   make publish-model  upload a retrained Iris to the GitHub release (maintainer)
#   make clean        remove build/

# ---- toolchain ---------------------------------------------------------
# Prefer an i686-elf cross compiler (Homebrew and most Linux distros ship
# one). Fall back to the host gcc in 32-bit freestanding mode.
ifneq ($(shell command -v i686-elf-gcc 2>/dev/null),)
    CC   := i686-elf-gcc
    GRUB := i686-elf-grub-mkrescue
else
    CC   := gcc
    GRUB := grub-mkrescue
    HOST_CFLAGS  := -m32 -fno-pic -fno-pie
    HOST_LDFLAGS := -m32 -no-pie -Wl,-m,elf_i386
endif

AS   := nasm
QEMU := qemu-system-i386

CFLAGS  := -std=gnu99 -ffreestanding -fno-stack-protector -fno-builtin \
           -nostdlib -Wall -Wextra -O2 -g -Isrc $(HOST_CFLAGS)
ASFLAGS := -f elf32 -g
LDFLAGS := -T link.ld -nostdlib -ffreestanding $(HOST_LDFLAGS)

# ---- files -------------------------------------------------------------
BUILD  := build
KERNEL := $(BUILD)/kernel.elf
ISO    := $(BUILD)/planckOS.iso
DISK   ?= disk.img

DISK_SIZE_MB             := 64
DISK_SECTORS_PER_CLUSTER := 64

C_SRCS   := $(shell find src -name '*.c')
ASM_SRCS := boot/boot.s $(shell find src -name '*.s')
OBJS     := $(patsubst %,$(BUILD)/%.o,$(ASM_SRCS) $(C_SRCS))
DEPS     := $(OBJS:.o=.d)

# Programs for 'exec': each programs/<name>.c becomes build/programs/<name>.elf,
# copied to the disk as <NAME>.ELF (so <name> must be at most 8 characters).
# The assembly examples programs/*.asm are copied as they are, as <NAME>.ASM,
# for the built-in 'asm' command.
PROG_SRCS    := $(wildcard programs/*.c)
PROG_ASM     := $(wildcard programs/*.asm)
PROGRAMS     := $(patsubst programs/%.c,$(BUILD)/programs/%.elf,$(PROG_SRCS))
PROG_CFLAGS  := -std=gnu99 -ffreestanding -fno-stack-protector -fno-builtin \
                -nostdlib -fno-pic -Wall -Wextra -O2 $(HOST_CFLAGS)
PROG_LDFLAGS := -T programs/program.ld -Wl,-z,max-page-size=0x1000 $(HOST_LDFLAGS)

# -rtc base=localtime: the guest's CMOS clock shows the host's local time
# instead of UTC.
QEMU_FLAGS := -boot d -cdrom $(ISO) -m 128M -serial stdio -rtc base=localtime \
              -drive file=$(DISK),format=raw,index=1,media=disk

# ---- targets -----------------------------------------------------------
.PHONY: all iso run run-headless ready check-tools disk programs disk-sync asm-test \
        templates-test iris-eval higgson-host models disk-models chat-model publish-model clean

all: iso

iso: $(ISO)

$(ISO): $(KERNEL) boot/grub.cfg
	@mkdir -p $(BUILD)/iso/boot/grub
	cp $(KERNEL) $(BUILD)/iso/boot/kernel
	cp boot/grub.cfg $(BUILD)/iso/boot/grub/grub.cfg
	$(GRUB) -o $@ $(BUILD)/iso 2>/dev/null

$(KERNEL): $(OBJS) link.ld
	$(CC) $(LDFLAGS) -o $@ $(OBJS) -lgcc

$(BUILD)/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/%.s.o: %.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

# 'make run' does everything a fresh clone needs, in this order: check the
# tools (before touching anything), build the ISO, create disk.img if it's
# missing, build the example programs, get the models, copy onto the disk
# what changed, boot. The second make guarantees that order.
run: check-tools
	@$(MAKE) --no-print-directory ready
	$(QEMU) $(QEMU_FLAGS)

run-headless: check-tools
	@$(MAKE) --no-print-directory ready
	$(QEMU) $(QEMU_FLAGS) -display none

ready: $(ISO) disk-sync

check-tools:
	@sh scripts/check_tools.sh

# Only creates the disk when it is missing, so an existing disk.img (and the
# files on it) is never overwritten.
disk: $(DISK)

$(DISK):
	dd if=/dev/zero of=$@ bs=1048576 count=$(DISK_SIZE_MB) 2>/dev/null
ifeq ($(shell uname),Darwin)
	@# newfs_msdos only formats devices, so attach the file as one first.
	dev=$$(hdiutil attach -imagekey diskimage-class=CRawDiskImage -nomount $@ | awk 'NR==1{print $$1}'); \
	newfs_msdos -F 12 -c $(DISK_SECTORS_PER_CLUSTER) -v PLANCK $$dev; \
	hdiutil detach $$dev >/dev/null
else
	mkfs.fat -F 12 -s $(DISK_SECTORS_PER_CLUSTER) -n PLANCK $@
endif

# Language models (docs/LLM.md). They are not in git: Iris's model and
# tokenizer are assets of the GitHub release $(RELEASE_TAG), the test model
# stories260K comes from karpathy/tinyllamas (MIT license). 'make models'
# downloads whatever is missing or doesn't match models/MODELS.sha256.
RELEASE_TAG   := iris-v1
RELEASE_URL   := https://github.com/ahmadodeh9000/planckOS/releases/download/$(RELEASE_TAG)
MODELS_URL    := https://huggingface.co/karpathy/tinyllamas/resolve/main/stories260K
MODEL_SOURCES := iris.bin=$(RELEASE_URL)/iris.bin iris_tok.bin=$(RELEASE_URL)/iris_tok.bin \
                 stories260K.bin=$(MODELS_URL)/stories260K.bin tok512.bin=$(MODELS_URL)/tok512.bin

# What goes onto the disk: models/<file>:<8.3 name>, and the example programs.
DISK_MODELS := models/iris.bin:IRIS.BIN models/iris_tok.bin:IRISTOK.BIN \
               models/stories260K.bin:S260K.BIN models/tok512.bin:TOK512.BIN
DISK_FILES  := $(DISK_MODELS) $(PROGRAMS) $(PROG_ASM)
DISK_STAMPS := $(BUILD)/disk-sync

models:
	@sh scripts/get_models.sh models $(MODEL_SOURCES)

# Copies onto the disk only what changed since the last copy, and never
# over a file that planckOS didn't put there (scripts/sync_disk.sh).
disk-sync: $(DISK) $(PROGRAMS) models
	@sh scripts/sync_disk.sh $(DISK) $(DISK_STAMPS) $(DISK_FILES)

programs: $(PROGRAMS) $(DISK)
	@sh scripts/sync_disk.sh $(DISK) $(DISK_STAMPS) $(PROGRAMS) $(PROG_ASM)

# Copies the models onto the disk even if the disk has other files with
# those names: for when you want to replace an older Iris on purpose.
# MTOOLS_SKIP_CHECK: mtools otherwise rejects disks whose size isn't a
# multiple of the classic floppy geometry, which ours needn't be.
disk-models: $(DISK) models
	@for pair in $(DISK_MODELS); do \
	    file=$${pair%%:*}; name=$${pair##*:}; \
	    echo "mcopy $$file -> $(DISK):$$name"; \
	    MTOOLS_SKIP_CHECK=1 mcopy -o -i $(DISK) $$file ::$$name || exit 1; \
	done

# For the maintainer, after 'make chat-model': uploads Iris to the release
# and updates models/MODELS.sha256 (commit it afterwards).
publish-model: models/iris.bin models/iris_tok.bin
	sh scripts/publish_model.sh $(RELEASE_TAG) models iris.bin iris_tok.bin

# Iris, the chat model (docs/TRAINING.md). Trains on this computer with
# llama2.c's train.py (pinned below).
#   prepare_data.py    my_chats.txt + my_tools.txt -> work/train.txt, val.txt
#   train_tokenizer.py a 512-token SentencePiece vocabulary
#   tokenize_data.py   the text as token numbers for train.py
#   train.py           the model; writes out/model.bin (llama2.c format)
# It trains on the CPU: on an M1 Mac with PyTorch 2.5.1, training on the GPU
# (IRIS_DEVICE=mps) often gets stuck at a loss of about 4 instead of
# learning (TRAINING.md, "Why the CPU").
IRIS_DEVICE ?= cpu
TRAIN       := training
PYTHON      := $(TRAIN)/.venv/bin/python
LLAMA2C     := $(TRAIN)/llama2.c
LLAMA2C_GIT := https://github.com/karpathy/llama2.c.git
LLAMA2C_REV := 350e04fe35433e6d2941dce5a1f53308f87058eb
IRIS_VOCAB  := 512
IRIS_TRAIN_FLAGS := --vocab_source=custom --vocab_size=$(IRIS_VOCAB) \
    --dim=128 --n_layers=5 --n_heads=4 --n_kv_heads=4 --max_seq_len=256 \
    --batch_size=32 --gradient_accumulation_steps=1 --learning_rate=1e-3 \
    --max_iters=4000 --warmup_iters=200 --eval_interval=500 --eval_iters=50 \
    --dropout=0.1 --dtype=float32 --compile=False --always_save_checkpoint=False

chat-model: $(PYTHON) $(LLAMA2C)/train.py
	cd $(TRAIN) && .venv/bin/python prepare_data.py
	$(PYTHON) $(TRAIN)/train_tokenizer.py $(LLAMA2C) $(IRIS_VOCAB)
	$(PYTHON) $(TRAIN)/tokenize_data.py $(LLAMA2C) $(IRIS_VOCAB)
	cd $(LLAMA2C) && ../.venv/bin/python train.py $(IRIS_TRAIN_FLAGS) --out_dir=../out \
	    --device=$(IRIS_DEVICE)
	cp $(TRAIN)/out/model.bin models/iris.bin
	cp $(LLAMA2C)/data/tok$(IRIS_VOCAB).bin models/iris_tok.bin
	@echo "chat-model: models/iris.bin and models/iris_tok.bin (make disk-models copies them)"

$(PYTHON): $(TRAIN)/requirements.txt
	python3 -m venv $(TRAIN)/.venv
	$(PYTHON) -m pip install -r $(TRAIN)/requirements.txt
	touch $(PYTHON)

$(LLAMA2C)/train.py:
	git clone $(LLAMA2C_GIT) $(LLAMA2C)
	git -C $(LLAMA2C) checkout $(LLAMA2C_REV)

# Builds the assembler in src/asm/ for this computer (HOST_CC), assembles
# tests/asm/encoding.asm with it and with 'nasm -O0', and compares the bytes.
# tests/asm/host/ supplies lib/string.h from the host's C library.
HOST_CC  ?= cc
ASM_TEST := $(BUILD)/asm-test

asm-test:
	@mkdir -p $(ASM_TEST)
	$(HOST_CC) -std=c99 -Wall -Wextra -Itests/asm/host -Isrc \
	    -o $(ASM_TEST)/asm_host tests/asm/asm_host.c $(wildcard src/asm/*.c)
	$(ASM_TEST)/asm_host tests/asm/encoding.asm $(ASM_TEST)/planck.bin
	nasm -O0 -f bin -I tests/asm/ -o $(ASM_TEST)/nasm.bin tests/asm/nasm.asm
	cmp $(ASM_TEST)/planck.bin $(ASM_TEST)/nasm.bin
	@echo "asm-test: identical to NASM ($$(wc -c < $(ASM_TEST)/nasm.bin | tr -d ' ') bytes)"

# The Higgson compiler for this computer (docs/HIGGSON.md): the same core
# files as the kernel's, src/higgson/, with tools/higgson-host/ in place of
# platform_kernel.c.
HIGGSON_HOST := $(BUILD)/higgson-host/higgson
HIGGSON_CORE := $(filter-out src/higgson/platform_kernel.c,$(wildcard src/higgson/*.c))

higgson-host: $(HIGGSON_HOST)

$(HIGGSON_HOST): $(HIGGSON_CORE) $(wildcard src/higgson/*.h) $(wildcard tools/higgson-host/*.c)
	@mkdir -p $(dir $@)
	$(HOST_CC) -std=c99 -Wall -Wextra -O2 -Isrc -o $@ $(wildcard tools/higgson-host/*.c) $(HIGGSON_CORE)

# Fills in every Iris app template (src/apps/iris_templates.c) with every
# color and with tricky titles, assembles each with the built-in assembler
# and with NASM, and compares the bytes (docs/LLM.md, "Tools"). NASM's copy
# gets 'strict near' on conditional jumps, as in tests/asm: NASM would pick
# the short form, which the planckOS assembler never uses.
TEMPLATES_TEST := $(BUILD)/templates-test

templates-test:
	@mkdir -p $(TEMPLATES_TEST)
	$(HOST_CC) -std=c99 -Wall -Wextra -Itests/asm/host -Isrc \
	    -o $(TEMPLATES_TEST)/fill tests/iris/templates_test.c src/apps/iris_templates.c
	$(HOST_CC) -std=c99 -Wall -Wextra -Itests/asm/host -Isrc \
	    -o $(TEMPLATES_TEST)/asm_host tests/asm/asm_host.c $(wildcard src/asm/*.c)
	$(TEMPLATES_TEST)/fill $(TEMPLATES_TEST)
	@for src in $(TEMPLATES_TEST)/*.asm; do \
	    $(TEMPLATES_TEST)/asm_host $$src $${src%.asm}.planck || exit 1; \
	    sed -E 's/^( +j[a-ln-z][a-z]*) /\1 strict near /' $$src > $${src%.asm}.for-nasm; \
	    printf 'bits 32\norg 0x2000054\n%%include "%s"\n' $${src%.asm}.for-nasm > $${src%.asm}.wrap; \
	    nasm -O0 -f bin -o $${src%.asm}.nasm $${src%.asm}.wrap || exit 1; \
	    cmp $${src%.asm}.planck $${src%.asm}.nasm || exit 1; \
	done
	@echo "templates-test: $$(ls $(TEMPLATES_TEST)/*.asm | wc -l | tr -d ' ') filled-in templates identical to NASM"

# Runs Iris's model and tool choice on this computer (the kernel's own
# src/llm and iris_tools.c, with tests/iris/host/ standing in for the
# kernel): scores the 50 held-out requests in training/tool_test.txt and
# prints the sample conversations in tests/iris/sample_chats.txt.
IRIS_EVAL := $(BUILD)/iris-eval

iris-eval: models/iris.bin models/iris_tok.bin
	@mkdir -p $(IRIS_EVAL)
	$(HOST_CC) -std=gnu99 -O2 -Wall -Wextra -Itests/iris/host -Isrc -o $(IRIS_EVAL)/iris_eval \
	    tests/iris/iris_eval.c src/apps/iris_tools.c \
	    src/llm/model.c src/llm/tokenizer.c src/llm/sampler.c src/llm/llm.c src/llm/chat.c -lm
	cp models/iris.bin $(IRIS_EVAL)/IRIS.BIN
	cp models/iris_tok.bin $(IRIS_EVAL)/IRISTOK.BIN
	IRIS_DIR=$(IRIS_EVAL) $(IRIS_EVAL)/iris_eval chat tests/iris/sample_chats.txt 2>/dev/null
	IRIS_DIR=$(IRIS_EVAL) $(IRIS_EVAL)/iris_eval tools training/tool_test.txt 2>/dev/null

$(BUILD)/programs/%.elf: programs/%.c programs/planck.h programs/program.ld
	@mkdir -p $(dir $@)
	$(CC) $(PROG_CFLAGS) $(PROG_LDFLAGS) -o $@ $<

clean:
	rm -rf $(BUILD)

-include $(DEPS)
