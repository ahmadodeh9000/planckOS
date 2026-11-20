#!/bin/sh
# sync_disk.sh - copies planckOS's own files onto the disk image, safely.
#
# Usage: sync_disk.sh <disk image> <stamp dir> <file>[:<NAME>] ...
#
# Copies each <file> onto the disk as <NAME> (an 8.3 name; without one, the
# file's own name in capitals: hello.asm -> HELLO.ASM), but only what
# changed, and never over a file of yours:
#
#   - the stamp dir remembers the SHA-256 of what was last copied for each
#     NAME. If the file still has that checksum, there's nothing to do and
#     the disk isn't even opened.
#   - if NAME isn't on the disk, it's copied.
#   - if NAME is on the disk with exactly what was last copied, it's
#     replaced with the new version.
#   - if NAME is on the disk with anything else (you wrote it, or edited
#     it in planckOS), it's left alone and a note says so.
#
# Each disk image gets its own stamps (by its FAT volume serial number), so
# a new disk.img is filled again from scratch.

set -u
disk=$1
stamps=$2
shift 2
export MTOOLS_SKIP_CHECK=1      # our disk's size isn't a floppy geometry

sha256() {
    if command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$1" | cut -d' ' -f1
    else
        sha256sum "$1" | cut -d' ' -f1
    fi
}

# The FAT volume serial number: 4 bytes at offset 39 of the boot sector.
serial=$(od -A n -t x1 -j 39 -N 4 "$disk" | tr -d ' \n')
stamps="$stamps/$serial"
mkdir -p "$stamps"
copy=$(mktemp)
trap 'rm -f "$copy"' EXIT

for pair in "$@"; do
    file=${pair%%:*}
    name=${pair##*:}
    [ "$name" = "$pair" ] && name=$(basename "$file" | tr a-z A-Z)
    [ -f "$file" ] || continue
    new=$(sha256 "$file")
    last=$(cat "$stamps/$name" 2>/dev/null)
    [ "$new" = "$last" ] && continue

    if mcopy -n -i "$disk" "::$name" "$copy" 2>/dev/null; then
        on_disk=$(sha256 "$copy")
        if [ "$on_disk" != "$new" ] && [ "$on_disk" != "$last" ]; then
            echo "disk: $name on $disk isn't one planckOS put there, leaving it alone"
            echo "      (to replace the models on purpose: make disk-models)"
            continue
        fi
    else
        on_disk=""
    fi
    if [ "$on_disk" != "$new" ]; then
        mcopy -o -i "$disk" "$file" "::$name" || exit 1
        echo "disk: copied $file -> $name"
    fi
    echo "$new" > "$stamps/$name"
done
