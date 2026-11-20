#!/bin/sh
# check_tools.sh - stops `make run` early if a tool it needs is missing.
#
# Checks for everything building, running and setting up planckOS uses,
# and prints exactly what to install (Homebrew on macOS, apt on Linux and
# WSL2), so nothing is half done when a tool turns out to be missing.

missing=""          # names of the missing tools, for the message
brew_packages=""
apt_packages=""

need() {            # need <command> <brew package> <apt package>
    if ! command -v "$1" >/dev/null 2>&1; then
        missing="$missing $1"
        brew_packages="$brew_packages $2"
        apt_packages="$apt_packages $3"
    fi
}

# A compiler for 32-bit x86: the cross compiler, or gcc that can do -m32.
compiler_ok() {
    command -v i686-elf-gcc >/dev/null 2>&1 && return 0
    command -v gcc >/dev/null 2>&1 &&
        echo 'int x;' | gcc -m32 -c -x c - -o /dev/null >/dev/null 2>&1
}

if ! compiler_ok; then
    missing="$missing i686-elf-gcc(or-gcc-m32)"
    brew_packages="$brew_packages i686-elf-gcc"
    apt_packages="$apt_packages gcc gcc-multilib"
fi
if command -v i686-elf-gcc >/dev/null 2>&1; then
    need i686-elf-grub-mkrescue i686-elf-grub grub-pc-bin
else
    need grub-mkrescue i686-elf-grub "grub-pc-bin grub-common"
fi
need nasm nasm nasm
need xorriso xorriso xorriso
need qemu-system-i386 qemu qemu-system-x86
need mcopy mtools mtools
need curl curl curl
if ! command -v shasum >/dev/null 2>&1 && ! command -v sha256sum >/dev/null 2>&1; then
    missing="$missing sha256sum"
    apt_packages="$apt_packages coreutils"
fi
if [ "$(uname)" != Darwin ]; then
    need mkfs.fat dosfstools dosfstools
fi

if [ -z "$missing" ]; then
    exit 0
fi
echo "planckOS needs some tools that aren't installed:$missing"
echo
if [ "$(uname)" = Darwin ]; then
    echo "Install them with Homebrew (https://brew.sh):"
    echo "    brew install$brew_packages"
else
    echo "Install them with (Debian, Ubuntu, and Ubuntu on WSL2):"
    echo "    sudo apt install$apt_packages"
fi
echo
echo "Then run the same make command again."
exit 1
