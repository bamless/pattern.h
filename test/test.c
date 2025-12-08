#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#undef __STRICT_ANSI__
#define CTEST_MAIN
#define CTEST_SEGFAULT
#define CTEST_COLOR_OK
#include "ctest.h"
#define PATTERN_IMPLEMENTATION
#include "../pattern.h"

int main(int argc, const char** argv) {
    return ctest_main(argc, argv);
}

static bool capture_eq(Pattern_Substring c, const char* o) {
    ASSERT_TRUE(c.size >= 0);
    return (size_t)c.size == strlen(o) && memcmp(o, c.data, c.size) == 0;
}

CTEST(pattern, star) {
    Pattern_State ps;
    pattern_match_cstr(&ps, "aaab", ".*b");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[0], "aaab"));
    pattern_match_cstr(&ps, "aaa", ".*a");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[0], "aaa"));
    pattern_match_cstr(&ps, "b", ".*b");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[0], "b"));
}

CTEST(pattern, plus) {
    Pattern_State ps;
    pattern_match_cstr(&ps, "aaab", ".+b");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[0], "aaab"));
    pattern_match_cstr(&ps, "aaa", ".+a");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[0], "aaa"));
    pattern_match_cstr(&ps, "b", ".+b");
    ASSERT_TRUE(ps.status == PATTERN_NO_MATCH);
}

CTEST(pattern, question) {
    Pattern_State ps;
    pattern_match_cstr(&ps, "aaab", ".?b");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[0], "ab"));
    pattern_match_cstr(&ps, "aaa", ".?a");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[0], "aa"));
    pattern_match_cstr(&ps, "b", ".?b");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[0], "b"));
}

CTEST(pattern, captures) {
    Pattern_State ps;
    pattern_match_cstr(&ps, "alo xyzK", "(%w+)K");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[1], "xyz"));
    pattern_match_cstr(&ps, "254 K", "(%d*)K");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[1], ""));
    pattern_match_cstr(&ps, "alo ", "(%w*)$");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[1], ""));
    pattern_match_cstr(&ps, "alo ", "(%w+)$");
    ASSERT_TRUE(ps.status == PATTERN_NO_MATCH);
    pattern_match_cstr(&ps, "testtset", "^(tes(t+)set)$");
    ASSERT_TRUE(ps.status == PATTERN_MATCH);
    ASSERT_TRUE(capture_eq(ps.captures[1], "testtset"));  // whole match
    ASSERT_TRUE(capture_eq(ps.captures[2], "tt"));        // inner (t+) group
}

CTEST(pattern, emptymatch) {
    Pattern_State ps;
    pattern_match_cstr(&ps, "", "");
    ASSERT_TRUE(capture_eq(ps.captures[0], ""));
    pattern_match_cstr(&ps, "alo", "");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[0], ""));
}

CTEST(pattern, nul_in_data) {
    Pattern_State ps;
    pattern_match(&ps, "a\0o a\0o a\0o", 11, "a");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[0], "a"));
    ASSERT_TRUE(ps.data.data == ps.captures[0].data);
    pattern_match(&ps, "a\0a\0a\0a\0\0ab", 11, "b");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[0], "b"));
    ASSERT_TRUE(pattern_get_capture_pos(&ps, 0) == 10);
}

CTEST(pattern, nul_in_pattern) {
    Pattern_State ps;
    pattern_match(&ps, "a\0\0a\0ab", 7, "b%z");
    ASSERT_TRUE(ps.status == PATTERN_NO_MATCH);
    pattern_match(&ps, "a\0\0a\0ab\0", 8, "b%z");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && ps.captures[0].size == 2 &&
                pattern_get_capture_pos(&ps, 0) == 6);
}

CTEST(pattern, start_anchor) {
    Pattern_State ps;
    pattern_match_cstr(&ps, "cantami123odiva", "12");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[0], "12"));
    pattern_match_cstr(&ps, "cantami123odiva", "^12");
    ASSERT_TRUE(ps.status == PATTERN_NO_MATCH);
    pattern_match_cstr(&ps, "12cantami123odiva", "^12");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[0], "12"));
    ASSERT_TRUE(ps.data.data == ps.captures[0].data);
}

