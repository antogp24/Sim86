#pragma once

// Table of ASCII Color Codes
// --------------------------------------------------------------------- //
// https://gist.github.com/RabaDabaDoba/145049536f815903c79944599c6f952a
// --------------------------------------------------------------------- //
#define ASCII_COLOR_END       "\033[0m"

// Regular.
#define ASCII_COLOR_BLACK   "\033[0;30m"
#define ASCII_COLOR_RED     "\033[0;31m"
#define ASCII_COLOR_GREEN   "\033[0;32m"
#define ASCII_COLOR_YELLOW  "\033[0;33m"
#define ASCII_COLOR_BLUE    "\033[0;34m"
#define ASCII_COLOR_MAGENTA "\033[0;35m"
#define ASCII_COLOR_CYAN    "\033[0;36m"
#define ASCII_COLOR_WHITE   "\033[0;37m"

// Bold.
#define ASCII_COLOR_B_BLACK   "\033[1;30m"
#define ASCII_COLOR_B_RED     "\033[1;31m"
#define ASCII_COLOR_B_GREEN   "\033[1;32m"
#define ASCII_COLOR_B_YELLOW  "\033[1;33m"
#define ASCII_COLOR_B_BLUE    "\033[1;34m"
#define ASCII_COLOR_B_MAGENTA "\033[1;35m"
#define ASCII_COLOR_B_CYAN    "\033[1;36m"
#define ASCII_COLOR_B_WHITE   "\033[1;37m"

// Underline.
#define ASCII_COLOR_U_BLACK   "\033[4;30m"
#define ASCII_COLOR_U_RED     "\033[4;31m"
#define ASCII_COLOR_U_GREEN   "\033[4;32m"
#define ASCII_COLOR_U_YELLOW  "\033[4;33m"
#define ASCII_COLOR_U_BLUE    "\033[4;34m"
#define ASCII_COLOR_U_MAGENTA "\033[4;35m"
#define ASCII_COLOR_U_CYAN    "\033[4;36m"
#define ASCII_COLOR_U_WHITE   "\033[4;37m"

// High Intensity Background.
#define ASCII_COLOR_HI_BG_BLACK   "\033[0;100m"
#define ASCII_COLOR_HI_BG_RED     "\033[0;101m"
#define ASCII_COLOR_HI_BG_GREEN   "\033[0;102m"
#define ASCII_COLOR_HI_BG_YELLOW  "\033[0;103m"
#define ASCII_COLOR_HI_BG_BLUE    "\033[0;104m"
#define ASCII_COLOR_HI_BG_MAGENTA "\033[0;105m"
#define ASCII_COLOR_HI_BG_CYAN    "\033[0;106m"
#define ASCII_COLOR_HI_BG_WHITE   "\033[0;107m"

// High Intensity Text.
#define ASCII_COLOR_HI_BLACK   "\033[0;90m"
#define ASCII_COLOR_HI_RED     "\033[0;91m"
#define ASCII_COLOR_HI_GREEN   "\033[0;92m"
#define ASCII_COLOR_HI_YELLOW  "\033[0;93m"
#define ASCII_COLOR_HI_BLUE    "\033[0;94m"
#define ASCII_COLOR_HI_MAGENTA "\033[0;95m"
#define ASCII_COLOR_HI_CYAN    "\033[0;96m"
#define ASCII_COLOR_HI_WHITE   "\033[0;97m"

// High Intensity Bold Text.
#define ASCII_COLOR_HI_B_BLACK   "\033[0;90m"
#define ASCII_COLOR_HI_B_RED     "\033[0;91m"
#define ASCII_COLOR_HI_B_GREEN   "\033[0;92m"
#define ASCII_COLOR_HI_B_YELLOW  "\033[0;93m"
#define ASCII_COLOR_HI_B_BLUE    "\033[0;94m"
#define ASCII_COLOR_HI_B_MAGENTA "\033[0;95m"
#define ASCII_COLOR_HI_B_CYAN    "\033[0;96m"
#define ASCII_COLOR_HI_B_WHITE   "\033[0;97m"
#include "util.h"

// An array of all the color codes.
// -------------------------------------------------- //

constexpr const char* ASCII_COLOR_TABLE[] = {
    ASCII_COLOR_END,

    // Regular.
    ASCII_COLOR_BLACK,
    ASCII_COLOR_RED,
    ASCII_COLOR_GREEN,
    ASCII_COLOR_YELLOW,
    ASCII_COLOR_BLUE,
    ASCII_COLOR_MAGENTA,
    ASCII_COLOR_CYAN,
    ASCII_COLOR_WHITE,

    // Bold.
    ASCII_COLOR_B_BLACK,
    ASCII_COLOR_B_RED,
    ASCII_COLOR_B_GREEN,
    ASCII_COLOR_B_YELLOW,
    ASCII_COLOR_B_BLUE,
    ASCII_COLOR_B_MAGENTA,
    ASCII_COLOR_B_CYAN,
    ASCII_COLOR_B_WHITE,

    // Underline.
    ASCII_COLOR_U_BLACK,
    ASCII_COLOR_U_RED,
    ASCII_COLOR_U_GREEN,
    ASCII_COLOR_U_YELLOW,
    ASCII_COLOR_U_BLUE,
    ASCII_COLOR_U_MAGENTA,
    ASCII_COLOR_U_CYAN,
    ASCII_COLOR_U_WHITE,

    // High Intensity Background.
    ASCII_COLOR_HI_BG_BLACK,
    ASCII_COLOR_HI_BG_RED,
    ASCII_COLOR_HI_BG_GREEN,
    ASCII_COLOR_HI_BG_YELLOW,
    ASCII_COLOR_HI_BG_BLUE,
    ASCII_COLOR_HI_BG_MAGENTA,
    ASCII_COLOR_HI_BG_CYAN,
    ASCII_COLOR_HI_BG_WHITE,

    // High Intensity Text.
    ASCII_COLOR_HI_BLACK,
    ASCII_COLOR_HI_RED,
    ASCII_COLOR_HI_GREEN,
    ASCII_COLOR_HI_YELLOW,
    ASCII_COLOR_HI_BLUE,
    ASCII_COLOR_HI_MAGENTA,
    ASCII_COLOR_HI_CYAN,
    ASCII_COLOR_HI_WHITE,

    // High Intensity Bold Text.
    ASCII_COLOR_HI_B_BLACK,
    ASCII_COLOR_HI_B_RED,
    ASCII_COLOR_HI_B_GREEN,
    ASCII_COLOR_HI_B_YELLOW,
    ASCII_COLOR_HI_B_BLUE,
    ASCII_COLOR_HI_B_MAGENTA,
    ASCII_COLOR_HI_B_CYAN,
    ASCII_COLOR_HI_B_WHITE,
};
#define ASCII_COLOR_COUNT (sizeof(ASCII_COLOR_TABLE) / sizeof(ASCII_COLOR_TABLE[0]))

inline size_t ASCII_COLOR_byteCount(const char* str) {
    size_t result = 0;
    for (size_t i = 0, len = strlen(str); i < len; i++) {
        if (str[i] != '\033') {
            continue;
        }
        for (auto const& color : ASCII_COLOR_TABLE) {
            const size_t colorLen = strlen(color);
            size_t n = colorLen;
            if (n > len - i) {
                n = len - i;
            }
            if (0 == strncmp(str+i, color, n)) {
                result += colorLen;
                goto next;
            }
        }
        next:;
    }
    return result;
}