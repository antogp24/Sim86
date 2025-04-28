#include "common_instructions.h"

#include "decoder.h"
#include "string_builder.h"


struct Three_Variants {
	u8 firstByteLiteral[3];
	u8 format1Literal  : 3;
	bool has_S_on_fmt1 : 1;
};

struct One_Variant {
	u8 literal             : 8;
	u8 literal3bit         : 3;
	u8 shift               : 2;
	bool hasOnlyOneOperand : 1;
};

struct Common_Format {
	Instruction_Type type;
	union {
		One_Variant one;
		Three_Variants three;
	};
	bool hasOnlyOneVariant;
};

static u8 Count1s(u8 const byte) {
	return ((byte >> 0) & 0b1) + ((byte >> 1) & 0b1) +
           ((byte >> 2) & 0b1) + ((byte >> 3) & 0b1) +
           ((byte >> 4) & 0b1) + ((byte >> 5) & 0b1) +
	       ((byte >> 6) & 0b1) + ((byte >> 7) & 0b1);
}

static void setFlagsFromResult(u16 const result) {
	using namespace FlagsRegister;
	setBit(Bit::ZF, result == 0);
	setBit(Bit::PF, Count1s(result & 0xFF) % 2 == 0);
	setBit(Bit::SF, (result >> 15) & 0b1);
}

static u32 execOp(Instruction const& inst) {
	u32 const A = getInstOpValue(inst.dst);
	u32 const B = (inst.type != Inst_lea)
		? getInstOpValue(inst.src)
		: EffectiveAddress::getInnerValue(inst.src.address);

	switch (inst.type) {
		case Inst_lea: {
			assertTrue(IsOperandMem16(inst.src));
			u32 const result = A + B;
			setFlagsFromResult(result);
			return result;
		}
		case Inst_add: {
			u32 const result = A + B;
			setFlagsFromResult(result);
			return result;
		}
		case Inst_sub: {
			u32 const result = A - B;
			setFlagsFromResult(result);
			return result;
		}
		case Inst_cmp: {
			u32 const result = A - B;
			setFlagsFromResult(result);
			return A;
		}
		default: unreachable();
	}
}

void exec_common_inst(Decoder_Context const& decoder, Instruction const& inst) {
    if (IsBinaryInstTypeOrderValid(inst)) {
    	String_Builder dstName = getInstOpName(inst.dst);
    	defer(dstName.destroy());

		u16 const oldFlags = FlagsRegister::get();
        u32 const oldValue = getInstOpValue(inst.dst);
		u32 const newValue = execOp(inst);
        setInstOpValue(inst.dst, newValue);
        decoder.print("%s:0x%x->0x%x ", dstName.items, oldValue, newValue);
		decoder.printIP(" flags:");
		decoder.printlnFlags(oldFlags);
	} else {
		decoder.println(ErrorComment_InvalidInstructionTypeOrder(inst));
	}
}


constexpr Common_Format formatList[] = {
	{
		.type = Inst_add,
		.three = {
			.firstByteLiteral = {[0]=0b000000, [1]=0b100000, [2]=0b0000010},
			.format1Literal = 0b000,
			.has_S_on_fmt1 = true,
		},
	},
	{
		.type = Inst_sub,
		.three = {
			.firstByteLiteral = {[0]=0b001010, [1]=0b100000, [2]=0b0010110},
			.format1Literal = 0b101,
			.has_S_on_fmt1 = true,
		},
	},
	{
		.type = Inst_cmp,
		.three = {
			.firstByteLiteral = {[0]=0b001110, [1]=0b100000, [2]=0b0011110},
			.format1Literal = 0b111,
			.has_S_on_fmt1 = true,
		},
	},
	{
		.type = Inst_lea,
		.one = { .literal = 0b10001101, .shift = 0},
		.hasOnlyOneVariant = true,
	},
	{
		.type = Inst_mul,
		.one = { .literal = 0b1111011, .literal3bit = 0b100, .shift = 1, .hasOnlyOneOperand = true  },
		.hasOnlyOneVariant = true,
	},
	{
		.type = Inst_imul,
		.one = { .literal = 0b1111011, .literal3bit = 0b101, .shift = 1, .hasOnlyOneOperand = true  },
		.hasOnlyOneVariant = true,
	},
	{
		.type = Inst_div,
		.one = { .literal = 0b1111011, .literal3bit = 0b110, .shift = 1, .hasOnlyOneOperand = true  },
		.hasOnlyOneVariant = true,
	},
	{
		.type = Inst_div,
		.one = { .literal = 0b1111011, .literal3bit = 0b110, .shift = 1, .hasOnlyOneOperand = true  },
		.hasOnlyOneVariant = true,
	},
	{
		.type = Inst_idiv,
		.one = { .literal = 0b1111011, .literal3bit = 0b111, .shift = 1, .hasOnlyOneOperand = true  },
		.hasOnlyOneVariant = true,
	},
	{
		.type = Inst_shl,
		.one = { .literal = 0b110100, .literal3bit = 0b100, .shift = 2 },
		.hasOnlyOneVariant = true,
	},
	{
		.type = Inst_shr,
		.one = { .literal = 0b110100, .literal3bit = 0b101, .shift = 2 },
		.hasOnlyOneVariant = true,
	},
	{
		.type = Inst_sar,
		.one = { .literal = 0b110100, .literal3bit = 0b111, .shift = 2 },
		.hasOnlyOneVariant = true,
	},
	{
		.type = Inst_not,
		.one = { .literal = 0b1111011, .literal3bit = 0b010, .shift = 1, .hasOnlyOneOperand = true  },
		.hasOnlyOneVariant = true,
	},
	{
		.type = Inst_and,
		.three = {
			.firstByteLiteral = {[0]=0b001000, [1]=0b1000000, [2]=0b0010010},
			.format1Literal = 0b100,
			.has_S_on_fmt1 = false,
		},
	},
	{
		.type = Inst_test,
		.three = {
			.firstByteLiteral = {[0]=0b000100, [1]=0b1111011, [2]=0b1010100},
			.format1Literal = 0b000,
			.has_S_on_fmt1 = false,
		},
	},
	{
		.type = Inst_or,
		.three = {
			.firstByteLiteral = {[0]=0b000010, [1]=0b1000000, [2]=0b0000110},
			.format1Literal = 0b001,
			.has_S_on_fmt1 = false,
		},
	},
	{
		.type = Inst_xor,
		.three = {
			.firstByteLiteral = {[0]=0b001100, [1]=0b0011010, [2]=0b0011010},
			.format1Literal = 0b110,
			.has_S_on_fmt1 = false,
		},
	},
};

