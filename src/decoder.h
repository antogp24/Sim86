#pragma once

#include "util.h"
#include "string_builder.h"
#include <cstdarg>

#if defined(_WIN32) || defined(_WIN64)
	#include <io.h>
	#define isatty _isatty
	#define fileno _fileno
#else
	#include <unistd.h>
#endif

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

enum Disp_Type : u8 { Disp_None = 0, Disp_08_bit, Disp_16_bit };

// (MOD = 0b00) mode:   (Memory), disp: (R/M==110 ? 16-bit : none)
// (MOD = 0b01) mode:   (Memory), disp: (08-bit)
// (MOD = 0b10) mode:   (Memory), disp: (16-bit)
// (MOD = 0b11) mode: (Register), disp: (none)
Disp_Type get_Disp_Type(u8 MOD, u8 R_M);

#define is_MOD_Valid(MOD) ((MOD) >= 0b00  && (MOD) <= 0b11)
#define is_R_M_Valid(R_M) ((R_M) >= 0b000 && (R_M) <= 0b111)
#define is_REG_Valid(REG) ((REG) >= 0b000 && (REG) <= 0b111)
#define is_SR_Valid(SR)   ((SR)  >= 0b00  && (SR)  <= 0b11)

#define is_MOD_Register_Mode(MOD) ((MOD) == 0b11)

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

#define IsRegisterGeneral(reg)    \
	((reg).type == Register::a || \
	 (reg).type == Register::b || \
	 (reg).type == Register::c || \
	 (reg).type == Register::d)

#define IsRegisterSegment(reg)     \
	((reg).type == Register::cs || \
	 (reg).type == Register::ds || \
	 (reg).type == Register::ss || \
	 (reg).type == Register::es)

u16 getRegisterValue(RegisterInfo const& reg);
void setRegisterValue(RegisterInfo const& reg, u16 value);
void incrementRegister(RegisterInfo const& reg, i16 increment);
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

Immediate makeImmediateDisp(u16 disp, u8 MOD, u8 R_M);

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
	u32 getInnerValue(Info const& info);
	u8 getClocks(Info const& info);

	#define is_Effective_Address_Direct(MOD, R_M) ((MOD) == 0b00 && (R_M) == 0b110)

	#define has_EA_Disp(ea) ((ea).displacement.word != 0)
	#define is_EA_Base_or_Index(ea)                 \
		((ea).base == EffectiveAddress::Base::bx || \
		 (ea).base == EffectiveAddress::Base::bp || \
		 (ea).base == EffectiveAddress::Base::si || \
		 (ea).base == EffectiveAddress::Base::di)
	#define is_EA_Base_Plus_Index(ea)                  \
		((ea).base == EffectiveAddress::Base::bx_si || \
		 (ea).base == EffectiveAddress::Base::bx_di || \
		 (ea).base == EffectiveAddress::Base::bp_si || \
		 (ea).base == EffectiveAddress::Base::bp_di)
}

enum Clock_Calculation_Part_Type : u8 {
	Clock_None = 0,
	Clock_Inst,
	Clock_EA,
	Clock_Range,
	Clock_AorB,
	Clock_SegmentOverride,
	Clock_16bitTransfer,
};

struct Clock_Calculation_Part {
	union {
		u16 value;
		struct { u8 A; u8 B; };
		EffectiveAddress::Info address;
	};
	Clock_Calculation_Part_Type type;
};

struct Clock_Calculation {
	Clock_Calculation_Part parts[4];
	u8 part_count;
};

#define ClocksPush(calc, part) do { \
	u8 const _i = (calc).part_count++;        \
	StaticArrayBoundsCheck(_i, (calc).parts); \
	(calc).parts[_i] = part;                  \
} while (0)

#define ClocksPushInst(calc, val) \
	ClocksPush(calc, (Clock_Calculation_Part{.value=val, .type=Clock_Inst}))

#define ClocksPushEA(calc, ea, transfers) do {                               \
	ClocksPush(calc, (Clock_Calculation_Part{.address=ea, .type=Clock_EA})); \
	ClocksPush16bitTransfer(calc, ea, transfers);                            \
} while (0)

#define ClocksPush16bitTransfer(calc, ea, transfers) do {             \
	bool const is_odd = EffectiveAddress::getInnerValue(ea) % 2 == 1; \
	bool const is_wide = (ea).wide;                                   \
	if (is_wide && is_odd) {                                          \
		ClocksPush(calc, (Clock_Calculation_Part{                     \
			.value=transfers,                                         \
			.type=Clock_16bitTransfer})                               \
		);                                                            \
	}                                                                 \
} while (0)

u8 getPartClocks(Clock_Calculation_Part const& part);
u16 getTotalClocks(Clock_Calculation const& calculation);
void explainClocks(FILE* f, Clock_Calculation const& calculation);

namespace Jumps {
	typedef i8 Offset;

	enum struct Outcome : u8 { error, jumped, stayed};
	constexpr const char* Outcome_Names[3] = {"error", "jump", "stayed"};
	#define GetJumpOutcomeName(outcome) Jumps::Outcome_Names[static_cast<u8>(outcome)]

