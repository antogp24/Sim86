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

void decode_MOV(Decoder_Context& decoder, u8& byte) {
	assertTrue(isByte_MOV(byte));

	// MOV: 1. Register/memory to/from register.
	if (byte >> 2 == 0b100010) {
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
			i8 const displacement = signExtendByte(decoder.advance08Bits(byte));

			// (D = 0) REG is the source.
			src.append(REG_TABLE[REG][W]);
			dst.append("[");
			dst.append(EFFECTIVE_ADDRESS_TABLE[R_M]);
			if (displacement > 0) {
				dst.append(" + ");
				dst.append(displacement);
			} else if (displacement < 0) {
				dst.append(" - ");
				dst.append(-displacement);
			}
			dst.append(']');
		}
		// Memory Mode (16-bit displacement)
		else if (MOD == 0b10) {
			i16 const displacement = signExtendWord(decoder.advance16Bits(byte));

			// (D = 0) REG is the source.
			src.append(REG_TABLE[REG][W]);
			dst.append("[");
			dst.append(EFFECTIVE_ADDRESS_TABLE[R_M]);
			if (displacement > 0) {
				dst.append(" + ");
				dst.append(displacement);
			} else if (displacement < 0) {
				dst.append(" - ");
				dst.append(-displacement);
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

		decoder.printInst("mov", dst.items, src.items);
		if (decoder.exec) {
			if (dst.items[0] == '[' || src.items[0] == '[') {
				decoder.println("; " LOG_ERROR_STRING ": Memory Expressions are Unimplemented.");
			} else {
                auto const dstReg = getRegisterInfo(dst.items);
                auto const srcReg = getRegisterInfo(src.items);
                u16 const oldDstValue = getRegisterValue(dstReg);
                u16 const srcValue = getRegisterValue(srcReg);
                setRegisterValue(dstReg, srcValue);
                decoder.print("; %s:0x%x->0x%x ", dst.items, oldDstValue, srcValue);
				decoder.printIP("\n");
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
	// MOV: 2. Immediate to register/memory.
	else if (byte >> 1 == 0b1100011) {
		bool const W = byte & 1;

		decoder.advance(byte);
		u8 const R_M = (byte)      & 0b111;
		u8 const MOD = (byte >> 6) & 0b11;

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

			u16 const immediate = decoder.advance8or16Bits(W, byte);

			src.append(W ? "word " : "byte ");
			W ? src.append(signExtendWord(immediate)) : src.append(signExtendByte(immediate));
			dst.append("[");
			dst.append(R_M != 0b110 ? EFFECTIVE_ADDRESS_TABLE[R_M] : direct_address);
			dst.append(']');
		}
		// Memory Mode (8-bit displacement)
		else if (MOD == 0b01) {
			i8 const displacement = signExtendByte(decoder.advance08Bits(byte));
			u16 const immediate = decoder.advance8or16Bits(W, byte);

			src.append(W ? "word " : "byte ");
			W ? src.append(signExtendWord(immediate)) : src.append(signExtendByte(immediate));
			dst.append("[");
			dst.append(EFFECTIVE_ADDRESS_TABLE[R_M]);
			if (displacement > 0) {
				dst.append(" + ");
				dst.append(displacement);
			} else if (displacement < 0) {
				dst.append(" - ");
				dst.append(-displacement);
			}
			dst.append(']');
		}
		// Memory Mode (16-bit displacement)
		else if (MOD == 0b10) {
			i16 const displacement = signExtendWord(decoder.advance16Bits(byte));
			u16 const immediate = decoder.advance8or16Bits(W, byte);

			src.append(W ? "word " : "byte ");
			W ? src.append(signExtendWord(immediate)) : src.append(signExtendByte(immediate));
			dst.append("[");
			dst.append(EFFECTIVE_ADDRESS_TABLE[R_M]);
			if (displacement > 0) {
				dst.append(" + ");
				dst.append(displacement);
			} else if (displacement < 0) {
				dst.append(" - ");
				dst.append(-displacement);
			}
			dst.append(']');
		}
		// Register Mode (no displacement)
		else if (MOD == 0b11) {
			u16 const immediate = decoder.advance8or16Bits(W, byte);
			W ? src.append(signExtendWord(immediate)) : src.append(signExtendByte(immediate));
			dst.append(REG_TABLE[R_M][W]);
		}

		decoder.printInst("mov", dst.items, src.items);
		decoder.print("; (W:%d, ", W);
		decoder.printMOD(MOD, ' ');
		decoder.printR_M(R_M, ')');
		decoder.print(" <- ");
		decoder.printByteStack();
	}
	// MOV: 3. Immediate to register.
	else if (byte >> 4 == 0b1011) {
		bool const W = (byte >> 3) & 1;
		u8 const REG = byte & 0b111;
		const char* dst = REG_TABLE[REG][W];
		char src[U16_STR_SIZE_BASE10] = {0};

		u16 const data = decoder.advance8or16Bits(W, byte);
		snprintf(src, sizeof(src), "%" PRIu16, data);

		decoder.printInst("mov", dst, src);
		if (decoder.exec) {
			auto const dstReg = getRegisterInfo(dst);
            u16 const oldDstValue = getRegisterValue(dstReg);
			setRegisterValue(dstReg, data);
			decoder.print("; %s:0x%x->0x%x ", dst, oldDstValue, data);
			decoder.printIP("\n");
		} else {
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

		u16 const address = decoder.advance16Bits(byte);
		char address_string[U16_STR_SIZE_BASE10+StrLen("[]")] = {0};
		snprintf(address_string, sizeof(address_string), "[%" PRIu16"]", address);

		// (D = 0) The address is the source.
		const char* description = "Memory to accumulator";
		const char* src = address_string;
		const char* dst = const_cast<char*>(REG_TABLE[0b000][W]);

		if (D) { // (D = 1) The address is the destination.
			description = "Accumulator to memory";
			Swap(const char*, src, dst);
		}

		decoder.printInst("mov", dst, src);
        decoder.print("; (%s, W:%d) <- ", description, W);
        decoder.printByteStack();
	}
	// MOV: 5. Register/memory to segment register (Or vice versa)
	else if (byte == 0b10001110 || byte == 0b10001100) {
		bool const D = (byte >> 1) & 1;

		decoder.advance(byte);
		u8 const MOD = (byte >> 6) & 0b11;
		u8 const SR  = (byte >> 3) & 0b11;
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

			// (D = 0) SR is the source.
			src.append(SR_TABLE[SR]);
			dst.append("[");
			dst.append(R_M != 0b110 ? EFFECTIVE_ADDRESS_TABLE[R_M] : direct_address);
			dst.append(']');
		}
		// Memory Mode (8-bit displacement)
		else if (MOD == 0b01) {
			i8 const displacement = signExtendByte(decoder.advance08Bits(byte));

			// (D = 0) SR is the source.
			src.append(SR_TABLE[SR]);
			dst.append("[");
			dst.append(EFFECTIVE_ADDRESS_TABLE[R_M]);
			if (displacement > 0) {
				dst.append(" + ");
				dst.append(displacement);
			} else if (displacement < 0) {
				dst.append(" - ");
				dst.append(-displacement);
			}
			dst.append(']');
		}
		// Memory Mode (16-bit displacement)
		else if (MOD == 0b10) {
			i16 const displacement = signExtendWord(decoder.advance16Bits(byte));

			// (D = 0) SR is the source.
			src.append(SR_TABLE[SR]);
			dst.append("[");
			dst.append(EFFECTIVE_ADDRESS_TABLE[R_M]);
			if (displacement > 0) {
				dst.append(" + ");
				dst.append(displacement);
			} else if (displacement < 0) {
				dst.append(" - ");
				dst.append(-displacement);
			}
			dst.append(']');
		}
		// Register Mode (no displacement)
		else if (MOD == 0b11) {
			// (D = 0) SR is the source.
			dst.append(REG_TABLE[R_M][0]);
			src.append(SR_TABLE[SR]);
		}

		if (D) { // (D = 1) SR is the destination.
			Swap(String_Builder, src, dst);
		}

		decoder.printInst("mov", dst.items, src.items);
		if (decoder.exec) {
			if (dst.items[0] == '[' || src.items[0] == '[') {
				decoder.println("; " LOG_ERROR_STRING ": Memory Expressions are Unimplemented.");
			} else {
                auto const dstReg = getRegisterInfo(dst.items);
                auto const srcReg = getRegisterInfo(src.items);
                u16 const oldDstValue = getRegisterValue(dstReg);
                u16 const srcValue = getRegisterValue(srcReg);
                setRegisterValue(dstReg, srcValue);
                decoder.print("; %s:0x%x->0x%x ", dst.items, oldDstValue, srcValue);
				decoder.printIP("\n");
			}
		} else {
            decoder.print("; (D:%d, ", D);
            decoder.printMOD(MOD, ' ');
            decoder.printSR(SR, ' ');
            decoder.printR_M(R_M, ')');
            decoder.print(" <- ");
            decoder.printByteStack();
		}
	}
}
