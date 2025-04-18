#include "add_sub_cmp.h"
#include "string_builder.h"

/*
Homework:
	- All 3 versions of ADD
	- All 3 versions of SUB
	- All 3 versions of CMP
*/

enum struct Type { add, sub, cmp };

struct Common_Format {
	Type type;
	u8 firstByteLiteral[3];
	u8 format1Literal;
};

static const char* getMnemonic(Common_Format const& format) {
	switch (format.type) {
		case Type::add: return "add";
		case Type::sub: return "sub";
		case Type::cmp: return "cmp";
		default: unreachable();
	}
}

static force_inline u8 Count1s(u8 const byte) {
	return ((byte >> 0) & 0b1) + ((byte >> 1) & 0b1) +
           ((byte >> 2) & 0b1) + ((byte >> 3) & 0b1) +
           ((byte >> 4) & 0b1) + ((byte >> 5) & 0b1) +
	       ((byte >> 6) & 0b1) + ((byte >> 7) & 0b1);
}

static force_inline void setFlagsFromResult(u16 const result) {
	using namespace FlagsRegister;
	setBit(Bit::ZF, result == 0);
	setBit(Bit::PF, Count1s(result & 0xFF) % 2 == 0);
	setBit(Bit::SF, (result >> 15) & 0b1);
}

static u16 execOp(Common_Format const& format, u16 const A, u16 const B) {
	u16 result;
	switch (format.type) {
		case Type::add:
			result = A + B;
			setFlagsFromResult(result);
			return result;
		case Type::sub:
			result = A - B;
			setFlagsFromResult(result);
			return result;
		case Type::cmp:
			result = A - B;
			setFlagsFromResult(result);
			return A;
		default: unreachable();
	}
}

void exec_ADD_SUB_CMP(
	Decoder_Context const& decoder,
	Instruction_Operand const& dst,
	Instruction_Operand const& src,
	Common_Format const& format)
{
    if (IsBinaryInstTypeOrderValid(dst, src)) {
    	String_Builder dstName = getInstOpName(dst); defer(dstName.destroy());
		u16 const oldFlags = FlagsRegister::get();
        u16 const oldValue = getInstOpValue(dst);
		u16 const newValue = execOp(format, oldValue, getInstOpValue(src));
        setInstOpValue(dst, newValue);
        decoder.print("; %s:0x%x->0x%x ", dstName.items, oldValue, newValue);
		decoder.printIP(" flags:");
		decoder.printlnFlags(oldFlags);
	} else {
        decoder.println("; " LOG_ERROR_STRING ": `%s <%s> <%s>` is an invalid order of types.",
        	getMnemonic(format),
        	Instruction_Operand_Type_Names[static_cast<u8>(dst.type)],
        	Instruction_Operand_Type_Names[static_cast<u8>(src.type)]);
	}
}

constexpr Common_Format FMT_ADD = {
	Type::add,
	{[0]=0b000000, [1]=0b100000, [2]=0b0000010},
	0b000,
};

constexpr Common_Format FMT_SUB = {
	Type::sub,
	{[0]=0b001010, [1]=0b100000, [2]=0b0010110},
	0b101,
};

