/**
 * pattern.h v1.1.0 - Lua's pattern matching in C
 *
 * Single header library implementing Lua's pattern matching
 *
 * Features:
 *     - Lua-compatible pattern syntax
 *     - Header-only (drop-in)
 *     - Works on binary data or C strings
 *     - Captures and back-references
 *     - Greedy and lazy repetition
 *     - Anchors (`^`, `$`)
 *     - Balanced matches (`%b`)
 *     - Frontier patterns (`%f`)
 *     - Detailed error diagnostics with source location
 *
 * Limitations:
 *     - Not a full regex engine
 *     - No alternation (`|`)
 *     - No lookahead/lookbehind
 *
 * Pattern Syntax:
 *
 * Character Matching:
 * `.`  - Any character
 * `%a` - Letters
 * `%c` - Control characters
 * `%d` - Digits
 * `%l` - Lowercase character
 * `%p` - Punctuation
 * `%s` - Whitespace
 * `%u` - Uppercase character
 * `%w` - Alphanumeric
 * `%x` - Hexadecimal
 * `%g` - Printable character and not whitespace
 * `%z` - `\0`
 * Uppercase classes - Negated (`%A`, `%D`, etc.)
 *
 * Character Classes:
 * [abc]   - a, b, or c
 * [a-z]   - range
 * [^0-9]  - negated class
 * [%a%d_] - escaped classes allowed
 *
 * Repetition Operators:
 * `?` - 0 or 1 (optional)
 * `*` - 0 or more (greedy)
 * `+` - 1 or more (greedy)
 * `-` - 0 or more (lazy)
 *
 * Anchors:
 * `^` - Start of string
 * `$` - End of string
 *
 * Captures:
 * (abc) - capture substring
 * ()    - position capture
 *
 * - Capture `0` is always the **entire match**
 * - Maximum captures: by default **31** (30 + 1 for the top-level match), can be overridden by
 *   defining `PATTERN_MAX_CAPTURES` before including the library.
 *
 * Backreferences:
 * (%a+)%1 - matches repeated word
 *
 * Balanced Matches:
 * %bxy - matches substring starting with x, ending with y, with balanced occurrences
 *        Example: %b() matches balanced parentheses like "(a(b)c)"
 *
 * Frontier Patterns:
 * %f[set] - matches empty string at any position where next char is in set and previous is not
 *           Example: %f[%w] matches word boundaries
 *
 *  Changelog:
 *  1.1.0:
 *    Added support for balanced matches and frontier patterns
 */
#ifndef PATTERN_H_
#define PATTERN_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#ifndef PATTERN_MAX_CAPTURES
#define PATTERN_MAX_CAPTURES 31
#endif
#define PATTERN_ESCAPE             '%'
#define PATTERN_CAPTURE_UNFINISHED -1
#define PATTERN_CAPTURE_POSITION   -2

typedef struct {
    ptrdiff_t size;
    const char* data;
} Pattern_Substring;

typedef enum {
    PATTERN_ERR_NONE = 0,
    PATTERN_ERR_MAX_CAPTURES,
    PATTERN_ERR_UNEXPECTED_CAPTURE_CLOSE,
    PATTERN_ERR_UNCLOSED_CAPTURE,
    PATTERN_ERR_INVALID_CAPTURE_IDX,
    PATTERN_ERR_INCOMPLETE_ESCAPE,
    PATTERN_ERR_UNCLOSED_CLASS,
    PATTERN_ERR_INVALID_BALANCED_PATTERN,
    PATTERN_ERR_UNCLOSED_FRONTIER_PATTERN,
} Pattern_Error;

typedef enum {
    PATTERN_NO_MATCH = 0,
    PATTERN_MATCH,
    PATTERN_ERROR,
} Pattern_Status;

typedef struct {
    Pattern_Error error;
    size_t error_loc;
    Pattern_Substring data;
    const char* pattern_base;
    int capture_count;
    Pattern_Substring captures[PATTERN_MAX_CAPTURES];
} Pattern_State;

