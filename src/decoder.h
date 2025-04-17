#pragma once

#include "util.h"
#include "string_builder.h"
#include <cstdarg>

#define INSTRUCTION_LINE_SIZE 32
#define INSTRUCTION_FMT "%-" XStringify(INSTRUCTION_LINE_SIZE) "s"

#define MAX_BYTES_PER_INSTRUCTION_8086 6
struct Byte_Stack_8086 {
	u8 items[MAX_BYTES_PER_INSTRUCTION_8086];
	i8 count;
};

enum struct Register : u8 {
	a = 0, b, c, d, sp, bp, si, di,
	cs, ds, ss, es, ip, fl, Count,
};
static_assert(static_cast<u8>(Register::Count) == 0b1110);

#define IDToReg(reg) static_cast<Register>(reg)
#define RegToID(reg) static_cast<u8>(reg)
#define RegisterCount RegToID(Register::Count)

enum struct RegisterUsage : u8 { x, l, h };

struct RegisterInfo {
    Register type : 4;
    RegisterUsage usage : 2;
};

u16 getRegisterValue(RegisterInfo const& reg);
void setRegisterValue(RegisterInfo const& reg, u16 value);
const char* getRegisterName(RegisterInfo const& reg);
RegisterInfo getRegisterInfo(const char* reg);

struct Immediate {
    union {
        i8  byte;
        i16 word;
    };
    bool wide : 1;
};

#define signExtendByte(value) (cast(i8)value)
#define signExtendWord(value) (cast(i16)value)

#define makeImmediateByte(Byte) Immediate{.byte=Byte, .wide=0}
#define makeImmediateWord(Word) Immediate{.word=Word, .wide=1}
#define makeImmediate(Wide, Value) ((Wide) ? makeImmediateWord(signExtendWord(Value)) : makeImmediateByte(signExtendByte(Value)))

namespace EffectiveAddress {
	enum struct Base : u8 {
		Direct = 0,
        bx_si, bx_di, bp_si,
        bp_di, si, di, bp, bx,
	};
	struct Info {
		Base base;
		Immediate displacement;
	};
	const char* base2string(Base base);
}

enum struct Instruction_Operand_Type : u8 {
	None = 0,
	Immediate,
	Register,
	EffectiveAddress,
};

enum struct Instruction_Operand_Prefix : u8 { None = 0, Byte, Word };

struct Instruction_Operand {
	union {
		RegisterInfo reg;
		EffectiveAddress::Info address;
		Immediate immediate;
	};
	Instruction_Operand_Type type     : 2;
	Instruction_Operand_Prefix prefix : 2;
};

#define InstPrefix(Wide)                       \
	((Wide) ? Instruction_Operand_Prefix::Word \
            : Instruction_Operand_Prefix::Byte)

#define InstImmediateByte(Byte) Instruction_Operand{ \
	.immediate = Immediate{.byte=Byte, .wide=0},     \
	.type = Instruction_Operand_Type::Immediate,     \
	.prefix = Instruction_Operand_Prefix::None       \
}

#define InstImmediateWord(Word) Instruction_Operand{ \
	.immediate = Immediate{.word=Word, .wide=1},     \
	.type = Instruction_Operand_Type::Immediate,     \
	.prefix = Instruction_Operand_Prefix::None       \
}

#define InstImmediate(Wide, Value) Instruction_Operand{ \
	.immediate = makeImmediate(Wide, Value),            \
	.type = Instruction_Operand_Type::Immediate,        \
	.prefix = Instruction_Operand_Prefix::None          \
}

#define InstImmediatePrefixed(Wide, Value) Instruction_Operand{ \
	.immediate = makeImmediate(Wide, Value),                    \
	.type = Instruction_Operand_Type::Immediate,                \
	.prefix = InstPrefix(Wide)                                  \
}

#define InstGeneralReg(Type, Usage) Instruction_Operand{           \
	.reg = {.type = Register::Type, .usage = RegisterUsage::Usage},\
	.type = Instruction_Operand_Type::Register,                    \
}

#define InstNonGeneralReg(Type) Instruction_Operand{           \
	.reg = {.type = Register::Type, .usage = RegisterUsage::x},\
	.type = Instruction_Operand_Type::Register,                \
}

String_Builder operand2string(Instruction_Operand const& operand);

namespace FlagsRegister {
    enum struct Bit : u16 { CF = 0, PF = 2, AF = 4, ZF = 6, SF, OF, IF, DF, TF};
    constexpr Bit BitList[] = { Bit::CF, Bit::PF, Bit::AF, Bit::ZF, Bit::SF, Bit::OF, Bit::IF, Bit::DF, Bit::TF };
	void setBit(Bit const& bit, bool value = true);
	u16 get();
	bool getBit(Bit const& bit, u16 flags);
	bool getBit(Bit const& bit);
	void printSet(FILE* outFile, u16 flags);
	void printSet(FILE* outFile);
    const char* getFullName(Bit const& bit);
    char getLetter(Bit const& bit);
}

