#!/bin/sh
# publish_model.sh - uploads Iris's model as assets of a GitHub release.
#
# Usage: publish_model.sh <tag> <models dir> <file> ...
#
# For the maintainer, after retraining (`make chat-model`): uploads the
# files to the release <tag> with the gh CLI (creating the release if it
# doesn't exist yet, replacing the assets if it does), then writes their
# SHA-256 into <models dir>/MODELS.sha256, which has to be committed so
# `make models` downloads and accepts the new files.

set -eu
tag=$1
dir=$2
shift 2
sums="$dir/MODELS.sha256"

sha256() {
    if command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$1" | cut -d' ' -f1
    else
        sha256sum "$1" | cut -d' ' -f1
    fi
}

if ! command -v gh >/dev/null 2>&1 || ! gh auth status >/dev/null 2>&1; then
    echo "publish-model: needs the GitHub CLI, logged in (brew install gh; gh auth login)"
    exit 1
fi
paths=""
for name in "$@"; do
    [ -f "$dir/$name" ] || { echo "publish-model: $dir/$name is missing"; exit 1; }
    paths="$paths $dir/$name"
done

if gh release view "$tag" >/dev/null 2>&1; then
    gh release upload "$tag" $paths --clobber
else
    gh release create "$tag" $paths --title "Iris model ($tag)" \
        --notes "Iris, planckOS's assistant: the trained model and its tokenizer. \
'make models' (or 'make run') downloads them and checks them against \
models/MODELS.sha256. License: GNU GPL v3, like planckOS (see models/README.md)."
fi

# Replace the published files' lines in MODELS.sha256, keep the others.
updated=$(mktemp)
for name in "$@"; do
    grep -v " $name\$" "$sums" > "$updated" || true
    echo "$(sha256 "$dir/$name")  $name" >> "$updated"
    cp "$updated" "$sums"
done
rm -f "$updated"
echo "publish-model: uploaded to release $tag; commit $sums so 'make models' uses them"
