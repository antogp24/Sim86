#include "mov.h"
#include "string_builder.h"

bool isByte_MOV(u8 const byte) {
	bool const result = (byte >> 2 == 0b100010)  ||
						(byte >> 1 == 0b1100011) ||
						(byte >> 4 == 0b1011)    ||
						(byte >> 2 == 0b101000)  ||
						(byte == 0b10001110 || byte == 0b10001100);
	return result;
}

void exec_MOV(Decoder_Context const& decoder, Instruction_Operand const& dst, Instruction_Operand const& src) {
    if (IsBinaryInstTypeOrderValid(dst, src)) {
    	String_Builder dstName = getInstOpName(dst); defer(dstName.destroy());
        u16 const oldValue = getInstOpValue(dst);
        u16 const newValue = getInstOpValue(src);
        setInstOpValue(dst, newValue);
        decoder.print("; %s:0x%x->0x%x ", dstName.items, oldValue, newValue);
        decoder.printlnIP();
	} else {
        decoder.println("; " LOG_ERROR_STRING ": `%s <%s> <%s>` is an invalid order of types.",
        	"mov",
        	Instruction_Operand_Type_Names[static_cast<u8>(dst.type)],
        	Instruction_Operand_Type_Names[static_cast<u8>(src.type)]);
	}
}

