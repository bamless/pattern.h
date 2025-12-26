# pattern.h

A **single-header C library** that implements **Lua-style pattern matching**, including captures, character classes, repetition operators, balanced matches, frontier patterns, and detailed error reporting.


This is **not regex** — it follows Lua's lightweight pattern syntax, making it small, fast, and easy to embed.
An overview of pattern matching is provided [below](#pattern-syntax-lua-compatible). For more information on Lua's patterns refer to the official [documentation](https://www.lua.org/pil/20.2.html).

---

### Features

- Lua-compatible pattern syntax
- Header-only (drop-in)
- Compiles cleanly with a C++ compiler
- Works on binary data or C strings
- Captures and back-references
- Greedy and lazy repetition
- Anchors (`^`, `$`)
- Balanced matches (`%b`)
- Frontier patterns (`%f`)
- Detailed error diagnostics with source location

---

### Design Notes

- No dynamic allocations
- Recursive backtracking matcher
- Designed for simplicity and embeddability
- Behavior closely follows Lua's `string.match`

---

### Limitations

- Not a full regex engine
- No alternation (`|`)
- No lookahead/lookbehind

## Getting Started

### Installation

Just copy `pattern.h` into your project.

In **one** C file, define `PATTERN_IMPLEMENTATION` **before** including it:

```c
#define PATTERN_IMPLEMENTATION
#include "pattern.h"
```

In other files, include it normally:

```c
#include "pattern.h"
```

## Basic Usage

```c
Pattern_State ps;

Pattern_Status status = pattern_match_cstr(&ps, "hello world", "h(ello)");
if(status == PATTERN_ERROR) {
    pattern_print_error(stderr, &ps);
    return 1;
}
if(status == PATTERN_NO_MATCH) {
    printf("No match");
    return 0;
}

printf("Matched!\n");
printf("Full match: %.*s\n", (int)ps.captures[0].size, ps.captures[0].data);
printf("Capture 1: %.*s\n", (int)ps.captures[1].size, ps.captures[1].data);
```

> NOTE: the full match is always stored at capture index `0`.  
> Conceptually, it is as the pattern is always surrounded with a top-level capture.

## Pattern Syntax (Lua-Compatible)

### Character Matching

| Pattern | Meaning |
|--------|--------|
| `.` | Any character |
| `%a` | Letters |
| `%c` | Control characters |
| `%d` | Digits |
| `%l` | Lowercase character |
| `%p` | Punctuation |
| `%s` | Whitespace |
| `%u` | Uppercase character |
| `%w` | Alphanumeric |
| `%x` | Hexadecimal |
| `%g` | Printable character and not whitespace |
| `%z` | `\0` |
| Uppercase classes | Negated (`%A`, `%D`, etc.) |

### Character Classes

```lua
[abc]       -- a, b, or c
[a-z]       -- range
[^0-9]      -- negated class
[%a%d_]     -- escaped classes allowed
```

### Repetition Operators

| Operator | Meaning |
|----------|--------|
| `?` | 0 or 1 (optional) |
| `*` | 0 or more (greedy) |
| `+` | 1 or more (greedy) |
| `-` | 0 or more (lazy) |

### Anchors

| Pattern | Meaning |
|--------|--------|
| `^` | Start of string |
| `$` | End of string |

### Captures

```lua
(abc)     -- capture substring
()        -- position capture
```

- Capture `0` is always the **entire match**
- Maximum captures: by default **31** (30 + 1 for the top-level match), can be overridden by defining `PATTERN_MAX_CAPTURES` before including the library.

#### Backreferences

```lua
(%a+)%1    -- matches repeated word
```

### Balanced Matches

```lua
%bxy    -- matches substring starting with x, ending with y, with balanced occurrences
```

Examples:
- `%b()` matches balanced parentheses: `"(a(b)c)"` → `"(a(b)c)"`
- `%b[]` matches balanced brackets: `"[a[b]c]"` → `"[a[b]c]"`
- `%b<>` matches balanced angle brackets: `"<a<b>c>"` → `"<a<b>c>"`

The balanced match starts at the first occurrence of `x` and continues until it finds a matching `y` where all nested `x`/`y` pairs are balanced.

### Frontier Patterns

```lua
%f[set]    -- matches empty string at boundary where next char is in set and previous is not
```

Examples:
- `%f[%w]` matches the start of a word (word boundary)
- `%f[%W]` matches the end of a word
- `%f[%a]hello%f[%A]` matches "hello" as a complete word
- `%f[%d]` matches positions just before a digit

The frontier pattern matches a zero-width position (like `()` position captures) where:
- The previous character is NOT in the specified set
- The current character IS in the specified set

This is useful for matching word boundaries and other transitions between character classes.

## Binary-Safe Matching

```c
pattern_match(&ps, "cantami\0o\0diva", 14, pattern);
```

## Starting Position

```c
pattern_match_ex(&ps, data, len, pattern, start_pos);
```

- Negative values start from the end (Lua-style)

## Error Handling

```c
Pattern_Status status = ...;
if(status == PATTERN_ERROR) {
    pattern_print_error(stderr, &ps);
}
```

Example output:

```bash
column:5: unclosed character class
[a-z
^
```

### Error Types

- `PATTERN_ERR_MAX_CAPTURES`
- `PATTERN_ERR_UNEXPECTED_CAPTURE_CLOSE`
- `PATTERN_ERR_UNCLOSED_CAPTURE`
- `PATTERN_ERR_INVALID_CAPTURE_IDX`
- `PATTERN_ERR_INCOMPLETE_ESCAPE`
- `PATTERN_ERR_UNCLOSED_CLASS`
- `PATTERN_ERR_INVALID_BALANCED_PATTERN`
- `PATTERN_ERR_UNCLOSED_FRONTIER_PATTERN`

## Utility Functions

```c
// Returns true if `idx` is a position-only capture (i.e. `()`)
bool pattern_is_position_capture(const Pattern_State* ps, int idx);
// Gets the offset from the start of the string where the capture `idx` starts
size_t pattern_get_capture_pos(const Pattern_State* ps, int idx);
// Returns a human readable string describing the error
const char* pattern_strerror(Pattern_Error err);
// Prints an error in human readable form along with the error location in the pattern
void pattern_print_error(FILE* stream, const Pattern_State* ps);
```

# Tests

A test suite is provided in 'test/' folder. To run them:
```bash
make test
```
