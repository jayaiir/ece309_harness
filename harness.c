/* harness.c - a tiny interactive command loop with a calculator tool.
 * Reads a line at a time from standard input and reacts to it.
 * Only the C standard library is used. */

#include <stdio.h>   /* printf, fgets, stdin, stdout */
#include <string.h>  /* strcmp, strncmp, strcspn, strstr, strlen */
#include <stdlib.h>  /* strtod */

/* Longest line we are willing to read, including the '\0' terminator. */
#define MAX_LINE 256

/* ------------------------------------------------------------------
 * Expression parser
 *
 * This is a recursive descent parser: one small function per level of
 * operator precedence, each calling the level below it. Because
 * addition calls multiplication (and not the other way round),
 * multiplication binds more tightly to its operands automatically.
 *
 *   expression := term (('+' | '-') term)*
 *   term       := factor (('*' | '/' | '%') factor)*
 *   factor     := ('+' | '-') factor | number | '(' expression ')'
 * ------------------------------------------------------------------ */

/* Everything the parser needs to know while it walks the text. */
typedef struct {
    const char *p; /* the character we are looking at right now */
    int error;     /* 0 = fine, 1 = malformed input, 2 = divide by zero */
} Parser;

/* Declared up front because parse_factor needs to call it for the
 * contents of parentheses, but it is defined further down. */
static double parse_expression(Parser *ps);

/* Step over spaces and tabs so no other function has to think about
 * whitespace. */
static void skip_spaces(Parser *ps)
{
    while (*ps->p == ' ' || *ps->p == '\t')
        ps->p++;
}

/* The innermost level: a number, a parenthesised group, or a sign
 * attached to one of those. */
static double parse_factor(Parser *ps)
{
    double value;
    char *end;

    skip_spaces(ps);

    /* A leading sign applies to whatever factor follows it. Recursing
     * rather than looping means even "--5" reads correctly as 5. */
    if (*ps->p == '-') {
        ps->p++;
        return -parse_factor(ps);
    }
    if (*ps->p == '+') {
        ps->p++;
        return parse_factor(ps);
    }

    /* "( ... )" - parse what is inside, then insist on a closing
     * bracket. A missing ')' is an error rather than something we
     * quietly forgive, so "(2 + 3" is rejected. */
    if (*ps->p == '(') {
        ps->p++;
        value = parse_expression(ps);
        skip_spaces(ps);
        if (*ps->p == ')')
            ps->p++;
        else
            ps->error = 1;
        return value;
    }

    /* Anything else has to be a number. strtod points 'end' at the
     * first character it could not use; if that is still our starting
     * position then there was no number here at all. */
    value = strtod(ps->p, &end);
    if (end == ps->p) {
        ps->error = 1;
        return 0.0;
    }
    ps->p = end;
    return value;
}

/* The middle level: factors joined by * / or %, consumed left to right. */
static double parse_term(Parser *ps)
{
    double left = parse_factor(ps);
    if (ps->error)
        return 0.0;

    for (;;) {
        char op;
        double right;

        skip_spaces(ps);
        op = *ps->p;

        /* Not an operator we handle, so this term is finished and the
         * character belongs to whoever called us. */
        if (op != '*' && op != '/' && op != '%')
            return left;

        ps->p++;
        right = parse_factor(ps);
        if (ps->error)
            return 0.0;

        if (op == '*') {
            left = left * right;
        } else {
            /* Both / and % are meaningless when the divisor is zero,
             * and dividing here would produce inf or nan instead of a
             * message the user can act on. */
            if (right == 0.0) {
                ps->error = 2;
                return 0.0;
            }
            if (op == '/') {
                left = left / right;
            } else {
                /* Remainder, done by hand. The obvious tool is fmod
                 * from math.h, but that can force the program to be
                 * linked with -lm on some systems, and the whole point
                 * is that plain "gcc harness.c -o harness" works.
                 * Truncating the quotient toward zero gives the same
                 * answer fmod would for calculator-sized numbers. */
                double quotient = left / right;
                left = left - right * (double)(long long)quotient;
            }
        }
    }
}

/* The outermost level: terms joined by + or -, consumed left to right. */
static double parse_expression(Parser *ps)
{
    double left = parse_term(ps);
    if (ps->error)
        return 0.0;

    for (;;) {
        char op;
        double right;

        skip_spaces(ps);
        op = *ps->p;

        if (op != '+' && op != '-')
            return left;

        ps->p++;
        right = parse_term(ps);
        if (ps->error)
            return 0.0;

        left = (op == '+') ? left + right : left - right;
    }
}

