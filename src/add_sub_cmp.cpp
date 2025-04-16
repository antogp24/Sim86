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
	bool const D = (byte >> 1) & 1;
	bool const W = (byte)      & 1;
	decoder.advance(byte);

	u8 const MOD = (byte >> 6) & 0b11;
	u8 const REG = (byte >> 3) & 0b111;
	u8 const R_M = (byte)      & 0b111;

	String_Builder src = string_builder_make();
	String_Builder dst = string_builder_make();
	defer(src.destroy());
	defer(dst.destroy());

	// Memory Mode (if R/M=110 then 16-bit displacement, otherwise no displacement).
	if (MOD == 0b00) {
		char direct_address[U16_STR_SIZE_BASE10] = {0};
		if (R_M == 0b110) {
			u16 const address = decoder.advance16Bits(byte);
			snprintf(direct_address, sizeof(direct_address), "%" PRIu16, address);
		}

		// (D = 0) REG is the source.
		src.append(REG_TABLE[REG][W]);
		dst.append("[");
		dst.append(R_M != 0b110 ? EFFECTIVE_ADDRESS_TABLE[R_M] : direct_address);
		dst.append(']');
	}
	// Memory Mode (8-bit displacement)
	else if (MOD == 0b01) {
		u8 const displacement = decoder.advance08Bits(byte);

		// (D = 0) REG is the source.
		src.append(REG_TABLE[REG][W]);
		dst.append("[");
		dst.append(EFFECTIVE_ADDRESS_TABLE[R_M]);
		if (displacement > 0) {
			dst.append(" + ");
			dst.append(displacement);
		}
		dst.append(']');
	}
	// Memory Mode (16-bit displacement)
	else if (MOD == 0b10) {
		u16 const displacement = decoder.advance16Bits(byte);

		// (D = 0) REG is the source.
		src.append(REG_TABLE[REG][W]);
		dst.append("[");
		dst.append(EFFECTIVE_ADDRESS_TABLE[R_M]);
		if (displacement > 0) {
			dst.append(" + ");
			dst.append(displacement);
		}
		dst.append(']');
	}
	// Register Mode (no displacement)
	else if (MOD == 0b11) {
		// (D = 0) REG is the source.
		dst.append(REG_TABLE[R_M][W]);
		src.append(REG_TABLE[REG][W]);
	}

	if (D) { // (D = 1) REG is the destination.
		Swap(String_Builder, src, dst);
	}

	decoder.printInst(getMnemonic(format), dst.items, src.items);
	if (decoder.exec) {
		if (dst.items[0] == '[' || src.items[0] == '[') {
			decoder.println("; " LOG_ERROR_STRING ": Memory Expressions are Unimplemented.");
		} else {
			auto const dstReg = getRegisterInfo(dst.items);
			auto const srcReg = getRegisterInfo(src.items);
			u16 const oldDstValue = getRegisterValue(dstReg);
			u16 const srcValue = getRegisterValue(srcReg);
			u16 const oldFlags = FlagsRegister::get();
			u16 const result = execOp(format, oldDstValue, srcValue);
			setRegisterValue(dstReg, result);
			decoder.print("; %s:0x%x->0x%x ", dst.items, oldDstValue, result);
			decoder.printIP(" flags:");
			decoder.printFlagsLN(oldFlags);
		}
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
	bool const S = (byte >> 1) & 1;
	bool const W = (byte)      & 1;
	decoder.advance(byte);

	u8 const MOD = (byte >> 6) & 0b11;
	u8 const R_M = (byte)      & 0b111;

	String_Builder src = string_builder_make();
	String_Builder dst = string_builder_make();
	defer(src.destroy());
	defer(dst.destroy());

	// Memory Mode (if R/M=110 then 16-bit displacement, otherwise no displacement).
	if (MOD == 0b00) {
		char direct_address[U16_STR_SIZE_BASE10] = {0};
		if (R_M == 0b110) {
			u16 const address = decoder.advance16Bits(byte);
			snprintf(direct_address, sizeof(direct_address), "%" PRIu16, address);
		}

		u16 const immediate = decoder.advance8or16Bits(!S && W, byte);

		S ? src.append(signExtendByte(immediate)) : src.append(immediate);
		dst.append(W ? "word " : "byte ");
		dst.append("[");
		dst.append(R_M != 0b110 ? EFFECTIVE_ADDRESS_TABLE[R_M] : direct_address);
		dst.append(']');
	}
	// Memory Mode (8-bit displacement)
	else if (MOD == 0b01) {
		u8 const displacement = decoder.advance08Bits(byte);
		u16 const immediate = decoder.advance8or16Bits(!S && W, byte);

		S ? src.append(signExtendByte(immediate)) : src.append(immediate);
		dst.append(W ? "word " : "byte ");
		dst.append("[");
		dst.append(EFFECTIVE_ADDRESS_TABLE[R_M]);
		if (displacement > 0) {
			dst.append(" + ");
			dst.append(displacement);
		}
		dst.append(']');
	}
	// Memory Mode (16-bit displacement)
	else if (MOD == 0b10) {
		u16 const displacement = decoder.advance16Bits(byte);
		u16 const immediate = decoder.advance8or16Bits(!S && W, byte);

		S ? src.append(signExtendByte(immediate)) : src.append(immediate);
		dst.append(W ? "word " : "byte ");
		dst.append("[");
		dst.append(EFFECTIVE_ADDRESS_TABLE[R_M]);
		if (displacement > 0) {
			dst.append(" + ");
			dst.append(displacement);
		}
		dst.append(']');
	}
	// Register Mode (no displacement)
	else if (MOD == 0b11) {
		u16 const immediate = decoder.advance8or16Bits(!S && W, byte);
		S ? src.append(signExtendByte(immediate)) : src.append(immediate);
		dst.append(REG_TABLE[R_M][W]);
	}

	decoder.printInst(getMnemonic(format), dst.items, src.items);
	if (decoder.exec) {
		if (dst.items[0] == '[' || src.items[0] == '[') {
			decoder.println("; " LOG_ERROR_STRING ": Memory Expressions are Unimplemented.");
		} else {
			auto const dstReg = getRegisterInfo(dst.items);
			u16 const oldDstValue = getRegisterValue(dstReg);
			u16 const srcValue = strtol(src.items, nullptr, 10);
			u16 const oldFlags = FlagsRegister::get();
			u16 const result = execOp(format, oldDstValue, srcValue);
			setRegisterValue(dstReg, result);
			decoder.print("; %s:0x%x->0x%x ", dst.items, oldDstValue, result);
			decoder.printIP(" flags:");
			decoder.printFlagsLN(oldFlags);
		}
	} else {
		decoder.print("; (W:%d, ", W);
		decoder.printMOD(MOD, ' ');
		decoder.printR_M(R_M, ')');
		decoder.print(" <- ");
		decoder.printByteStack();
	}
}

static void decodeFormat2(Decoder_Context &decoder, u8 &byte, Common_Format const& format) {
	bool const W = byte & 1;

	u16 const data = decoder.advance8or16Bits(W, byte);

	String_Builder immediate = string_builder_make();
	W ? immediate.append(signExtendWord(data)) : immediate.append(signExtendByte(data));
	const char* accumulator = REG_TABLE[0b000][W];

	decoder.printInst(getMnemonic(format), accumulator, immediate.items);
	decoder.print("; (W:%d) <- ", W);
	decoder.printByteStack();
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
		} else if (byte >> 2 == format.firstByteLiteral[1]) {
			decoder.advance(byte);
			u8 const literal = (byte >> 3) & 0b111;
			bool const is = (literal == format.format1Literal);
			decoder.backOne(byte);
			if (is) {
				decodeFormat1(decoder, byte, format);
				return;
			}
		} else if (byte >> 1 == format.firstByteLiteral[2]) {
			decodeFormat2(decoder, byte, format);
			return;
		}
	}
	unreachable();
}
