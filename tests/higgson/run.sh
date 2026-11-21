#!/bin/sh
# run.sh - 'make higgson-test': the Higgson compiler, checked on this
# computer with the same core files the kernel uses.
#
# Usage: run.sh <higgson> <asm_host> <out dir> <examples...> -- <broken...>
#
# Every example must compile, and its assembly must assemble with the
# planckOS assembler (built for this computer). Every broken program must
# fail with exactly the error its first line names:
#     // expect: line 4: expected ';' after statement

higgson=$1
asm=$2
out=$3
shift 3
failed=0
passed=0

while [ "$1" != "--" ]; do
    name=$(basename "$1" .hgs)
    if "$higgson" "$1" "$out/$name.asm" && "$asm" "$out/$name.asm" "$out/$name.elf" >/dev/null; then
        passed=$((passed + 1))
    else
        echo "higgson-test: $1 should compile and assemble"
        failed=$((failed + 1))
    fi
    shift
done
shift

for program in "$@"; do
    expected="higgson: $program $(sed -n '1s|^// expect: ||p' "$program")"
    actual=$("$higgson" "$program" "$out/broken.asm" 2>&1)
    if [ $? -ne 0 ] && [ "$actual" = "$expected" ]; then
        passed=$((passed + 1))
    else
        echo "higgson-test: $program"
        echo "    expected: $expected"
        echo "    got:      $actual"
        failed=$((failed + 1))
    fi
done

echo "higgson-test: $passed passed, $failed failed"
[ $failed -eq 0 ]