	#define JumpModifiesCX(inst_type) \
		((inst_type) == Inst_loopnz || \
		 (inst_type) == Inst_loopz  || \
		 (inst_type) == Inst_loop)
}

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

#define GetInstOpTypeName(op) Instruction_Operand_Type_Names[static_cast<u8>((op).type)]

struct Instruction_Operand {
	union {
		RegisterInfo reg;
		EffectiveAddress::Info address;
		Immediate immediate;
		Jumps::Offset jump_offset;
	};
	Instruction_Operand_Type type;
};

#define IsOperandReg(operand) ((operand).type == Instruction_Operand_Type::Register)
#define IsOperandImm(operand) ((operand).type == Instruction_Operand_Type::Immediate)
#define IsOperandMem(operand) ((operand).type == Instruction_Operand_Type::EffectiveAddress)

#define IsOperandReg16(operand) (IsOperandReg(operand) && (operand).reg.usage == RegisterUsage::x)
#define IsOperandMem16(operand) (IsOperandMem(operand) && (operand).address.wide)

#define IsOperandSegment(operand) (IsOperandReg(operand) && IsRegisterSegment((operand).reg))
#define IsOperandGeneralReg(operand) (IsOperandReg(operand) && IsRegisterGeneral((operand).reg))
#define IsOperandAccumulator(operand) (IsOperandReg(operand) && (operand).reg.type == Register::a)

// rr: Register, []: Memory, XX: Immediate
//
//     1. inst rr, (rr | XX | [])
//     2. inst [], (rr | XX)
//
#define IsBinaryInstTypeOrderValid(inst)                       \
    ((IsOperandReg((inst).dst) && (IsOperandReg((inst).src) || \
                                   IsOperandImm((inst).src) || \
                                   IsOperandMem((inst).src)))  \
                                                            || \
     (IsOperandMem((inst).dst) && (IsOperandReg((inst).src) || \
	                               IsOperandImm((inst).src))))


#define ErrorComment_InvalidInstructionTypeOrder(inst) \
	LOG_ERROR_STRING ": `%s <%s> <%s>` is an invalid order of types.", \
	GetInstMnemonic(inst), \
	GetInstOpTypeName((inst).dst), \
	GetInstOpTypeName((inst).src)

String_Builder getInstOpName(Instruction_Operand const& operand);
u16  getInstOpValue(Instruction_Operand const& operand);
void setInstOpValue(Instruction_Operand const& operand, u16 value);

#define InstOpNone Instruction_Operand{ .type = Instruction_Operand_Type::None }

#define InstOpJump(data) Instruction_Operand{ \
	.jump_offset=signExtendByte(data),        \
	.type = Instruction_Operand_Type::Jump,   \
}

#define InstOpImmediate(Wide, Value) Instruction_Operand{ \
	.immediate = makeImmediate(Wide, Value),            \
	.type = Instruction_Operand_Type::Immediate,        \
}

#define InstOpEffectiveAddress(MOD, R_M, Wide, Disp) Instruction_Operand{ \
	.address = EffectiveAddress::Info {                                   \
		.base = (is_Effective_Address_Direct(MOD, R_M))                   \
				? EffectiveAddress::Base::Direct                          \
				: Effective_Address_Table[R_M],                           \
		.displacement = makeImmediateDisp(Disp, MOD, R_M),                \
		.wide = Wide,                                                     \
	},                                                                    \
	.type = Instruction_Operand_Type::EffectiveAddress,                   \
}

#define InstOpEffectiveAddressDirectWord(Wide, Disp) Instruction_Operand{ \
		.address = EffectiveAddress::Info {                               \
			.base = EffectiveAddress::Base::Direct,                       \
			.displacement = makeImmediateWord(Disp),                      \
			.wide = Wide,                                                 \
		},                                                                \
		.type = Instruction_Operand_Type::EffectiveAddress,               \
	}

#define InstOpGeneralReg(Type, Usage) Instruction_Operand{           \
	.reg = {.type = Register::Type, .usage = RegisterUsage::Usage},  \
	.type = Instruction_Operand_Type::Register,                      \
}

#define InstOpNonGeneralReg(Type) Instruction_Operand{           \
	.reg = {.type = Register::Type, .usage = RegisterUsage::x},  \
	.type = Instruction_Operand_Type::Register,                  \
}

enum Instruction_Type : u8 {
	Inst_None = 0,
	#include "instructions.inl"
	Inst_Count,
};

struct Instruction {
	Instruction_Operand dst;
	Instruction_Operand src;
	Instruction_Type type;
};

Clock_Calculation getInstructionClocksCalculation(Instruction const& inst);

#define SwapInstructionOperands(inst) Swap(Instruction_Operand, (inst).dst, (inst).src)

constexpr const char* Instruction_Mnemonics[Inst_Count] = {
	#define Inst(x) #x,
	Inst(INVALID_INSTRUCTION)
	#include "instructions.inl"
};
#define GetInstMnemonic(inst) Instruction_Mnemonics[static_cast<u8>((inst).type)]

