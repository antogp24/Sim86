#include <cstdio>
#include <cstdlib>
#include <cinttypes>
#include <cstring>

#include "string_builder.h"

String_Builder string_builder_make() {
	size_t constexpr capacity = STRING_BUILDER_DEFAULT_CAPACITY;
	String_Builder builder = {
		.items = static_cast<char*>(malloc(capacity * sizeof(char))),
		.len = 0,
		.cap = capacity,
	};
	assertTrue(builder.items != nullptr);
	builder.items[0] = '\0';
	return builder;
}

String_Builder string_builder_from(const char* str) {
	size_t const length = strlen(str);
	size_t const capacity = length + 1;
	String_Builder builder = {
		.items = static_cast<char*>(malloc(capacity * sizeof(char))),
		.len = length,
		.cap = capacity,
	};
	assertTrue(builder.items != nullptr);
	for (size_t i = 0; i < length; i++) {
		builder.items[i] = str[i];
	}
	builder.items[length] = '\0';
	return builder;
}

String_Builder string_builder_from_u16(u16 value) {
	size_t constexpr capacity = 8;
	String_Builder builder = {
		.items = static_cast<char*>(calloc(capacity, sizeof(char))),
		.cap = capacity,
	};
	assertTrue(builder.items != nullptr);
	builder.len = snprintf(builder.items, capacity, "%" PRIu16, value);
	return builder;
}

void String_Builder::grow(size_t const minimum) {
	assertTrue(items != nullptr);
	assertTrue(minimum > 0);
	cap = static_cast<size_t>(static_cast<double>(minimum) * 1.5);
	items = static_cast<char*>(realloc(items, cap * sizeof(char)));
	assertTrue(items != nullptr);
}

void String_Builder::append(char const c) {
	size_t const old_len = len;
	len += 1;
	if (old_len + 1 + 1 > cap) {
		grow(old_len + 1 + 1);
	}
	items[old_len + 0] = c;
	items[old_len + 1] = '\0';
}

void String_Builder::append(const char* str, size_t const count) {
	if (count <= 0) {
		return;
	}

	size_t const old_len = len;
	len += count;
	if (old_len + count + 1 > cap) {
		grow(old_len + count + 1);
	}
	for (size_t i = 0; i < count; i++) {
		items[old_len + i] = str[i];
	}
	items[old_len + count] = '\0';
}

[[nodiscard]] String_Builder String_Builder::copy() const {
	String_Builder result = {};
	result.cap = len + 1;
	result.len = len;
	result.items = static_cast<char*>(malloc(result.cap));
	memcpy(result.items, items, result.cap);
	result.items[result.len] = '\0';
	return result;
}

void String_Builder::append(std::string const& str) {
	append(str.c_str(), str.size());
}

void String_Builder::append(String_Builder const& builder) {
	append(builder.items);
}

void String_Builder::append(String_View const& view) {
	append(view.items, view.count);
}

void String_Builder::append(const char* str) {
	append(str, strlen(str));
}

void String_Builder::append_u8(u8 const value) {
	char buf[U8_STR_SIZE_BASE10] = {0};
	size_t const chars_written = snprintf(buf, sizeof(buf), "%" PRIu8, value);
	assertTrue(chars_written != -1);
	append(buf, chars_written);
}

void String_Builder::append_u16(u16 const value) {
	char buf[U16_STR_SIZE_BASE10] = {0};
	size_t const chars_written = snprintf(buf, sizeof(buf), "%" PRIu16, value);
	assertTrue(chars_written != -1);
	append(buf, chars_written);
}

void String_Builder::append_u32(u32 const value) {
	char buf[U32_STR_SIZE_BASE10] = {0};
	size_t const chars_written = snprintf(buf, sizeof(buf), "%" PRIu32, value);
	assertTrue(chars_written != -1);
	append(buf, chars_written);
}

void String_Builder::append_u64(u64 const value) {
	char buf[U64_STR_SIZE_BASE10] = {0};
	size_t const chars_written = snprintf(buf, sizeof(buf), "%" PRIu64, value);
	assertTrue(chars_written != -1);
	append(buf, chars_written);
}

void String_Builder::append_i8(i8 const value) {
	char buf[I8_STR_SIZE_BASE10] = {0};
	size_t const chars_written = snprintf(buf, sizeof(buf), "%" PRIi8, value);
	assertTrue(chars_written != -1);
	append(buf, chars_written);
}

void String_Builder::append_i16(i16 const value) {
	char buf[I16_STR_SIZE_BASE10] = {0};
	size_t const chars_written = snprintf(buf, sizeof(buf), "%" PRIi16, value);
	assertTrue(chars_written != -1);
	append(buf, chars_written);
}

void String_Builder::append_i32(i32 const value) {
	char buf[I32_STR_SIZE_BASE10] = {0};
	size_t const chars_written = snprintf(buf, sizeof(buf), "%" PRIi32, value);
	assertTrue(chars_written != -1);
	append(buf, chars_written);
}

void String_Builder::append_i64(i64 const value) {
	char buf[I64_STR_SIZE_BASE10] = {0};
	size_t const chars_written = snprintf(buf, sizeof(buf), "%" PRIi64, value);
	assertTrue(chars_written != -1);
	append(buf, chars_written);
}

void String_Builder::append(u8 const value) {
	append_u8(value);
}

void String_Builder::append(u16 const value) {
	append_u16(value);
}

void String_Builder::append(u32 const value) {
	append_u32(value);
}

void String_Builder::append(u64 const value) {
	append_u64(value);
}

void String_Builder::append(i8 const value) {
	append_i8(value);
}

void String_Builder::append(i16 const value) {
	append_i16(value);
}

void String_Builder::append(i32 const value) {
	append_i32(value);
}

void String_Builder::append(i64 const value) {
	append_i64(value);
}

void String_Builder::destroy() {
	free(items);
	memset(this, 0, sizeof(String_Builder));
}
