#include "add_sub_cmp.h"
#include "string_builder.h"

struct Common_Format {
	Instruction_Type type;
	u8 firstByteLiteral[3];
	u8 format1Literal;
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

static u16 execOp(Instruction_Type const inst_type, u16 const A, u16 const B) {
	u16 result;
	switch (inst_type) {
		case Inst_add:
			result = A + B;
			setFlagsFromResult(result);
			return result;
		case Inst_sub:
			result = A - B;
			setFlagsFromResult(result);
			return result;
		case Inst_cmp:
			result = A - B;
			setFlagsFromResult(result);
			return A;
		default: unreachable();
	}
}

void exec_ADD_SUB_CMP(Decoder_Context const& decoder, Instruction const& inst) {
    if (IsBinaryInstTypeOrderValid(inst)) {
    	String_Builder dstName = getInstOpName(inst.dst);
    	defer(dstName.destroy());

		u16 const oldFlags = FlagsRegister::get();
        u16 const oldValue = getInstOpValue(inst.dst);
		u16 const newValue = execOp(inst.type, oldValue, getInstOpValue(inst.src));
        setInstOpValue(inst.dst, newValue);
        decoder.print("%s:0x%x->0x%x ", dstName.items, oldValue, newValue);
		decoder.printIP(" flags:");
		decoder.printlnFlags(oldFlags);
	} else {
		decoder.println(ErrorComment_InvalidInstructionTypeOrder(inst));
	}
}

constexpr Common_Format FMT_ADD = {
	Inst_add,
	{[0]=0b000000, [1]=0b100000, [2]=0b0000010},
	0b000,
};

constexpr Common_Format FMT_SUB = {
	Inst_sub,
	{[0]=0b001010, [1]=0b100000, [2]=0b0010110},
	0b101,
};

constexpr Common_Format FMT_CMP = {
	Inst_cmp,
	{[0]=0b001110, [1]=0b100000, [2]=0b0011110},
	0b111,
};

constexpr Common_Format formatTable[] = {FMT_ADD, FMT_SUB, FMT_CMP};

constexpr const char* descriptionTable[] = {
	[0] = "Reg/memory with register to either",
	[1] = "Immediate to register/memory",
	[2] = "Immediate to accumulator",
};

static void decodeFormat0(Decoder_Context &decoder, u8 &byte, Common_Format const& format) {
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
	if (decoder.exec) {
		exec_ADD_SUB_CMP(decoder, inst);
	} else {
		decoder.print("(D:%d, W:%d, ", D, W);
		decoder.printMOD(MOD, ' ');
		decoder.printREG(REG, ' ');
		decoder.printR_M(R_M, ')');
		decoder.print(" <- ");
		decoder.printByteStack();
	}
}

static void decodeFormat1(Decoder_Context &decoder, u8 &byte, Common_Format const& format) {
    // Immediate to register/memory
	bool const S = (byte >> 1) & 1;
	bool const W = (byte)      & 1;
	decoder.advance(byte);

	u8 const MOD = (byte >> 6) & 0b11;
	u8 const R_M = (byte)      & 0b111;
	assertTrue(is_MOD_Valid(MOD));
	assertTrue(is_R_M_Valid(R_M));

	Instruction inst = { .type = format.type };

	// (D = 0) REG is the source.
	if (is_MOD_Register_Mode(MOD)) {
		u16 const immediate = decoder.advance8or16Bits(!S && W, byte);
		inst.src = InstOpImmediate(!S && W, immediate);
		inst.dst = REG_Table[R_M][W];
	} else {
		u16 const displacement = decoder.advanceDisplacement(MOD, R_M, byte);
		u16 const immediate = decoder.advance8or16Bits(!S && W, byte);
		inst.src = InstOpImmediate(!S && W, immediate);
		inst.dst = InstOpEffectiveAddress(MOD, R_M, W, displacement);
	}

	decoder.printInst(inst);
	if (decoder.exec) {
		exec_ADD_SUB_CMP(decoder, inst);
	} else {
		decoder.print("(W:%d, ", W);
		decoder.printMOD(MOD, ' ');
		decoder.printR_M(R_M, ')');
		decoder.print(" <- ");
		decoder.printByteStack();
	}
}

static void decodeFormat2(Decoder_Context &decoder, u8 &byte, Common_Format const& format) {
    // Immediate to accumulator
	bool const W = byte & 1;

	u16 const data = decoder.advance8or16Bits(W, byte);

	Instruction const inst = {
		.dst = REG_Table[RegToID(Register::a)][W],
		.src = InstOpImmediate(W, data),
		.type = format.type,
	};

	decoder.printInst(inst);
	if (decoder.exec) {
		exec_ADD_SUB_CMP(decoder, inst);
	} else {
		decoder.print("(W:%d) <- ", W);
		decoder.printByteStack();
	}
}

bool isByte_ADD_SUB_CMP(Decoder_Context &decoder, u8 &byte) {
	for (const auto& [_, firstByteLiteral, format1Literal] : formatTable) {
		if (byte >> 2 == firstByteLiteral[0] ||
			byte >> 1 == firstByteLiteral[2]) {
			return true;
		}
		if (byte >> 2 == firstByteLiteral[1]) {
			decoder.advance(byte);
			u8 const literal = (byte >> 3) & 0b111;
			bool const is = (literal == format1Literal);
			decoder.backOne(byte);
			if (is) {
				return true;
			}
		}
	}
	return false;
}

void decode_ADD_SUB_CMP(Decoder_Context &decoder, u8 &byte) {
	assertTrue(isByte_ADD_SUB_CMP(decoder, byte));
	for (auto const& format: formatTable) {
		if (byte >> 2 == format.firstByteLiteral[0]) {
			decodeFormat0(decoder, byte, format);
			return;
		}
		if (byte >> 2 == format.firstByteLiteral[1]) {
			decoder.advance(byte);
			u8 const literal = (byte >> 3) & 0b111;
			bool const is = (literal == format.format1Literal);
			decoder.backOne(byte);
			if (is) {
				decodeFormat1(decoder, byte, format);
				return;
			}
		}
		if (byte >> 1 == format.firstByteLiteral[2]) {
			decodeFormat2(decoder, byte, format);
			return;
		}
	}
	unreachable();
}