// Try to match some data (or cstring) with `pattern` starting from `starting_pos` in the data.
// If `starting_pos` is negative, it will be interpreted as an offset from the end of the data.
// Returns the match status (PATTERN_MATCH, PATTERN_NO_MATCH, or PATTERN_ERROR).
Pattern_Status pattern_match(Pattern_State* ps, const void* data, size_t len, const char* pattern);
Pattern_Status pattern_match_ex(Pattern_State* ps, const void* data, size_t len,
                                const char* pattern, ptrdiff_t starting_pos);
Pattern_Status pattern_match_cstr(Pattern_State* ps, const char* str, const char* pattern);
Pattern_Status pattern_match_cstr_ex(Pattern_State* ps, const char* str, const char* pattern,
                                     ptrdiff_t starting_pos);

// Returns true if capture `idx` is a position-only capture (i.e. `()`)
bool pattern_is_position_capture(const Pattern_State* ps, int idx);
// Gets the offset from the start of the string where the capture `idx` starts
size_t pattern_get_capture_pos(const Pattern_State* ps, int idx);

// Returns a human readable string describing the error
const char* pattern_strerror(Pattern_Error err);
// Prints an error in human readable form along with the error location in the pattern
void pattern_print_error(FILE* stream, const Pattern_State* ps);

#ifdef PATTERN_IMPLEMENTATION

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

static void pattern_init(Pattern_State* ps, const void* data, size_t len, const char* pattern) {
    ps->error = PATTERN_ERR_NONE;
    ps->error_loc = 0;
    ps->data.data = (const char*)data;
    ps->data.size = len;
    ps->pattern_base = pattern;
    ps->capture_count = 1;
    ps->captures[0].data = (const char*)data;
    ps->captures[0].size = PATTERN_CAPTURE_UNFINISHED;
}

static void pattern_set_error(Pattern_State* ps, Pattern_Error err, size_t err_loc) {
    if(ps->error) return;
    ps->error = err;
    ps->error_loc = err_loc;
}

static bool pattern_is_at_end(const Pattern_State* ps, const char* string_ptr) {
    return string_ptr >= ps->data.data + ps->data.size;
}

static bool pattern_is_at_pattern_end(const char* pattern_ptr) {
    return *pattern_ptr == '\0';
}

static const char* pattern_match_start(Pattern_State* ps, const char* str, const char* pattern);

static bool pattern_match_class(char c, char cls) {
    bool res;
    switch(tolower(cls)) {
    case 'a':
        res = isalpha(c);
        break;
    case 'c':
        res = iscntrl(c);
        break;
    case 'd':
        res = isdigit(c);
        break;
    case 'l':
        res = islower(c);
        break;
    case 'p':
        res = ispunct(c);
        break;
    case 's':
        res = isspace(c);
        break;
    case 'u':
        res = isupper(c);
        break;
    case 'w':
        res = isalnum(c);
        break;
    case 'x':
        res = isxdigit(c);
        break;
    case 'g':
        res = isprint(c) && !isspace(c);
        break;
    case 'z':
        res = c == '\0';
        break;
    default:
        return c == cls;
    }
    return isupper(cls) ? !res : res;
}

static bool pattern_match_custom_class(char c, const char* pattern_ptr, const char* class_end) {
    bool ret = true;
    if(pattern_ptr[1] == '^') {
        ret = false;
        pattern_ptr++;
    }

    while(++pattern_ptr < class_end) {
        if(*pattern_ptr == PATTERN_ESCAPE) {
            pattern_ptr++;
            if(pattern_match_class(c, *pattern_ptr)) {
                return ret;
            }
        } else if(pattern_ptr[1] == '-' && pattern_ptr + 2 < class_end) {
            pattern_ptr += 2;
            if(pattern_ptr[-2] <= c && c <= *pattern_ptr) {
                return ret;
            }
        } else if(*pattern_ptr == c) {
            return ret;
        }
    }

    return !ret;
}