constexpr const char* descriptionTable[] = {
	[0] = "Reg/memory with register to either",
	[1] = "Immediate to register/memory",
	[2] = "Immediate to accumulator",
};

static Instruction decodeOneVariant(Decoder_Context &decoder, u8 &byte, Common_Format const& format) {
	assertTrue(format.hasOnlyOneVariant);
	bool const V = (byte >> 1) & 1;
	bool const W = (byte)      & 1;
	decoder.advance(byte);

	u8 const MOD = (byte >> 6) & 0b11;
	u8 const REG = (byte >> 3) & 0b111;
	u8 const R_M = (byte)      & 0b111;
	assertTrue(is_MOD_Valid(MOD));
	assertTrue(is_REG_Valid(REG));
	assertTrue(is_R_M_Valid(R_M));

	bool const has_W = format.one.shift == 1;
	bool const has_V = format.one.shift == 2;
	bool const has_src = !format.one.hasOnlyOneOperand;

	Instruction inst = {.type = format.type};

	if (has_src) {
		if (has_V) {
			inst.src = !V ? InstOpImmediate(false, 1) : REG_Table[0b001][0];
		} else {
			inst.src = REG_Table[REG][W];
		}
	}
	if (is_MOD_Register_Mode(MOD)) {
		inst.dst = REG_Table[R_M][W];
	} else {
		u16 const displacement = decoder.advanceDisplacement(MOD, R_M, byte);
		inst.dst = InstOpEffectiveAddress(MOD, R_M, W, displacement);
	}

	if (format.type == Inst_lea) SwapInstructionOperands(inst);

	decoder.printInst(inst);
	if (!decoder.exec) {
		decoder.print("(");
		if (has_V) decoder.print("V:%d ", V);
		if (has_W) decoder.print("W:%d ", W);
		decoder.printMOD(MOD, ' ');
		decoder.printREG(REG, ' ');
		decoder.printR_M(R_M, ' ');
		decoder.print(") <- ");
		decoder.printByteStack();
	}
	return inst;
}

static Instruction decodeFormat0(Decoder_Context &decoder, u8 &byte, Common_Format const& format) {
	assertTrue(!format.hasOnlyOneVariant);
    // Reg/memory with register to either
	bool const D = (byte >> 1) & 1;
	bool const W = (byte)      & 1;
	decoder.advance(byte);

	u8 const MOD = (byte >> 6) & 0b11;
	u8 const REG = (byte >> 3) & 0b111;
	u8 const R_M = (byte)      & 0b111;
	assertTrue(is_MOD_Valid(MOD));
	assertTrue(is_REG_Valid(REG));
	assertTrue(is_R_M_Valid(R_M));

	Instruction inst = { .type = format.type };

	// (D = 0) REG is the source.
	inst.src = REG_Table[REG][W];
	if (is_MOD_Register_Mode(MOD)) {
		inst.dst = REG_Table[R_M][W];
	} else {
		u16 const displacement = decoder.advanceDisplacement(MOD, R_M, byte);
		inst.dst = InstOpEffectiveAddress(MOD, R_M, W, displacement);
	}

	if (D) { // (D = 1) REG is the destination.
		SwapInstructionOperands(inst);
	}

	decoder.printInst(inst);
	if (!decoder.exec) {
		decoder.print("(D:%d, W:%d, ", D, W);
		decoder.printMOD(MOD, ' ');
		decoder.printREG(REG, ' ');
		decoder.printR_M(R_M, ')');
		decoder.print(" <- ");
		decoder.printByteStack();
	}
	return inst;
}

