#pragma once

#include "util.h"
#include "string_builder.h"
#include <cstdarg>

#define INSTRUCTION_LINE_SIZE 30

#define COMMENT_COLOR      ASCII_COLOR_GREEN
#define MNEMONIC_COLOR     ASCII_COLOR_B_RED
#define NUMBER_COLOR       ASCII_COLOR_CYAN
#define INST_PREFIX_COLOR  ASCII_COLOR_MAGENTA
#define REGISTER_COLOR     ASCII_COLOR_YELLOW

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

#define RegX(R) RegisterInfo{.type=Register::R, .usage=RegisterUsage::x}
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
		bool wide;
	};
    const char* base2string(Base base);
	i16 getInnerValue(Info const& info);
}

namespace Jumps {
	enum struct Type : u8 {
		jo = 112,     // 0b01110000 = 112
		jno,          // 0b01110001 = 113
		jb,           // 0b01110010 = 114
		jnb,          // 0b01110011 = 115
		je,           // 0b01110100 = 116
		jne,          // 0b01110101 = 117
		jbe,          // 0b01110110 = 118
		ja,           // 0b01110111 = 119
		js,           // 0b01111000 = 120
		jns,          // 0b01111001 = 121
		jp,           // 0b01111010 = 122
		jnp,          // 0b01111011 = 123
		jl,           // 0b01111100 = 124
		jnl,          // 0b01111101 = 125
		jle,          // 0b01111110 = 126
		jg,           // 0b01111111 = 127
		loop = 224,   // 0b11100010 = 224
		loopz,        // 0b11100001 = 225
		loopnz,       // 0b11100000 = 226
		jcxz,         // 0b11100011 = 227
	};

	#define ByteToJumpType(byte) static_cast<Jumps::Type>(byte)

	struct Info {
		Type type;
		i8 offset;
	};

	enum struct Outcome { error, jumped, stayed };

	const char* getMnemonic(Type type);
	const char* getOutcomeName(Outcome outcome);
}

enum struct Instruction_Operand_Prefix : u8 { None = 0, Byte, Word };
constexpr const char* InstOpPrefixName[3] = { "", "byte", "word"};

enum struct Instruction_Operand_Type : u8 {
	None = 0,
	Immediate,
	Register,
	EffectiveAddress,
	Jump,
};

#define X(x) [static_cast<u8>(Instruction_Operand_Type::x)] = #x
constexpr const char* Instruction_Operand_Type_Names[] = {
	X(None), X(Immediate), X(Register), X(EffectiveAddress), X(Jump),
};
#undef X

struct Instruction_Operand {
	union {
		RegisterInfo reg;
		EffectiveAddress::Info address;
		Immediate immediate;
		Jumps::Info jump;
	};
	Instruction_Operand_Type type;
	Instruction_Operand_Prefix prefix : 2;
};

#define IsInstRegister(inst)  ((inst).type == Instruction_Operand_Type::Register)
#define IsInstImmediate(inst) ((inst).type == Instruction_Operand_Type::Immediate)
#define IsInstMemory(inst)    ((inst).type == Instruction_Operand_Type::EffectiveAddress)

// rr: Register, []: Memory, XX: Immediate
//
//     1. inst rr, (rr | XX | [])
//     2. inst [], (rr | XX)
//
#define IsBinaryInstTypeOrderValid(dst, src)                                                        \
    ((IsInstRegister(dst) && (IsInstRegister(src) || IsInstImmediate(src) || IsInstMemory(src))) || \
     (IsInstMemory(dst)   && (IsInstRegister(src) || IsInstImmediate(src))))

String_Builder getInstOpName(Instruction_Operand const& operand);
u16  getInstOpValue(Instruction_Operand const& operand);
void setInstOpValue(Instruction_Operand const& operand, u16 value);

#define InstPrefix(Wide)                       \
	((Wide) ? Instruction_Operand_Prefix::Word \
            : Instruction_Operand_Prefix::Byte)