static bool pattern_match_class_or_char(char c, const char* pattern, const char* class_end) {
    switch(*pattern) {
    case '.':
        return true;
    case PATTERN_ESCAPE:
        return pattern_match_class(c, pattern[1]);
    case '[':
        return pattern_match_custom_class(c, pattern, class_end - 1);
    default:
        return c == *pattern;
    }
}

static const char* pattern_match_balanced(Pattern_State* ps, const char* string_ptr,
                                          const char* pattern_ptr) {
    if(pattern_is_at_pattern_end(&pattern_ptr[2]) || pattern_is_at_pattern_end(&pattern_ptr[3])) {
        pattern_set_error(ps, PATTERN_ERR_INVALID_BALANCED_PATTERN, pattern_ptr - ps->pattern_base);
        return NULL;
    }

    char open = pattern_ptr[2];
    char close = pattern_ptr[3];

    if(pattern_is_at_end(ps, string_ptr) || *string_ptr != open) {
        return NULL;
    }

    string_ptr++;
    int count = 1;
    while(!pattern_is_at_end(ps, string_ptr)) {
        if(*string_ptr == open) {
            count++;
        } else if(*string_ptr == close) {
            count--;
            if(count == 0) {
                return pattern_match_start(ps, string_ptr + 1, pattern_ptr + 4);
            }
        }
        string_ptr++;
    }

    return NULL;
}

static const char* pattern_match_frontier(Pattern_State* ps, const char* string_ptr,
                                          const char* pattern_ptr) {
    if(pattern_is_at_pattern_end(&pattern_ptr[1]) || pattern_ptr[2] != '[') {
        pattern_set_error(ps, PATTERN_ERR_UNCLOSED_FRONTIER_PATTERN,
                          pattern_ptr - ps->pattern_base);
        return NULL;
    }

    const char* class_start = &pattern_ptr[2];
    const char* class_ptr = class_start + 1;
    while(!pattern_is_at_pattern_end(class_ptr) && *class_ptr != ']') {
        if(*class_ptr == PATTERN_ESCAPE && !pattern_is_at_pattern_end(&class_ptr[1])) {
            class_ptr += 2;
        } else {
            class_ptr++;
        }
    }

    if(*class_ptr != ']') {
        pattern_set_error(ps, PATTERN_ERR_UNCLOSED_FRONTIER_PATTERN,
                          pattern_ptr - ps->pattern_base);
        return NULL;
    }

    const char* class_end = class_ptr;

    char prev_char = (string_ptr > ps->data.data) ? string_ptr[-1] : '\0';
    char curr_char = pattern_is_at_end(ps, string_ptr) ? '\0' : *string_ptr;
    bool prev_in_set = pattern_match_custom_class(prev_char, class_start, class_end);
    bool curr_in_set = pattern_match_custom_class(curr_char, class_start, class_end);

    if(!prev_in_set && curr_in_set) {
        return pattern_match_start(ps, string_ptr, class_end + 1);
    }

    return NULL;
}

static int pattern_finish_captures(Pattern_State* ps, const char* pattern_ptr) {
    for(int i = ps->capture_count - 1; i > 0; i--) {
        if(ps->captures[i].size == PATTERN_CAPTURE_UNFINISHED) return i;
    }
    pattern_set_error(ps, PATTERN_ERR_UNEXPECTED_CAPTURE_CLOSE, pattern_ptr - ps->pattern_base);
    return -1;
}