static Instruction decodeFormat1(Decoder_Context &decoder, u8 &byte, Common_Format const& format) {
	assertTrue(!format.hasOnlyOneVariant);
    // Immediate to register/memory
	bool const S = (byte >> 1) & 1;
	bool const W = (byte)      & 1;
	decoder.advance(byte);

	u8 const MOD = (byte >> 6) & 0b11;
	u8 const R_M = (byte)      & 0b111;
	assertTrue(is_MOD_Valid(MOD));
	assertTrue(is_R_M_Valid(R_M));

	bool const is_immediate_wide = format.three.has_S_on_fmt1 ? (!S && W) : W;

	Instruction inst = { .type = format.type };

	// (D = 0) REG is the source.
	if (is_MOD_Register_Mode(MOD)) {
		u16 const immediate = decoder.advance8or16Bits(is_immediate_wide, byte);
		inst.src = InstOpImmediate(is_immediate_wide, immediate);
		inst.dst = REG_Table[R_M][W];
	} else {
		u16 const displacement = decoder.advanceDisplacement(MOD, R_M, byte);
		u16 const immediate = decoder.advance8or16Bits(is_immediate_wide, byte);
		inst.src = InstOpImmediate(is_immediate_wide, immediate);
		inst.dst = InstOpEffectiveAddress(MOD, R_M, W, displacement);
	}

	decoder.printInst(inst);
	if (!decoder.exec) {
		decoder.print("(W:%d, ", W);
		decoder.printMOD(MOD, ' ');
		decoder.printR_M(R_M, ')');
		decoder.print(" <- ");
		decoder.printByteStack();
	}
	return inst;
}

static Instruction decodeFormat2(Decoder_Context &decoder, u8 &byte, Common_Format const& format) {
	assertTrue(!format.hasOnlyOneVariant);
    // Immediate to accumulator
	bool const W = byte & 1;

	u16 const data = decoder.advance8or16Bits(W, byte);

	Instruction const inst = {
		.dst = REG_Table[RegToID(Register::a)][W],
		.src = InstOpImmediate(W, data),
		.type = format.type,
	};

	decoder.printInst(inst);
	if (!decoder.exec) {
		decoder.print("(W:%d) <- ", W);
		decoder.printByteStack();
	}
	return inst;
}

static bool does_1_variant_match(Decoder_Context &decoder, u8 &byte, Common_Format const& format) {
	assertTrue(format.hasOnlyOneVariant);
	if (format.one.shift == 0) {
		return format.one.literal == byte;
	}
	if ((byte >> format.one.shift) != format.one.literal) return false;

	decoder.advance(byte);
	u8 const literal = (byte >> 3) & 0b111;
	bool const is = (literal == format.one.literal3bit);
	decoder.backOne(byte);
	return is;
}

static i8 which_3_variant_fmt(Decoder_Context &decoder, u8 &byte, Common_Format const& format) {
	assertTrue(!format.hasOnlyOneVariant);
	auto const byte1lit = format.three.firstByteLiteral;
	u8 const fmt1_shift = format.three.has_S_on_fmt1 ? 2 : 1;

	if ((byte >> 2) == byte1lit[0]) {
		return 0;
	} else if ((byte >> fmt1_shift) == byte1lit[1]) {
		decoder.advance(byte);
		u8 const literal = (byte >> 3) & 0b111;
		bool const is = (literal == format.three.format1Literal);
		decoder.backOne(byte);
		if (is) return 1;
	} else if ((byte >> 1) == byte1lit[2]) {
		return 2;
	}
	return -1;
}

bool isByte_common_inst(Decoder_Context &decoder, u8 &byte) {
	for (const auto& format : formatList) {
		if (format.hasOnlyOneVariant) {
			if (does_1_variant_match(decoder, byte, format)) return true;
		} else {
			if (which_3_variant_fmt(decoder, byte, format) != -1) return true;
		}
	}
	return false;
}

void decode_common_inst(Decoder_Context &decoder, u8 &byte) {
	assertTrue(isByte_common_inst(decoder, byte));
	Instruction inst = {.type = Inst_None };
	for (auto const& format : formatList) {
		if (format.hasOnlyOneVariant) {
			if (does_1_variant_match(decoder, byte, format)) {
				inst = decodeOneVariant(decoder, byte, format);
				goto done;
			}
		} else {
			switch (which_3_variant_fmt(decoder, byte, format)) {
				case 0: inst = decodeFormat0(decoder, byte, format); goto done;
				case 1: inst = decodeFormat1(decoder, byte, format); goto done;
				case 2: inst = decodeFormat2(decoder, byte, format); goto done;
				default:;
			}
		}
	}
	done:;
	if (decoder.exec) {
		exec_common_inst(decoder, inst);
	}
}