/* ------------------------------------------------------------------
 * Tools
 *
 * A tool is a name the user types plus the function that handles the
 * rest of the line. Keeping them in a table means adding another tool
 * is one function and one row, with no changes to the main loop.
 * ------------------------------------------------------------------ */

/* Handles "calc <expression>". 'args' is everything after the word
 * "calc", so it usually starts with a space. */
static void tool_calc(const char *args)
{
    Parser ps;
    double result;

    ps.p = args;
    ps.error = 0;

    skip_spaces(&ps);

    /* Bare "calc" is a usage slip rather than a broken expression, so
     * it earns a more helpful message. */
    if (*ps.p == '\0') {
        printf("calc: usage: calc <expression>   (example: calc 2 + 3 * 4)\n");
        return;
    }

    result = parse_expression(&ps);

    /* Division by zero gets its own wording so the user can tell it
     * apart from a typo in the expression. */
    if (ps.error == 2) {
        printf("calc: cannot divide by zero\n");
        return;
    }

    /* Two ways left to fail: the parser gave up, or it succeeded but
     * left characters behind. The second case catches input like "2 + "
     * and "2 ) 3", where a prefix parsed cleanly but the rest is
     * garbage. */
    skip_spaces(&ps);
    if (ps.error || *ps.p != '\0') {
        printf("calc: cannot evaluate that expression\n");
        return;
    }

    /* %g trims trailing zeros, so 14 prints as "14" rather than
     * "14.000000", while 2.5 still prints as "2.5". */
    printf("%.10g\n", result);
}

/* One row per tool: the word to match and the function to run. */
typedef struct {
    const char *name;
    void (*run)(const char *args);
} Tool;

static const Tool tools[] = {
    { "calc", tool_calc }
};

/* How many rows the table has, worked out by the compiler so it stays
 * correct when tools are added. */
#define TOOL_COUNT (sizeof tools / sizeof tools[0])

/* If the line starts with a tool's name, run that tool and report 1 so
 * the caller knows the line is already dealt with. Otherwise report 0. */
static int run_tool_if_matched(const char *line)
{
    size_t i;

    for (i = 0; i < TOOL_COUNT; i++) {
        size_t len = strlen(tools[i].name);

        /* The character after the name must be a space or the end of
         * the line. Without that check "calculate the tax" would be
         * mistaken for a call to "calc". */
        if (strncmp(line, tools[i].name, len) == 0 &&
            (line[len] == '\0' || line[len] == ' ')) {
            tools[i].run(line + len);
            return 1;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------
 * Main loop
 * ------------------------------------------------------------------ */

int main(void)
{
    char line[MAX_LINE]; /* storage for the line the user typed */

    /* Loop forever; the only way out is the "exit" command or end of input. */
    while (1) {
        /* Show the prompt. printf does not add a newline here, so the
         * cursor stays on the same line as the "> ". */
        printf("> ");

        /* stdout is often line-buffered (or fully buffered when piped), so
         * force the prompt out to the terminal before we block on input. */
        fflush(stdout);

        /* Read at most MAX_LINE-1 characters into line, stopping at newline.
         * fgets returns NULL on end-of-file or a read error, which happens
         * when input comes from a pipe that has run dry or the user presses
         * Ctrl+D. Leave the loop cleanly in that case. */
        if (fgets(line, sizeof line, stdin) == NULL) {
            printf("\n"); /* keep the shell prompt on a fresh line */
            break;
        }

        /* fgets keeps the trailing newline. strcspn returns the index of the
         * first '\n' (or of the '\0' if there is none), so writing '\0' there
         * trims the newline without touching anything else. */
        line[strcspn(line, "\n")] = '\0';

        /* strcmp returns 0 only when the two strings match exactly, so this
         * matches "exit" and not "exit now" or "Exit". */
        if (strcmp(line, "exit") == 0) {
            printf("Goodbye!\n");
            break; /* leave the while loop and fall through to return 0 */
        }

        /* Tools get first refusal, ahead of the greeting and the echo. */
        if (run_tool_if_matched(line))
            continue;

        /* strstr returns a pointer to the first occurrence of "hello"
         * anywhere in the line, or NULL if it is not present. */
        if (strstr(line, "hello") != NULL) {
            printf("Hello there! Nice to meet you.\n");
            continue; /* handled, so skip the echo below */
        }

        /* Anything else: repeat the input back exactly as it was typed. */
        printf("%s\n", line);
    }

    /* Returning 0 from main exits the program with a success status. */
    return 0;
}