static const char* pattern_start_capture(Pattern_State* ps, const char* string_ptr,
                                         const char* pattern_ptr) {
    if(ps->capture_count >= PATTERN_MAX_CAPTURES) {
        pattern_set_error(ps, PATTERN_ERR_MAX_CAPTURES, pattern_ptr - ps->pattern_base);
        return NULL;
    }

    if(pattern_ptr[1] != ')') {
        ps->captures[ps->capture_count].size = PATTERN_CAPTURE_UNFINISHED;
    } else {
        ps->captures[ps->capture_count].size = PATTERN_CAPTURE_POSITION;
        pattern_ptr++;
    }
    ps->captures[ps->capture_count].data = string_ptr;
    ps->capture_count++;

    const char* res = pattern_match_start(ps, string_ptr, pattern_ptr + 1);
    if(!res) ps->capture_count--;

    return res;
}

static const char* pattern_end_capture(Pattern_State* ps, const char* string_ptr,
                                       const char* pattern_ptr) {
    int i = pattern_finish_captures(ps, pattern_ptr);
    if(i == -1) return NULL;

    ps->captures[i].size = string_ptr - ps->captures[i].data;
    const char* res = pattern_match_start(ps, string_ptr, pattern_ptr + 1);
    if(!res) ps->captures[i].size = PATTERN_CAPTURE_UNFINISHED;

    return res;
}

static const char* pattern_match_capture(Pattern_State* ps, const char* string_ptr,
                                         const char* capture_idx_ptr, int capture_idx) {
    if(capture_idx > ps->capture_count - 1 ||
       ps->captures[capture_idx].size == PATTERN_CAPTURE_UNFINISHED ||
       ps->captures[capture_idx].size == PATTERN_CAPTURE_POSITION) {
        pattern_set_error(ps, PATTERN_ERR_INVALID_CAPTURE_IDX, capture_idx_ptr - ps->pattern_base);
        return NULL;
    }

    const char* capture = ps->captures[capture_idx].data;
    size_t capture_len = ps->captures[capture_idx].size;
    if((size_t)(ps->data.data + ps->data.size - string_ptr) < capture_len ||
       memcmp(string_ptr, capture, capture_len) != 0) {
        return NULL;
    }

    return string_ptr + capture_len;
}

static const char* pattern_greedy_match(Pattern_State* ps, const char* string_ptr,
                                        const char* pattern_ptr, const char* cls_end) {
    ptrdiff_t i = 0;
    while(!pattern_is_at_end(ps, &string_ptr[i]) &&
          pattern_match_class_or_char(string_ptr[i], pattern_ptr, cls_end)) {
        i++;
    }

    while(i >= 0) {
        const char* res = pattern_match_start(ps, string_ptr + i, cls_end + 1);
        if(res) return res;
        if(ps->error) return NULL;
        i--;
    }

    return NULL;
}

static const char* pattern_lazy_match(Pattern_State* ps, const char* string_ptr,
                                      const char* pattern_ptr, const char* cls_end) {
    do {
        const char* res = pattern_match_start(ps, string_ptr, cls_end + 1);
        if(res) return res;
        if(ps->error) return NULL;
    } while(!pattern_is_at_end(ps, string_ptr) &&
            pattern_match_class_or_char(*string_ptr++, pattern_ptr, cls_end));

    return NULL;
}

static const char* pattern_find_class_end(Pattern_State* ps, const char* pattern_ptr) {
    switch(*pattern_ptr++) {
    case PATTERN_ESCAPE:
        if(pattern_is_at_pattern_end(pattern_ptr)) {
            pattern_set_error(ps, PATTERN_ERR_INCOMPLETE_ESCAPE,
                              pattern_ptr - 1 - ps->pattern_base);
            return NULL;
        }
        return pattern_ptr + 1;
    case '[': {
        const char* cls_start = pattern_ptr - 1;
        if(*pattern_ptr == '^') pattern_ptr++;
        do {
            if(pattern_is_at_pattern_end(pattern_ptr)) {
                pattern_set_error(ps, PATTERN_ERR_UNCLOSED_CLASS, cls_start - ps->pattern_base);
                return NULL;
            }
            if(*pattern_ptr++ == PATTERN_ESCAPE && !pattern_is_at_pattern_end(pattern_ptr)) {
                pattern_ptr++;
            }
        } while(*pattern_ptr != ']');

        return pattern_ptr + 1;
    }
    default:
        return pattern_ptr;
    }
}