constexpr Common_Format FMT_CMP = {
	Type::cmp,
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

	Instruction_Operand src = {}, dst = {};

	// Memory Mode (if R/M=110 then 16-bit displacement, otherwise no displacement).
	if (MOD == 0b00) {
        i16 const displacement = (R_M == 0b110) ? signExtendWord(decoder.advance16Bits(byte)) : signExtendWord(0);

		// (D = 0) REG is the source.
        src = REG_Table[REG][W];
        dst.type = Instruction_Operand_Type::EffectiveAddress;
        dst.address.base = (R_M == 0b110) ? EffectiveAddress::Base::Direct : Effective_Address_Table[R_M];
        dst.address.displacement = makeImmediateWord(displacement);
		dst.address.wide = W;
	}
	// Memory Mode (8-bit displacement)
	else if (MOD == 0b01) {
		i8 const displacement = signExtendByte(decoder.advance08Bits(byte));

		// (D = 0) REG is the source.
        src = REG_Table[REG][W];
        dst.type = Instruction_Operand_Type::EffectiveAddress;
        dst.address.base = Effective_Address_Table[R_M];
        dst.address.displacement = makeImmediateByte(displacement);
		dst.address.wide = W;
	}
	// Memory Mode (16-bit displacement)
	else if (MOD == 0b10) {
		i16 const displacement = signExtendWord(decoder.advance16Bits(byte));

		// (D = 0) REG is the source.
        src = REG_Table[REG][W];
        dst.type = Instruction_Operand_Type::EffectiveAddress;
        dst.address.base = Effective_Address_Table[R_M];
        dst.address.displacement = makeImmediateWord(displacement);
		dst.address.wide = W;
	}
	// Register Mode (no displacement)
	else if (MOD == 0b11) {
		// (D = 0) REG is the source.
        dst = REG_Table[R_M][W];
        src = REG_Table[REG][W];
	}

	if (D) { // (D = 1) REG is the destination.
		Swap(Instruction_Operand, src, dst);
	}

	decoder.printInst(getMnemonic(format), dst, src);
	if (decoder.exec) {
		exec_ADD_SUB_CMP(decoder, dst, src, format);
	} else {
		decoder.print("; (D:%d, W:%d, ", D, W);
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

	Instruction_Operand src = {}, dst = {};

	// Memory Mode (if R/M=110 then 16-bit displacement, otherwise no displacement).
	if (MOD == 0b00) {
        i16 const displacement = (R_M == 0b110) ? signExtendWord(decoder.advance16Bits(byte)) : signExtendWord(0);
		u16 const immediate = decoder.advance8or16Bits(!S && W, byte);

		src = InstImmediate(!S && W, immediate);
        dst.type = Instruction_Operand_Type::EffectiveAddress;
		dst.prefix = InstPrefix(W);
        dst.address.base = Effective_Address_Table[R_M];
        dst.address.displacement = makeImmediateWord(displacement);
		dst.address.wide = W;
	}
	// Memory Mode (8-bit displacement)
	else if (MOD == 0b01) {
		i8 const displacement = signExtendByte(decoder.advance08Bits(byte));
		u16 const immediate = decoder.advance8or16Bits(!S && W, byte);

		src = InstImmediate(!S && W, immediate);
        dst.type = Instruction_Operand_Type::EffectiveAddress;
		dst.prefix = InstPrefix(W);
        dst.address.base = Effective_Address_Table[R_M];
        dst.address.displacement = makeImmediateByte(displacement);
		dst.address.wide = W;
	}
	// Memory Mode (16-bit displacement)
	else if (MOD == 0b10) {
		i16 const displacement = signExtendWord(decoder.advance16Bits(byte));
		u16 const immediate = decoder.advance8or16Bits(!S && W, byte);

		src = InstImmediate(!S && W, immediate);
        dst.type = Instruction_Operand_Type::EffectiveAddress;
		dst.prefix = InstPrefix(W);
        dst.address.base = Effective_Address_Table[R_M];
        dst.address.displacement = makeImmediateWord(displacement);
		dst.address.wide = W;
	}
	// Register Mode (no displacement)
	else if (MOD == 0b11) {
		u16 const immediate = decoder.advance8or16Bits(!S && W, byte);
		src = InstImmediate(!S && W, immediate);
		dst = REG_Table[R_M][W];
	}

	decoder.printInst(getMnemonic(format), dst, src);
	if (decoder.exec) {
		exec_ADD_SUB_CMP(decoder, dst, src, format);
	} else {
		decoder.print("; (W:%d, ", W);
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

	Instruction_Operand const src = InstImmediate(W, data);
	Instruction_Operand const dst = REG_Table[RegToID(Register::a)][W];

	decoder.printInst(getMnemonic(format), dst, src);
	if (decoder.exec) {
		exec_ADD_SUB_CMP(decoder, dst, src, format);
	} else {
		decoder.print("; (W:%d) <- ", W);
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