enum struct Instruction_Operand_Prefix : u8 { None, Byte,   Word };
constexpr const char* InstOpPrefixName[3] = { "",  "byte", "word"};
Instruction_Operand_Prefix getInstDstPrefix(Instruction const& inst);

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
	[0b000] = {InstOpGeneralReg(a,l), InstOpGeneralReg(a,x)},
	[0b001] = {InstOpGeneralReg(c,l), InstOpGeneralReg(c,x)},
	[0b010] = {InstOpGeneralReg(d,l), InstOpGeneralReg(d,x)},
	[0b011] = {InstOpGeneralReg(b,l), InstOpGeneralReg(b,x)},
	[0b100] = {InstOpGeneralReg(a,h), InstOpNonGeneralReg(sp)},
	[0b101] = {InstOpGeneralReg(c,h), InstOpNonGeneralReg(bp)},
	[0b110] = {InstOpGeneralReg(d,h), InstOpNonGeneralReg(si)},
	[0b111] = {InstOpGeneralReg(b,h), InstOpNonGeneralReg(di)},
};

// Expects to be indexed as [SR]
constexpr Instruction_Operand SR_Table[4] = {
	[0b00] = InstOpNonGeneralReg(es),
	[0b01] = InstOpNonGeneralReg(cs),
	[0b10] = InstOpNonGeneralReg(ss),
	[0b11] = InstOpNonGeneralReg(ds),
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
extern u64 gClocks;

Register getReg(const char* reg);

force_inline inline void incrementIP(i16 const value) {
	incrementRegister(RegX(ip), value);
}

force_inline inline u16 getIP() {
	return gRegisterValues[RegToID(Register::ip)];
}

bool decodeOrSimulate(FILE* outFile, Slice<u8> binaryBytes, bool exec, bool showClocks);
void printBits(FILE* outFile, u8 byte, int count);
void printBits(FILE* outFile, u8 byte, int count, char ending);

struct Decoder_Context {
	FILE* const outFile;
	Slice<u8> const& binaryBytes;
	Byte_Stack_8086 byteStack = {};
	i64 bytesRead = 0;
	bool exec = false;
	bool showClocks = false;

	explicit Decoder_Context(FILE* OutFile, Slice<u8> const& BinaryBytes, bool const Exec, bool const ShowClocks):
		outFile(OutFile), binaryBytes(BinaryBytes), exec(Exec), showClocks(ShowClocks) {}

	[[nodiscard]] force_inline bool shouldDecorateOutput() const {
		return isatty(fileno(outFile));
	}

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

	[[nodiscard]] u16 advanceDisplacement(u8 const MOD, u8 const R_M, u8& byte) {
		switch (get_Disp_Type(MOD, R_M)) {
			case Disp_None:   return 0;
			case Disp_08_bit: return advance08Bits(byte);
			case Disp_16_bit: return advance16Bits(byte);
			default: unreachable();
		}
	}

	void explainClocksUpdate(Instruction const& inst) const {
		if (showClocks) {
			explainClocks(outFile, getInstructionClocksCalculation(inst));
		}
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
		if (shouldDecorateOutput()) print(ASCII_COLOR_END);
	}

	void printlnIP() const {
		printIP();
		fputc('\n', outFile);
		if (shouldDecorateOutput()) print(ASCII_COLOR_END);
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
	[[nodiscard]] int printInstOperand(Instruction_Operand const& operand, Instruction_Operand_Prefix prefix) const;

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
			if (shouldDecorateOutput()) {
				println(MNEMONIC_COLOR "bits" ASCII_COLOR_END " " NUMBER_COLOR "16" ASCII_COLOR_END);
			} else {
				println("bits 16");
			}
		}
	}

	void printInst(Instruction const& inst) const {
		assertTrue(inst.dst.type != Instruction_Operand_Type::None);
		if (shouldDecorateOutput()) print(MNEMONIC_COLOR);
		int n = _print("%s ", GetInstMnemonic(inst));
		if (shouldDecorateOutput()) print(ASCII_COLOR_END);
		n += printInstOperand(inst.dst, getInstDstPrefix(inst));
		if (inst.src.type != Instruction_Operand_Type::None) {
			n += _print(", ");
			n += printInstOperand(inst.src, Instruction_Operand_Prefix::None);
		}
		for (int i = 0; i < INSTRUCTION_LINE_SIZE-n; i++) {
			fputc(' ', outFile);
		}
		if (shouldDecorateOutput()) print(COMMENT_COLOR);
		print(" ; ");
		explainClocksUpdate(inst);
	}

	void printByteStack() const {
		for (size_t i = 0; i < byteStack.count; i++) {
			if (i > 0) fputc(' ', outFile);
			printBits(outFile, byteStack.items[i], 8);
		}
		if (shouldDecorateOutput()) print(ASCII_COLOR_END);
		fputc('\n', outFile);
	}

	void resetByteStack() {
		memset(&byteStack, 0, sizeof(byteStack));
		byteStack.count = 0;
	}
};

