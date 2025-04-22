#pragma once

#pragma once

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>
#include <chrono>
#include <cmath>
#include "ascii_colors.h"

// Preprocessor Utils
// ------------------------------------------------------------------------------------------------------ //
#define Join1(x, y) x##y
#define Join2(x, y) Join1(x, y)
#define Stringify(x) #x
#define XStringify(x) Stringify(x)

// Defer Statement: https://github.com/gingerBill/gb/blob/master/gb.h
// ------------------------------------------------------------------------------------------------------ //
template <typename T> struct cppRemoveReference       { typedef T Type; };
template <typename T> struct cppRemoveReference<T &>  { typedef T Type; };
template <typename T> struct cppRemoveReference<T &&> { typedef T Type; };

template <typename T> inline T &&cpp_forward(typename cppRemoveReference<T>::Type &t)  { return static_cast<T &&>(t); }
template <typename T> inline T &&cpp_forward(typename cppRemoveReference<T>::Type &&t) { return static_cast<T &&>(t); }
template <typename F>
struct DeferInfo {
	F f;
	explicit DeferInfo(F &&f) : f(cpp_forward<F>(f)) {}
	~DeferInfo() { f(); }
};
template <typename F> DeferInfo<F> defer_macro_func(F &&f) { return DeferInfo<F>(cpp_forward<F>(f)); }

#define DEFER_NAME() Join2(_defer_, __COUNTER__)
#define defer(code)  auto DEFER_NAME() = defer_macro_func([&]()->void{code;})

// Convenient Integer Types
// ------------------------------------------------------------------------------------------------------ //
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;

typedef float    f32;
typedef double   f64;

typedef unsigned int uint;

// Integer Limits as Decimal Strings
// ------------------------------------------------------------------------------------------------------ //
#define I8_MIN_STR_BASE10  "-128"
#define I16_MIN_STR_BASE10 "-32768"
#define I32_MIN_STR_BASE10 "-2147483648"
#define I64_MIN_STR_BASE10 "-9223372036854775808"

#define I8_MAX_STR_BASE10  "127"
#define I16_MAX_STR_BASE10 "32767"
#define I32_MAX_STR_BASE10 "2147483647"
#define I64_MAX_STR_BASE10 "9223372036854775807"

#define U8_MAX_STR_BASE10  "255"
#define U16_MAX_STR_BASE10 "65535"
#define U32_MAX_STR_BASE10 "4294967295"
#define U64_MAX_STR_BASE10 "18446744073709551615"

// Constants of enough size to make a string of an integer
// ------------------------------------------------------------------------------------------------------ //
#define I8_STR_SIZE_BASE10  StaticArrayCount(I8_MIN_STR_BASE10)
#define I16_STR_SIZE_BASE10 StaticArrayCount(I16_MIN_STR_BASE10)
#define I32_STR_SIZE_BASE10 StaticArrayCount(I32_MIN_STR_BASE10)
#define I64_STR_SIZE_BASE10 StaticArrayCount(I64_MIN_STR_BASE10)

#define U8_STR_SIZE_BASE10  StaticArrayCount(U8_MAX_STR_BASE10)
#define U16_STR_SIZE_BASE10 StaticArrayCount(U16_MAX_STR_BASE10)
#define U32_STR_SIZE_BASE10 StaticArrayCount(U32_MAX_STR_BASE10)
#define U64_STR_SIZE_BASE10 StaticArrayCount(U64_MAX_STR_BASE10)

// Convenient Macros
// ------------------------------------------------------------------------------------------------------ //
#define  printfln(   fmt, ...)  printf(        fmt"\n", ##__VA_ARGS__)
#define fprintfln(f, fmt, ...) fprintf(f,      fmt"\n", ##__VA_ARGS__)
#define eprintfln(fmt, ...)    fprintf(stderr, fmt"\n", ##__VA_ARGS__)
#define   eprintf(fmt, ...)    fprintf(stderr, fmt,     ##__VA_ARGS__)

#define StaticArrayCount(arr) std::size(arr)
#define StrLen(str) (StaticArrayCount(str)-1)
#define cast(T) (T)
#define Swap(T, a, b) do { T t = a; a = b; b = t; } while(0)
#define Unused(x) (void)x
#define UnsignedDigitCount(x) (x == 0 ? 1 : floor(log10(x)) + 1)

#define Sign(x)   ((x) >= 0 ? 1 : -1)
#define Min(a, b) ((a) < (b) ? (a) : (b))
#define Max(a, b) ((a) > (b) ? (a) : (b))
#define Clamp(x, min, max)  ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))

// Force inlining
#if !defined(force_inline)
	#if defined(_MSC_VER)
		#if _MSC_VER < 1300
		    #define force_inline
		#else
		    #define force_inline __forceinline
		#endif
	#else
		#define force_inline __attribute__ ((__always_inline__))
	#endif
#endif


// --------------------------------------------------------------------- //

#define LOG_INFO_STRING  ASCII_COLOR_BLUE "[INFO]" ASCII_COLOR_END
#define LOG_ERROR_STRING ASCII_COLOR_RED "[ERROR]" ASCII_COLOR_END

#define _DEBUG

#ifdef _DEBUG
    #ifdef _WIN32
        #define setBreakpoint() __debugbreak()
    #elif defined(__unix__) || defined(__APPLE__)
        #define setBreakpoint() __builtin_trap()
    #else
        #error "util.h (setBreakpoint): Unsupported platform"
    #endif
#endif