// Expects to be indexed as [REG][W] or as [R/M][W]
constexpr const char* REG_TABLE[8][2] = {
	[0b000] = {"al", "ax"},
	[0b001] = {"cl", "cx"},
	[0b010] = {"dl", "dx"},
	[0b011] = {"bl", "bx"},
	[0b100] = {"ah", "sp"},
	[0b101] = {"ch", "bp"},
	[0b110] = {"dh", "si"},
	[0b111] = {"bh", "di"},
};

// Expects to be indexed as [REG][W] or as [R/M][W]
constexpr Instruction_Operand REG_Table[8][2] = {
	[0b000] = {InstGeneralReg(a,l), InstGeneralReg(a,x)},
	[0b001] = {InstGeneralReg(c,l), InstGeneralReg(c,x)},
	[0b010] = {InstGeneralReg(d,l), InstGeneralReg(d,x)},
	[0b011] = {InstGeneralReg(b,l), InstGeneralReg(b,x)},
	[0b100] = {InstGeneralReg(a,h), InstNonGeneralReg(sp)},
	[0b101] = {InstGeneralReg(c,h), InstNonGeneralReg(bp)},
	[0b110] = {InstGeneralReg(d,h), InstNonGeneralReg(si)},
	[0b111] = {InstGeneralReg(b,h), InstNonGeneralReg(di)},
};

// Expects to be indexed as [SR]
constexpr const char* SR_TABLE[4] = {
	[0b00] = "es",
	[0b01] = "cs",
	[0b10] = "ss",
	[0b11] = "ds",
};

// Expects to be indexed as [SR]
constexpr Instruction_Operand SR_Table[4] = {
	[0b00] = InstNonGeneralReg(es),
	[0b01] = InstNonGeneralReg(cs),
	[0b10] = InstNonGeneralReg(ss),
	[0b11] = InstNonGeneralReg(ds),
};

// Expects to be indexed as [R/M]
constexpr const char* EFFECTIVE_ADDRESS_TABLE[8] = {
	[0b000] = "bx + si",
	[0b001] = "bx + di",
	[0b010] = "bp + si",
	[0b011] = "bp + di",
	[0b100] = "si",
	[0b101] = "di",
	[0b110] = "bp",      // DIRECT ADDRESS if MOD = 00
	[0b111] = "bx",
};

// Expects to be indexed as [R/M]
constexpr EffectiveAddress::Base Effective_Address_Table[8] = {
	[0b000] = EffectiveAddress::Base::bx_si,
	[0b001] = EffectiveAddress::Base::bx_di,
	[0b010] = EffectiveAddress::Base::bp_si,
	[0b011] = EffectiveAddress::Base::bp_di,
	[0b100] = EffectiveAddress::Base::si,
	[0b101] = EffectiveAddress::Base::di,
	[0b110] = EffectiveAddress::Base::bp,      // DIRECT ADDRESS if MOD = 00
	[0b111] = EffectiveAddress::Base::bx,
};

constexpr const char* registerNames[RegisterCount] = {
	"ax", "bx", "cx", "dx", "sp", "bp", "si", "di", "cs",
	"ds", "ss", "es", "ip", "fl",
};

constexpr const char* registerFullNames[RegisterCount] = {
	"Accumulator", "Base", "Count", "Data", "Stack Pointer",
	"Base Pointer", "Source Index", "Destination Index",
	"Code Segment", "Data Segment", "Stack Segment",
	"Extra Segment", "Instruction Pointer", "Flag",
};

extern u16 gRegisterValues[RegisterCount];

Register getReg(const char* reg);
#define getRegFullName(reg) registerFullNames[RegToID(reg)]

force_inline inline void incrementIP(i16 const value) {
	RegisterInfo constexpr ip = {Register::ip, RegisterUsage::x};
	u16 const old = getRegisterValue(ip);
    setRegisterValue(ip, old + value);
}

force_inline inline u16 getIP() {
	return gRegisterValues[RegToID(Register::ip)];
}


bool decodeOrSimulate(FILE* outFile, Slice<u8> binaryBytes, bool exec);
void printBits(FILE* outFile, u8 byte, int count);
void printBits(FILE* outFile, u8 byte, int count, char ending);

struct Decoder_Context {
	FILE* const outFile;
	Slice<u8> const& binaryBytes;
	Byte_Stack_8086 byteStack = {};
	i64 bytesRead = 0;
	bool exec = false;

	explicit Decoder_Context(FILE* OutFile, Slice<u8> const& BinaryBytes, bool const Exec):
		outFile(OutFile), binaryBytes(BinaryBytes), exec(Exec) {}

	void advance(u8& currentByte) {
		assertTrue(byteStack.count + 1 <= MAX_BYTES_PER_INSTRUCTION_8086);
		assertTrue(bytesRead + 1 <= binaryBytes.count);
		currentByte = binaryBytes.ptr[bytesRead++];
		byteStack.items[byteStack.count++] = currentByte;
	}

	void backOne(u8& currentByte) {
		assertTrue(byteStack.count - 1 >= 0);
		assertTrue(bytesRead - 1 >= 0);
		--bytesRead;
		currentByte = binaryBytes.ptr[bytesRead > 0 ? bytesRead - 1 : 0];
		byteStack.items[--byteStack.count] = 0;
	}