static const char* pattern_match_rep_operator(Pattern_State* ps, const char* string_ptr,
                                              const char* pattern_ptr) {
    const char* class_end = pattern_find_class_end(ps, pattern_ptr);
    if(!class_end) return NULL;

    bool is_match = !pattern_is_at_end(ps, string_ptr) &&
                    pattern_match_class_or_char(*string_ptr, pattern_ptr, class_end);
    switch(*class_end) {
    case '?': {
        const char* res;
        if(is_match && (res = pattern_match_start(ps, string_ptr + 1, class_end + 1))) {
            return res;
        }
        return pattern_match_start(ps, string_ptr, class_end + 1);
    }
    case '+':
        return is_match ? pattern_greedy_match(ps, string_ptr + 1, pattern_ptr, class_end) : NULL;
    case '*':
        return pattern_greedy_match(ps, string_ptr, pattern_ptr, class_end);
    case '-':
        return pattern_lazy_match(ps, string_ptr, pattern_ptr, class_end);
    default:
        return is_match ? pattern_match_start(ps, string_ptr + 1, class_end) : NULL;
    }
}

static const char* pattern_match_start(Pattern_State* ps, const char* string_ptr,
                                       const char* pattern_ptr) {
    switch(*pattern_ptr) {
    case '\0':
        return string_ptr;
    case '(':
        return pattern_start_capture(ps, string_ptr, pattern_ptr);
    case ')':
        return pattern_end_capture(ps, string_ptr, pattern_ptr);
    case '$':
        // Treat `$` specially only if at pattern end
        if(pattern_is_at_pattern_end(&pattern_ptr[1])) {
            return pattern_is_at_end(ps, string_ptr) ? string_ptr : NULL;
        }
        return pattern_match_rep_operator(ps, string_ptr, pattern_ptr);
    case PATTERN_ESCAPE:
        switch(pattern_ptr[1]) {
        case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8':
        case '9': {  // If there are digits after a `%`, then it's a capture reference
            int digit_count = 1;
            while(isdigit(pattern_ptr[digit_count])) {
                digit_count++;
            }

            int capture = strtol(pattern_ptr + 1, NULL, 10);
            if(!(string_ptr = pattern_match_capture(ps, string_ptr, pattern_ptr + 1, capture))) {
                return NULL;
            }

            return pattern_match_start(ps, string_ptr, pattern_ptr + digit_count);
        }
        case 'b':  // Check for balanced match `%bxy`
            return pattern_match_balanced(ps, string_ptr, pattern_ptr);
        case 'f':  // Check for frontier pattern `%f[set]`
            return pattern_match_frontier(ps, string_ptr, pattern_ptr);
        default:   // No special handling required, proceed with matching
            return pattern_match_rep_operator(ps, string_ptr, pattern_ptr);
        }
    default:
        return pattern_match_rep_operator(ps, string_ptr, pattern_ptr);
    }
}

static void pattern_check_unclosed_captures(Pattern_State* ps) {
    for(int i = 1; i < ps->capture_count; i++) {
        if(ps->captures[i].size == PATTERN_CAPTURE_UNFINISHED) {
            // Find position of unclosed capture
            int captures = 0;
            const char* pat = ps->pattern_base;
            while(pat < ps->pattern_base + strlen(ps->pattern_base)) {
                if(*pat == PATTERN_ESCAPE) {
                    pat += 2;
                    continue;
                }
                if(*pat == '(') {
                    if(++captures == i) break;
                }
                pat++;
            }
            pattern_set_error(ps, PATTERN_ERR_UNCLOSED_CAPTURE, pat - ps->pattern_base);
            return;
        }
    }
}

Pattern_Status pattern_match(Pattern_State* ps, const void* data, size_t len, const char* pattern) {
    return pattern_match_ex(ps, data, len, pattern, 0);
}