#if defined(_MSC_VER)
	// MSVC compiler error format.
	#define FatalDebugMsgFormat " %s(%d): "
#else
	// gcc/clang compiler error format.
	#define FatalDebugMsgFormat " %s:%d:0: "
#endif

#define FatalDebugMsg(type, fmt, ...) do {                            \
	fprintfln(                                                        \
		stderr,                                                       \
		ASCII_COLOR_RED type ASCII_COLOR_END FatalDebugMsgFormat fmt, \
		__FILE__, __LINE__, ##__VA_ARGS__);                           \
	setBreakpoint();                                                  \
	exit(1);                                                          \
} while(0)
#define assertTrue(c) do { if (!(c)) { FatalDebugMsg("[Assert]", "false == (%s)", #c); } }while(0)
#define panic(fmt, ...) FatalDebugMsg("[Panic]", fmt, ##__VA_ARGS__)
#define TODO(msg) FatalDebugMsg("[TODO]", msg)

#define StaticArrayBoundsCheck(i, arr) do {                   \
	if ((i) < 0 || (i) > StaticArrayCount(arr)) {             \
		FatalDebugMsg(                                        \
			"[OutOfBounds]",                                  \
			"Indexing " #arr "[%d] where len(" #arr ") = %d", \
			i, StaticArrayCount(arr));                        \
	}                                                         \
} while (0)

#define BoundsCheck(i, len) do {               \
	if ((i) < 0 || (i) > (len)) {              \
		FatalDebugMsg(                         \
			"[OutOfBounds]",                   \
			"%d is outside of [0, %d]", 0, i); \
	}                                          \
} while (0)

#ifdef _MSC_VER
	#define unreachable() do {                                      \
		FatalDebugMsg("[Unreachable]", "Reached unreachable code"); \
		__assume(false);                                            \
	} while(0)
#else
	#define unreachable() do {                                      \
		FatalDebugMsg("[Unreachable]", "Reached unreachable code"); \
		__builtin_unreachable();                                    \
	} while(0)
#endif


#define PtrBaseType(ptr) cppRemoveReference<decltype(*ptr)>::Type
#define PtrToSlice(ptr, size) Slice<PtrBaseType(ptr)>(ptr, size)
#define ArrayToSlice(array) Slice<PtrBaseType(array)>(array, StaticArrayCount(array))
#define StdVectorToSlice(v) Slice<PtrBaseType(v.data())>(v.data(), v.size())

template <typename T>
struct Slice {
	T* ptr = nullptr;
	size_t count = 0;

	Slice() = default;
	Slice(T* Ptr, size_t Count): ptr(Ptr), count(Count) {}
};

struct BakedFile {
    const char* label;
    uint8_t* data;
    size_t size;
};

struct Timer {
    std::chrono::high_resolution_clock::time_point start;
    const char* message;

    explicit Timer(const char* Message):
        start(std::chrono::high_resolution_clock::now()),
        message(Message)
    {
    }

    void stop() const {
        const auto end = std::chrono::high_resolution_clock::now();
        const std::chrono::duration<f64> elapsed = end - start;
        const f64 s = elapsed.count();
        const f64 ms = s * pow(10, 3);
        const f64 us = s * pow(10, 6);
        const f64 ns = s * pow(10, 9);

        printfln("Timing: %s", message);
        printfln("--------------------");
        printfln("%g s", s);
        printfln("%g ms", ms);
        printfln("%g us", us);
        printfln("%g ns", ns);
        printfln("--------------------");
    }
};

struct Scoped_Timer : public Timer {
	~Scoped_Timer() {
		stop();
	}
};

struct String_View {
    const char* items;
    size_t count;

	explicit String_View(std::string const& String): items(String.data()), count(String.size()) {}
    explicit String_View(const char* Items): items(Items), count(strlen(Items)) {}
	explicit String_View(const char* Items, const size_t Count): items(Items), count(Count) {}

	void advance(u32 const offset = 1) {
		if (offset <= count) {
			items += offset;
			count -= offset;
		} else {
			items = nullptr;
			count = 0;
		}
	}

	[[nodiscard]] std::vector<String_View> split(char c) const {
		std::vector<String_View> result = {};
		char* start = const_cast<char*>(items);
		for (u64 i = 0, offset = 0; i < count; i++, offset++) {
			if (items[i] == c) {
                if (offset == i) {
					result.emplace_back(start, offset);
                } else {
					result.emplace_back(start + 1, offset - 1);
                }
				start += offset;
                offset = 0;
            } else if (i == count - 1) {
				result.emplace_back(start + 1, offset);
            }
		}
#if 0
        printf("\"%.*s\".split('%c'): {", cast(u32)count, items, c);
        for (int i = 0; i < result.size(); i++) {
            auto const& part = result[i];
            if (i > 0) printf(", ");
            printf("\"%.*s\"", cast(u32)part.count, part.items);
        }
        printf("}\n");
#endif
		return result;
	}

	static bool isWhitespace(char c) {
		return c == ' ' || c == '\t' || c == '\r' || c == '\n';
	}

	[[nodiscard]] String_View trim() const {
		char* ptr = const_cast<char*>(items);
		while (isWhitespace(*ptr)) {
			if (ptr + 1 > items + count) {
				break;
			}
			ptr++;
		}
		size_t end = count - 1;
		while (isWhitespace(items[end])) {
			if (end - 1 < 0) {
				break;
			}
			end--;
		}
		return String_View(ptr, end + 1);
	}
};