void decode_MOV(Decoder_Context& decoder, u8& byte) {
	assertTrue(isByte_MOV(byte));

	Instruction_Operand src = {}, dst = {};

	// MOV: 1. Register/memory to/from register.
	if (byte >> 2 == 0b100010) {
		bool const D = (byte >> 1) & 1;
		bool const W = (byte)      & 1;

		decoder.advance(byte);
		u8 const MOD = (byte >> 6) & 0b11;
		u8 const REG = (byte >> 3) & 0b111;
		u8 const R_M = (byte)      & 0b111;

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

		decoder.printInst("mov", dst, src);
		if (!decoder.exec) {
            decoder.print("; (D:%d, W:%d, ", D, W);
            decoder.printMOD(MOD, ' ');
            decoder.printREG(REG, ' ');
            decoder.printR_M(R_M, ')');
            decoder.print(" <- ");
            decoder.printByteStack();
		}
	}
	// MOV: 2. Immediate to register/memory.
	else if (byte >> 1 == 0b1100011) {
		bool const W = byte & 1;

		decoder.advance(byte);
		u8 const R_M = (byte)      & 0b111;
		u8 const MOD = (byte >> 6) & 0b11;

		// Memory Mode (if R/M=110 then 16-bit displacement, otherwise no displacement).
		if (MOD == 0b00) {
            i16 const displacement = (R_M == 0b110) ? signExtendWord(decoder.advance16Bits(byte)) : signExtendWord(0);
			u16 const immediate = decoder.advance8or16Bits(W, byte);

			src = InstImmediatePrefixed(W, immediate);
			dst.type = Instruction_Operand_Type::EffectiveAddress;
			dst.address.base = (R_M == 0b110) ? EffectiveAddress::Base::Direct : Effective_Address_Table[R_M];
			dst.address.displacement = makeImmediateWord(displacement);
			dst.address.wide = W;
		}
		// Memory Mode (8-bit displacement)
		else if (MOD == 0b01) {
			i8 const displacement = signExtendByte(decoder.advance08Bits(byte));
			u16 const immediate = decoder.advance8or16Bits(W, byte);

			src = InstImmediatePrefixed(W, immediate);
			dst.type = Instruction_Operand_Type::EffectiveAddress;
			dst.address.base = Effective_Address_Table[R_M];
			dst.address.displacement = makeImmediateByte(displacement);
			dst.address.wide = W;
		}
		// Memory Mode (16-bit displacement)
		else if (MOD == 0b10) {
			i16 const displacement = signExtendWord(decoder.advance16Bits(byte));
			u16 const immediate = decoder.advance8or16Bits(W, byte);

			src = InstImmediatePrefixed(W, immediate);
			dst.type = Instruction_Operand_Type::EffectiveAddress;
			dst.address.base = Effective_Address_Table[R_M];
			dst.address.displacement = makeImmediateWord(displacement);
			dst.address.wide = W;
		}
		// Register Mode (no displacement)
		else if (MOD == 0b11) {
			u16 const immediate = decoder.advance8or16Bits(W, byte);
			src = InstImmediate(W, immediate);
			dst = REG_Table[R_M][W];
		}

		decoder.printInst("mov", dst, src);
		if (!decoder.exec) {
            decoder.print("; (W:%d, ", W);
            decoder.printMOD(MOD, ' ');
            decoder.printR_M(R_M, ')');
            decoder.print(" <- ");
            decoder.printByteStack();
		}
	}
	// MOV: 3. Immediate to register.
	else if (byte >> 4 == 0b1011) {
		bool const W = (byte >> 3) & 1;
		u8 const REG = byte & 0b111;
		u16 const data = decoder.advance8or16Bits(W, byte);

		dst = REG_Table[REG][W];
		src = InstImmediate(W, data);

		decoder.printInst("mov", dst, src);
		if (!decoder.exec) {
            decoder.print("; (W:%d, ", W);
            decoder.printREG(REG, ' ');
            decoder.print(" <- ");
            decoder.printByteStack();
		}
	}
	// MOV: 4. Memory to accumulator or Accumulator to memory.
	else if (byte >> 2 == 0b101000) {
		bool const D = (byte >> 1) & 1;
		bool const W = byte & 1;

		i16 const address = signExtendWord(decoder.advance16Bits(byte));

		// (D = 0) The address is the source.
		const char* description = "Memory to accumulator";
		src.type = Instruction_Operand_Type::EffectiveAddress;
		src.address.base = EffectiveAddress::Base::Direct;
		src.address.displacement = makeImmediateWord(address);
		dst = REG_Table[0b000][W];

		if (D) { // (D = 1) The address is the destination.
			description = "Accumulator to memory";
			Swap(Instruction_Operand, src, dst);
		}

		decoder.printInst("mov", dst, src);
		if (!decoder.exec) {
            decoder.print("; (%s, W:%d) <- ", description, W);
            decoder.printByteStack();
		}
	}
	// MOV: 5. Register/memory to segment register (Or vice versa)
	else if (byte == 0b10001110 || byte == 0b10001100) {
		bool const D = (byte >> 1) & 1;

		decoder.advance(byte);
		u8 const MOD = (byte >> 6) & 0b11;
		u8 const SR  = (byte >> 3) & 0b11;
		u8 const R_M = (byte)      & 0b111;

		// Memory Mode (if R/M=110 then 16-bit displacement, otherwise no displacement).
		if (MOD == 0b00) {
            i16 const displacement = (R_M == 0b110) ? signExtendWord(decoder.advance16Bits(byte)) : signExtendWord(0);

			// (D = 0) SR is the source.
			src = SR_Table[SR];
			dst.type = Instruction_Operand_Type::EffectiveAddress;
			dst.address.base = (R_M == 0b110) ? EffectiveAddress::Base::Direct : Effective_Address_Table[R_M];
			dst.address.displacement = makeImmediateWord(displacement);
			dst.address.wide = true;
		}
		// Memory Mode (8-bit displacement)
		else if (MOD == 0b01) {
			i8 const displacement = signExtendByte(decoder.advance08Bits(byte));

			// (D = 0) SR is the source.
			src = SR_Table[SR];
			dst.type = Instruction_Operand_Type::EffectiveAddress;
			dst.address.base = Effective_Address_Table[R_M];
			dst.address.displacement = makeImmediateByte(displacement);
			dst.address.wide = true;
		}
		// Memory Mode (16-bit displacement)
		else if (MOD == 0b10) {
			i16 const displacement = signExtendWord(decoder.advance16Bits(byte));

			// (D = 0) SR is the source.
			src = SR_Table[SR];
			dst.type = Instruction_Operand_Type::EffectiveAddress;
			dst.address.base = Effective_Address_Table[R_M];
			dst.address.displacement = makeImmediateWord(displacement);
			dst.address.wide = true;
		}
		// Register Mode (no displacement)
		else if (MOD == 0b11) {
			// (D = 0) SR is the source.
			dst = REG_Table[R_M][0];
			src = SR_Table[SR];
		}

		if (D) { // (D = 1) SR is the destination.
			Swap(Instruction_Operand, src, dst);
		}

		decoder.printInst("mov", dst, src);
		if (!decoder.exec) {
            decoder.print("; (D:%d, ", D);
            decoder.printMOD(MOD, ' ');
            decoder.printSR(SR, ' ');
            decoder.printR_M(R_M, ')');
            decoder.print(" <- ");
            decoder.printByteStack();
		}
	} else {
		unreachable();
	}

    if (decoder.exec) {
    	exec_MOV(decoder, dst, src);
    }
}