Pattern_Status pattern_match_ex(Pattern_State* ps, const void* data, size_t len,
                                const char* pattern, ptrdiff_t starting_pos) {
    pattern_init(ps, data, len, pattern);
    if(starting_pos < 0) starting_pos += len;  // negative starting_pos start from end of string
    assert(starting_pos >= 0 && (size_t)starting_pos <= len && "starting_pos out of bounds");

    const char* str = (const char*)data + starting_pos;
    if(*pattern == '^') {
        const char* res = pattern_match_start(ps, str, pattern + 1);
        pattern_check_unclosed_captures(ps);
        if(ps->error) return PATTERN_ERROR;
        if(res) {
            ps->captures[0].size = res - str;
            return PATTERN_MATCH;
        }
    } else {
        do {
            const char* res = pattern_match_start(ps, str, pattern);
            pattern_check_unclosed_captures(ps);
            if(ps->error) return PATTERN_ERROR;
            if(res) {
                ps->captures[0].data = str;
                ps->captures[0].size = res - str;
                return PATTERN_MATCH;
            }
        } while(!pattern_is_at_end(ps, str++));
    }

    return PATTERN_NO_MATCH;
}

Pattern_Status pattern_match_cstr(Pattern_State* ps, const char* str, const char* pattern) {
    size_t len = strlen(str);
    return pattern_match(ps, str, len, pattern);
}

Pattern_Status pattern_match_cstr_ex(Pattern_State* ps, const char* str, const char* pattern,
                                     ptrdiff_t starting_pos) {
    size_t len = strlen(str);
    return pattern_match_ex(ps, str, len, pattern, starting_pos);
}

bool pattern_is_position_capture(const Pattern_State* ps, int capture_idx) {
    assert(capture_idx < ps->capture_count && "Capture index out of bounds");
    return ps->captures[capture_idx].size == PATTERN_CAPTURE_POSITION;
}

size_t pattern_get_capture_pos(const Pattern_State* ps, int capture_idx) {
    assert(capture_idx < ps->capture_count && "Capture index out of bounds");
    return ps->captures[capture_idx].data - ps->data.data;
}

const char* pattern_strerror(Pattern_Error err) {
    switch(err) {
    case PATTERN_ERR_NONE:
        return "no error";
    case PATTERN_ERR_MAX_CAPTURES:
        return "max capture number exceeded";
    case PATTERN_ERR_UNEXPECTED_CAPTURE_CLOSE:
        return "unexpected capture close";
    case PATTERN_ERR_UNCLOSED_CAPTURE:
        return "capture not closed";
    case PATTERN_ERR_INVALID_CAPTURE_IDX:
        return "invalid capture index";
    case PATTERN_ERR_INCOMPLETE_ESCAPE:
        return "incomplete escape";
    case PATTERN_ERR_UNCLOSED_CLASS:
        return "unclosed character class";
    case PATTERN_ERR_INVALID_BALANCED_PATTERN:
        return "invalid balanced pattern (expected %bxy)";
    case PATTERN_ERR_UNCLOSED_FRONTIER_PATTERN:
        return "unclosed frontier pattern (expected %f[set])";
    }
    assert(false && "Unreachable");
}

void pattern_print_error(FILE* stream, const Pattern_State* ps) {
    assert(ps->error && "Pattern isn't in an error state");
    fprintf(stream, "column:%zu: %s\n", ps->error_loc, pattern_strerror(ps->error));
    fprintf(stream, "%s\n", ps->pattern_base);
    for(size_t i = 0; i < ps->error_loc; i++) {
        fprintf(stream, " ");
    }
    fprintf(stream, "^\n");
}

#endif  // PATTERN_IMPLEMENTATION
#endif  // PATTERN_H_

/**
 * MIT License
 *
 * Copyright (c) 2025 Fabrizio Pietrucci
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
