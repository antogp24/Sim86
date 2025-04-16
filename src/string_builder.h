#ifndef STRING_BUILDER_H
#define STRING_BUILDER_H

#include <stdint.h>
#include <inttypes.h>

#include "util.h"

#define STRING_BUILDER_DEFAULT_CAPACITY 8

struct String_Builder {
    char* items = nullptr;
    size_t len = 0;
    size_t cap = STRING_BUILDER_DEFAULT_CAPACITY;

    [[nodiscard]] String_Builder copy() const;
    void append(std::string const& str);
    void append(String_Builder const& builder);
    void append(String_View const& view);
    void append(const char* str);
    void append(const char* str, size_t);
    void append(char c);
    void append(u8 value);
    void append(u16 value);
    void append(u32 value);
    void append(u64 value);
    void append(i8 value);
    void append(i16 value);
    void append(i32 value);
    void append(i64 value);
    void append_u8(u8 value);
    void append_u16(u16 value);
    void append_u32(u32 value);
    void append_u64(u64 value);
    void append_i8(i8 value);
    void append_i16(i16 value);
    void append_i32(i32 value);
    void append_i64(i64 value);
    void destroy();

private:
    void grow(size_t);
};

String_Builder string_builder_make();
String_Builder string_builder_from(const char* str);
String_Builder string_builder_from_u16(u16 value);

#endif //STRING_BUILDER_H
