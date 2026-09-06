#!/usr/bin/env bash
# test.sh - compile harness.c and feed it scripted input, no typing required.

set -e  # stop immediately if the compile or the program fails

SRC="harness.c"
BIN="./harness"

echo "== Compiling $SRC =="
gcc "$SRC" -o harness
echo "Compiled to $BIN"
echo

echo "== Running with scripted input =="
# Each line below becomes one line of stdin for the program:
#   hello world        -> the hardcoded greeting
#   this is a test...  -> echoed back verbatim
#   calc 2 + 3 * 4     -> 14, proving * beats + regardless of order
#   calc (2 + 3) * 4   -> 20, proving parentheses override precedence
#   calc 10 / 4        -> 2.5, proving results are not truncated to ints
#   calc 10 % 3        -> 1, the remainder operator
#   calc 5 / 0         -> a divide-by-zero message, not inf or nan
#   calc 2 +           -> rejected as malformed
#   exit               -> goodbye message, loop ends
OUTPUT=$(printf '%s\n' \
    'hello world' \
    'this is a test string' \
    'calc 2 + 3 * 4' \
    'calc (2 + 3) * 4' \
    'calc 10 / 4' \
    'calc 10 % 3' \
    'calc 5 / 0' \
    'calc 2 +' \
    'exit' | "$BIN")

echo "$OUTPUT"
echo

echo "== Checking output =="
FAILED=0

# Report PASS or FAIL depending on whether the captured output contains
# a line matching the given pattern.
check() {
    description="$1"
    pattern="$2"
    if printf '%s\n' "$OUTPUT" | grep -q "$pattern"; then
        echo "PASS: $description"
    else
        echo "FAIL: $description"
        FAILED=1
    fi
}

# Results are preceded by the "> " prompt on the same line, because piped
# input is never echoed back, so the patterns below expect that prefix.
check "greeting fired for 'hello world'"        "Hello there!"
check "test string echoed verbatim"             "this is a test string"
check "precedence honoured (2 + 3 * 4 = 14)"    '^> 14$'
check "parentheses honoured ((2 + 3) * 4 = 20)" '^> 20$'
check "decimal result kept (10 / 4 = 2.5)"      '^> 2.5$'
check "remainder works (10 % 3 = 1)"            '^> 1$'
check "divide by zero refused"                  "cannot divide by zero"
check "malformed expression refused"            "cannot evaluate"
check "'exit' terminated the program"           "Goodbye!"

echo
if [ "$FAILED" -eq 0 ]; then
    echo "All tests passed."
else
    echo "Some tests failed."
fi

exit "$FAILED"