#define InstJump(typeByte, data) Instruction_Operand{ \
	.jump=Jumps::Info{                                \
		.type=ByteToJumpType(typeByte),               \
		.offset=signExtendByte(data),                 \
	},                                                \
	.type = Instruction_Operand_Type::Jump,           \
	.prefix = Instruction_Operand_Prefix::None        \
}

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
constexpr Instruction_Operand SR_Table[4] = {
	[0b00] = InstNonGeneralReg(es),
	[0b01] = InstNonGeneralReg(cs),
	[0b10] = InstNonGeneralReg(ss),
	[0b11] = InstNonGeneralReg(ds),
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

#define X(R) [RegToID(Register::R)] = RegX(R)
constexpr RegisterInfo RegisterList[RegisterCount] = {
	X(a),  X(b),  X(c),  X(d),  X(sp), X(bp), X(si),
	X(di), X(cs), X(ds), X(ss), X(es), X(ip), X(fl),
};
#undef X

constexpr const char* RegisterNames[RegisterCount] = {
	[RegToID(Register::a)] = "ax",  [RegToID(Register::b)] = "bx",
	[RegToID(Register::c)] = "cx",  [RegToID(Register::d)] = "dx",
	[RegToID(Register::sp)] = "sp", [RegToID(Register::bp)] = "bp",
	[RegToID(Register::si)] = "si", [RegToID(Register::di)] = "di",
	[RegToID(Register::cs)] = "cs", [RegToID(Register::ds)] = "ds",
	[RegToID(Register::ss)] = "ss", [RegToID(Register::es)] = "es",
	[RegToID(Register::ip)] = "ip", [RegToID(Register::fl)] = "fl",
};

constexpr const char* RegisterExtraNames[8] = {
	[0] = "ah", [1] = "al",
	[2] = "bh", [3] = "bl",
	[4] = "ch", [5] = "cl",
	[6] = "dh", [7] = "dl",
};

constexpr const char* RegisterFullNames[RegisterCount] = {
	"Accumulator", "Base", "Count", "Data", "Stack Pointer",
	"Base Pointer", "Source Index", "Destination Index",
	"Code Segment", "Data Segment", "Stack Segment",
	"Extra Segment", "Instruction Pointer", "Flag",
};

extern u16 gRegisterValues[RegisterCount];
extern u8 gMemory[1024 * 1024];

Register getReg(const char* reg);

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

	void printlnFlags(u16 const oldFlags) const {
		FlagsRegister::printSet(outFile, oldFlags);
		print("->");
		FlagsRegister::printSet(outFile);
		fputc('\n', outFile);
		if (outFile == stdout) print(ASCII_COLOR_END);
	}

	void printlnIP() const {
		printIP();
		fputc('\n', outFile);
		if (outFile == stdout) print(ASCII_COLOR_END);
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
		for (size_t i = 0; i < RegisterCount; i++) {
			RegisterInfo const reg = RegisterList[i];
			const char* name = RegisterNames[i];
			u16 const value = getRegisterValue(reg);
			if (value == 0) continue;
			if (reg.type == Register::fl) {
				print("%8s: ", "flags", value, value);
				FlagsRegister::printSet(outFile);
				fputc('\n', outFile);
			} else {
				println("%8s: 0x%04x (%d)", name, value, value);
			}
		}
	}

	[[nodiscard]] int printEffectiveAddressBase(EffectiveAddress::Base base) const;
	[[nodiscard]] int printInstOperand(Instruction_Operand const& operand) const;

	void print(const char* fmt, ...) const {
		va_list args;
		va_start(args, fmt);
		vfprintf(outFile, fmt, args);
		va_end(args);
	}

	int _print(const char* fmt, ...) const {
		va_list args;
		va_start(args, fmt);
		int const n = vfprintf(outFile, fmt, args);
		va_end(args);
		return n;
	}

	void println(const char* fmt, ...) const {
		va_list args;
		va_start(args, fmt);
		vfprintf(outFile, fmt, args);
		fputc('\n', outFile);
		va_end(args);
	}

	void printBitsHeader() const {
		if (!exec) {
			if (outFile == stdout) {
				println(MNEMONIC_COLOR "bits" ASCII_COLOR_END " " NUMBER_COLOR "16" ASCII_COLOR_END);
			} else {
				println("bits 16");
			}
		}
	}

	void printInst(const char* mnemonic, Instruction_Operand const& dst, Instruction_Operand const& src) const {
		int n = 0;
		if (outFile == stdout) print(MNEMONIC_COLOR);
		n += _print("%s ", mnemonic);
		if (outFile == stdout) print(ASCII_COLOR_END);
		n += printInstOperand(dst);
		if (src.type != Instruction_Operand_Type::None) {
			n += _print(", ");
			n += printInstOperand(src);
		}
		n ++; fputc(' ', outFile);
		for (int i = 0; i < INSTRUCTION_LINE_SIZE-n; i++) {
			fputc(' ', outFile);
		}
		if (outFile == stdout) print(COMMENT_COLOR);
	}

	void printInst(const char* mnemonic, Instruction_Operand const& operand) const {
		printInst(mnemonic, operand, Instruction_Operand{.type=Instruction_Operand_Type::None});
	}

	void printByteStack() const {
		for (size_t i = 0; i < byteStack.count; i++) {
			if (i > 0) fputc(' ', outFile);
			printBits(outFile, byteStack.items[i], 8);
		}
		if (outFile == stdout) print(ASCII_COLOR_END);
		fputc('\n', outFile);
	}

	void resetByteStack() {
		memset(&byteStack, 0, sizeof(byteStack));
		byteStack.count = 0;
	}
};

