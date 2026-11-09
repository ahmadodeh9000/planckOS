# planckOS

planckOS is a hobby 32-bit x86 operating system written in C and NASM by Ahmad Odeh.
Its kernel is called **Iris**. It boots with GRUB (Multiboot 1) and runs in QEMU.

## History

The original text-mode version of planckOS (VGA text console and a command shell)
is preserved as the tag `v1.0-text` and the branch `legacy/text-shell`.
To check it out:

```sh
git checkout v1.0-text            # read-only snapshot (detached HEAD)
git checkout legacy/text-shell    # or as a branch you can build on
```