	[[nodiscard]] u16 read16Bits(int offset) {
		if (offset < 0) {
			offset = byteStack.count + offset;
			assertTrue(offset >= 0);
		}
		assertTrue(offset > 0 && offset < byteStack.count);
		return *reinterpret_cast<u16*>(byteStack.items + offset);
	}

	[[nodiscard]] u8 read08Bits(int offset) const {
		if (offset < 0) {
			offset = byteStack.count + offset;
			assertTrue(offset >= 0);
		}
		assertTrue(offset > 0 && offset < byteStack.count);
		return byteStack.items[offset];
	}

	[[nodiscard]] u16 advance8or16Bits(bool const condition, u8& byte) {
		u16 data;

		advance(byte);
		if (condition) {
			advance(byte);
			data = read16Bits(-2);
		} else {
			data = read08Bits(-1);
		}
		return data;
	}

	[[nodiscard]] u16 advance16Bits(u8& byte) {
		advance(byte);
		advance(byte);
		return read16Bits(-2);
	}

	[[nodiscard]] u8 advance08Bits(u8& byte) {
		advance(byte);
		return byte;
	}

	void printSR(u8 const SR, char const ending) const {
		fprintf(outFile, "SR:");
		printBits(outFile, SR, 2, ending);
	}

	void printMOD(u8 const MOD, char const ending) const {
		fprintf(outFile, "MOD:");
		printBits(outFile, MOD, 2, ending);
	}

	void printREG(u8 const REG, char const ending) const {
		fprintf(outFile, "REG:");
		printBits(outFile, REG, 3, ending);
	}

	void printR_M(u8 const R_M, char const ending) const {
		fprintf(outFile, "R/M:");
		printBits(outFile, R_M, 3, ending);
	}

	void printFlagsLN(u16 const oldFlags) const {
		FlagsRegister::printSet(outFile, oldFlags);
		print("->");
		FlagsRegister::printSet(outFile);
		fputc('\n', outFile);
	}

	void printIP(char const* ending) const {
		printIP();
		fputs(ending, outFile);
    }

	void printIP() const {
		u16 const ip = gRegisterValues[RegToID(Register::ip)];
		print("ip:0x%x->0x%x", ip, ip+byteStack.count);
    }

	void printRegistersLN() const {
		for (const char* name: registerNames) {
			u16 const value = getRegisterValue(getRegisterInfo(name));
			if (value == 0) {
				continue;
			}
			if (0 == strcmp(name, "fl")) {
				print("%8s: ", "flags", value, value);
				FlagsRegister::printSet(outFile);
				fputc('\n', outFile);
			} else {
				println("%8s: 0x%04x (%d)", name, value, value);
			}
		}
	}

	void print(const char* fmt, ...) const {
		va_list args;
		va_start(args, fmt);
		vfprintf(outFile, fmt, args);
		va_end(args);
	}

	void println(const char* fmt, ...) const {
		va_list args;
		va_start(args, fmt);
		vfprintf(outFile, fmt, args);
		fputc('\n', outFile);
		va_end(args);
	}

	void printInst(const char* mnemonic, Instruction_Operand const& dst, Instruction_Operand const& src) const {
		char instruction[INSTRUCTION_LINE_SIZE+1] = {0};
		auto builderDst = operand2string(dst);
		auto builderSrc = operand2string(src);
		snprintf(instruction, sizeof(instruction), "%s %s, %s", mnemonic, builderDst.items, builderSrc.items);
		print(INSTRUCTION_FMT" ", instruction);
		builderDst.destroy();
		builderSrc.destroy();
	}

	void printInst(const char* mnemonic, Instruction_Operand const& operand) const {
		char instruction[INSTRUCTION_LINE_SIZE+1] = {0};
		auto builder = operand2string(operand);
		snprintf(instruction, sizeof(instruction), "%s %s", mnemonic, builder.items);
		print(INSTRUCTION_FMT" ", instruction);
		builder.destroy();
	}

	void printInst(const char* mnemonic, const char* dst, const char* src) const {
		char instruction[INSTRUCTION_LINE_SIZE+1] = {0};
		snprintf(instruction, sizeof(instruction), "%s %s, %s", mnemonic, dst, src);
		print(INSTRUCTION_FMT" ", instruction);
	}

	void printInst(const char* mnemonic, const char* operand) const {
		char instruction[INSTRUCTION_LINE_SIZE+1] = {0};
		snprintf(instruction, sizeof(instruction), "%s %s", mnemonic, operand);
		print(INSTRUCTION_FMT" ", instruction);
	}

	void printByteStack() const {
		for (size_t i = 0; i < byteStack.count; i++) {
			printBits(outFile, byteStack.items[i], 8, i < byteStack.count-1 ? ' ' : '\n');
		}
	}

	void resetByteStack() {
		memset(&byteStack, 0, sizeof(byteStack));
		byteStack.count = 0;
	}
};

