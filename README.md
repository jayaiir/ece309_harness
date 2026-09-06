# harness

A small interactive command-line loop written in C, with a built-in calculator
tool. It reads a line at a time from standard input, decides what kind of input
it is, and responds. The whole program is a single file with no dependencies
beyond the C standard library.

The calculator exists because arithmetic is the classic example of something a
language model should hand off to real code rather than attempt itself, so the
loop is set up to dispatch that work to a tool.

## Requirements

- `gcc` (or any C compiler)
- `bash` and `grep`, to run the test script

## Build

```bash
gcc harness.c -o harness
```

## Run

```bash
./harness
```

## Commands

| Input | Behaviour |
| --- | --- |
| `exit` | Prints a goodbye message and terminates. Must be exactly `exit`. |
| `calc <expression>` | Evaluates the expression and prints the result. |
| any line containing `hello` | Prints a fixed greeting. |
| anything else | Echoes the line back verbatim. |

Pressing `Ctrl+D`, or reaching the end of piped input, also exits cleanly.

### Example session

```
> calc 2 + 3 * 4
14
> calc (2 + 3) * 4
20
> calc 10 / 4
2.5
> hello there
Hello there! Nice to meet you.
> some other text
some other text
> exit
Goodbye!
```

## The calculator tool

Supports `+`, `-`, `*`, `/`, `%`, parentheses, decimal numbers, and unary
signs. Operator precedence is respected, which is why `2 + 3 * 4` is 14 and
`(2 + 3) * 4` is 20.

It is implemented as a recursive descent parser: one function per level of
precedence, each calling the level below it. Since the addition level calls the
multiplication level, multiplication ends up binding more tightly to its
operands without any explicit precedence table.

Bad input is reported rather than guessed at. Division by zero, a malformed
expression such as `2 +`, and leftover characters such as `2 ) 3` each produce
a message instead of `inf`, `nan`, or a partial answer.

## Tests

```bash
chmod +x test.sh
./test.sh
```

The script compiles `harness.c`, pipes a fixed list of inputs into the binary,
and checks the output for each expected behaviour. It needs no interaction and
exits with status `0` when everything passes and `1` otherwise, so it can be
dropped into CI as-is.

Covered: the greeting, the echo, operator precedence, parentheses, decimal
results, remainder, division by zero, a malformed expression, and `exit`.

## Adding another tool

Tools live in a table in `harness.c`:

```c
static const Tool tools[] = {
    { "calc", tool_calc }
};
```

To add one, write a function taking `const char *args` (everything the user
typed after the tool's name) and add a row to the table. The main loop needs no
changes. A tool name only matches when followed by a space or the end of the
line, so `calculate the total` is not mistaken for a call to `calc`.

## Files

| File | Purpose |
| --- | --- |
| `harness.c` | The entire program. |
| `test.sh` | Compiles and tests it automatically. |

## Note on line endings

`test.sh` must have Unix (LF) line endings. If it is saved with Windows (CRLF)
endings, the shell reports `env: 'bash\r': No such file or directory`. To fix:

```bash
sed -i 's/\r$//' test.sh harness.c
```
