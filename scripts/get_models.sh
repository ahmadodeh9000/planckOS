#!/bin/sh
# get_models.sh - makes sure every model file is in models/ and correct.
#
# Usage: get_models.sh <models dir> <file>=<url> ...
#
# For each file: if it's there and its SHA-256 matches models/MODELS.sha256,
# nothing happens. Otherwise it's downloaded from <url> (to a .part file
# first, so an interrupted download never looks finished) and checked
# again. A wrong checksum or a failed download stops with a clear message.

set -u
dir=$1
shift
sums="$dir/MODELS.sha256"

sha256() {
    if command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$1" | cut -d' ' -f1
    else
        sha256sum "$1" | cut -d' ' -f1
    fi
}

for pair in "$@"; do
    name=${pair%%=*}
    url=${pair#*=}
    expected=$(grep " $name\$" "$sums" | cut -d' ' -f1)
    if [ -z "$expected" ]; then
        echo "make models: $name has no checksum in $sums"
        exit 1
    fi
    if [ -f "$dir/$name" ] && [ "$(sha256 "$dir/$name")" = "$expected" ]; then
        continue
    fi
    echo "make models: downloading $name"
    if ! curl -fL --retry 3 --progress-bar -o "$dir/$name.part" "$url"; then
        echo "make models: couldn't download $name from"
        echo "    $url"
        echo "Are you online? Run 'make models' again when you are."
        exit 1
    fi
    actual=$(sha256 "$dir/$name.part")
    if [ "$actual" != "$expected" ]; then
        echo "make models: $name has the wrong checksum, so it wasn't kept"
        echo "    expected $expected"
        echo "    got      $actual"
        echo "The download may be damaged; run 'make models' again to retry."
        exit 1
    fi
    mv "$dir/$name.part" "$dir/$name"
    echo "make models: $name OK"
done