CTEST(pattern, matches_and_operators) {
    Pattern_State ps;
    pattern_match_cstr(&ps, "aloALO", "%l*");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[0], "alo"));
    pattern_match_cstr(&ps, "aLo_ALO", "%a*");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[0], "aLo"));
    pattern_match_cstr(&ps, "  \n\r*&\n\r   xuxu  \n\n", "%g%g%g+");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[0], "xuxu"));
    pattern_match_cstr(&ps, "aaab", "a*");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[0], "aaa"));
    pattern_match_cstr(&ps, "aaa", "^.*$");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[0], "aaa"));
    pattern_match_cstr(&ps, "aaa", "b*");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[0], ""));
    pattern_match_cstr(&ps, "aaa", "b*");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[0], ""));
    pattern_match_cstr(&ps, "aaa", "ab*a");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[0], "aa"));
    pattern_match_cstr(&ps, "aba", "ab*a");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[0], "aba"));
    pattern_match_cstr(&ps, "aaab", "a+");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[0], "aaa"));
    pattern_match_cstr(&ps, "aaa", "^.+$");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[0], "aaa"));
    pattern_match_cstr(&ps, "aaa", "b+");
    ASSERT_TRUE(ps.status == PATTERN_NO_MATCH);
    pattern_match_cstr(&ps, "aaa", "ab+a");
    ASSERT_TRUE(ps.status == PATTERN_NO_MATCH);
    pattern_match_cstr(&ps, "aba", "ab+a");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[0], "aba"));
    pattern_match_cstr(&ps, "a$a", ".$");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[0], "a"));
    pattern_match_cstr(&ps, "a$a", ".%$");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[0], "a$"));
    pattern_match_cstr(&ps, "a$a", ".$.");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[0], "a$a"));
    pattern_match_cstr(&ps, "a$a", "$$");
    ASSERT_TRUE(ps.status == PATTERN_NO_MATCH);
    pattern_match_cstr(&ps, "a$b", "a$");
    ASSERT_TRUE(ps.status == PATTERN_NO_MATCH);
    pattern_match_cstr(&ps, "a$a", "$");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[0], ""));
    pattern_match_cstr(&ps, "", "b*");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[0], ""));
    pattern_match_cstr(&ps, "aaa", "bb*");
    ASSERT_TRUE(ps.status == PATTERN_NO_MATCH);
    pattern_match_cstr(&ps, "aaab", "a-");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[0], ""));
    pattern_match_cstr(&ps, "aaa", "^.-$");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[0], "aaa"));
    pattern_match_cstr(&ps, "aabaaabaaabaaaba", "b.*b");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[0], "baaabaaabaaab"));
    pattern_match_cstr(&ps, "aabaaabaaabaaaba", "b.-b");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[0], "baaab"));
    pattern_match_cstr(&ps, "alo xo", ".o$");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[0], "xo"));
    pattern_match_cstr(&ps, " \n isto é assim", "%S%S*");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[0], "isto"));
    pattern_match_cstr(&ps, " \n isto é assim", "%S*$");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[0], "assim"));
    pattern_match_cstr(&ps, " \n isto e assim", "[a-z]*$");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[0], "assim"));
    pattern_match_cstr(&ps, "um caracter ? extra", "[^%sa-z]");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[0], "?"));
    pattern_match_cstr(&ps, "", "a?");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[0], ""));
    pattern_match_cstr(&ps, "abl", "a?b?l?");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[0], "abl"));
    pattern_match_cstr(&ps, "aa", "^aa?a?a");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[0], "aa"));
    pattern_match_cstr(&ps, "0alo alo", "%x*");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[0], "0a"));
    pattern_match_cstr(&ps, "alo alo", "%C+");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[0], "alo alo"));
    pattern_match_cstr(&ps, "(álo)", "%(á");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && capture_eq(ps.captures[0], "(á"));
    pattern_match_cstr(&ps, "==", "^([=]*)=%1$");
    ASSERT_TRUE(ps.status == PATTERN_NO_MATCH);
    pattern_match_cstr(&ps, "===", "^([=]*)=%1$");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && ps.capture_count == 2 &&
                capture_eq(ps.captures[0], "===") && capture_eq(ps.captures[1], "="));
    pattern_match_cstr(&ps, "====", "^([=]*)=%1$");
    ASSERT_TRUE(ps.status == PATTERN_NO_MATCH);
    pattern_match_cstr(&ps, "==========", "^([=]*)=%1$");
    ASSERT_TRUE(ps.status == PATTERN_NO_MATCH);
}

CTEST(pattern, nested_captures) {
    Pattern_State ps;
    pattern_match_cstr(&ps, "clo alo", "^(((.).).* (%w*))$");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && ps.capture_count == 5);
    ASSERT_TRUE(pattern_get_capture_pos(&ps, 1) == 0);
    ASSERT_TRUE(capture_eq(ps.captures[1], "clo alo"));
    ASSERT_TRUE(capture_eq(ps.captures[2], "cl"));
    ASSERT_TRUE(capture_eq(ps.captures[3], "c"));
    ASSERT_TRUE(capture_eq(ps.captures[4], "alo"));

    pattern_match_cstr(&ps, "0123456789", "(.+(.?)())");
    ASSERT_TRUE(ps.status == PATTERN_MATCH && ps.capture_count == 4);
    ASSERT_TRUE(pattern_get_capture_pos(&ps, 1) == 0);
    ASSERT_TRUE(capture_eq(ps.captures[1], "0123456789"));
    ASSERT_TRUE(capture_eq(ps.captures[2], ""));
    ASSERT_TRUE(pattern_is_position_capture(&ps, 3) && pattern_get_capture_pos(&ps, 3) == 10);
}

CTEST(pattern, errors) {
    Pattern_State ps;

    pattern_match_cstr(&ps, "  a", "  (.");
    ASSERT_TRUE(ps.status == PATTERN_ERROR && ps.error == PATTERN_ERR_UNCLOSED_CAPTURE);
    pattern_print_error(stderr, &ps);

    pattern_match_cstr(&ps, " a", " .+)");
    ASSERT_TRUE(ps.status == PATTERN_ERROR && ps.error == PATTERN_ERR_UNEXPECTED_CAPTURE_CLOSE);
    pattern_print_error(stderr, &ps);

    pattern_match_cstr(&ps, "a", "[a");
    ASSERT_TRUE(ps.status == PATTERN_ERROR && ps.error == PATTERN_ERR_UNCLOSED_CLASS);
    pattern_print_error(stderr, &ps);

    pattern_match_cstr(&ps, "a", "[]");
    ASSERT_TRUE(ps.status == PATTERN_ERROR && ps.error == PATTERN_ERR_UNCLOSED_CLASS);
    pattern_print_error(stderr, &ps);

    pattern_match_cstr(&ps, "a", "[^]");
    ASSERT_TRUE(ps.status == PATTERN_ERROR && ps.error == PATTERN_ERR_UNCLOSED_CLASS);
    pattern_print_error(stderr, &ps);

    pattern_match_cstr(&ps, " a", " [a%]");
    ASSERT_TRUE(ps.status == PATTERN_ERROR && ps.error == PATTERN_ERR_UNCLOSED_CLASS);
    pattern_print_error(stderr, &ps);

    pattern_match_cstr(&ps, " a", " [a%");
    ASSERT_TRUE(ps.status == PATTERN_ERROR && ps.error == PATTERN_ERR_UNCLOSED_CLASS);
    pattern_print_error(stderr, &ps);

    pattern_match_cstr(&ps, "a", "%");
    ASSERT_TRUE(ps.status == PATTERN_ERROR && ps.error == PATTERN_ERR_INCOMPLETE_ESCAPE);
    pattern_print_error(stderr, &ps);

    pattern_match_cstr(&ps, "aaa", "(.)%1%2");
    ASSERT_TRUE(ps.status == PATTERN_ERROR && ps.error == PATTERN_ERR_INVALID_CAPTURE_IDX);
    pattern_print_error(stderr, &ps);
}
