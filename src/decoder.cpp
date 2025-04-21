#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <cmath>

#include "string_builder.h"
#include "decoder.h"
#include "mov.h"
#include "add_sub_cmp.h"
#include "jumps.h"
#include "util.h"

u16 gRegisterValues[RegisterCount] = {0};
u8 gMemory[1024 * 1024] = {0};

Instruction_Operand_Prefix getInstDstPrefix(Instruction_Operand const& dst, Instruction_Operand const& src) {
	if (dst.type != Instruction_Operand_Type::EffectiveAddress) {
		return Instruction_Operand_Prefix::None;
	}
	bool wide = false;
	switch (src.type) {
		case Instruction_Operand_Type::Immediate: wide = src.immediate.wide; break;
		case Instruction_Operand_Type::Register:  wide = src.reg.usage == RegisterUsage::x; break;
		default: unreachable();
	}
	return wide ? Instruction_Operand_Prefix::Word : Instruction_Operand_Prefix::Byte;
}

String_Builder getInstOpName(Instruction_Operand const& operand) {
	String_Builder builder = string_builder_make();

	switch (operand.type) {
		case Instruction_Operand_Type::Register: {
			builder.append(getRegisterName(operand.reg));
		} break;

		case Instruction_Operand_Type::EffectiveAddress: {
			builder.append('[');
			builder.append(EffectiveAddress::getInnerValue(operand.address));
			builder.append(']');
		} break;

		case Instruction_Operand_Type::Immediate: {
			if (operand.immediate.wide) {
				builder.append(operand.immediate.word);
			} else {
				builder.append(operand.immediate.byte);
			}
		} break;

		default: break;
	}
	return builder;
}

u16 getInstOpValue(Instruction_Operand const& operand) {
	switch (operand.type) {
		case Instruction_Operand_Type::Register:  return getRegisterValue(operand.reg);
		case Instruction_Operand_Type::Immediate: return operand.immediate.word;

		case Instruction_Operand_Type::EffectiveAddress: {
			u16 const idx = EffectiveAddress::getInnerValue(operand.address);
			StaticArrayBoundsCheck(idx, gMemory);
			if (operand.address.wide) {
				StaticArrayBoundsCheck(idx+1, gMemory);
				return (gMemory[idx+1] << 8) + gMemory[idx+0];
			} else {
				return gMemory[idx];
			}
		}

		default: return 0;
	}
}

void setInstOpValue(Instruction_Operand const& operand, u16 const value) {
	switch (operand.type) {
		case Instruction_Operand_Type::Register: {
			setRegisterValue(operand.reg, value);
		} break;

		case Instruction_Operand_Type::EffectiveAddress: {
			u16 const idx = EffectiveAddress::getInnerValue(operand.address);
			StaticArrayBoundsCheck(idx, gMemory);
			if (operand.address.wide) {
				StaticArrayBoundsCheck(idx+1, gMemory);
				gMemory[idx+0] = value & 0xFF;
				gMemory[idx+1] = value >> 8;
			} else {
				gMemory[idx] = value;
			}
		}

		default: /* error */ break;
	}
}

namespace FlagsRegister {
	void setBit(Bit const& bit, bool const value) {
		u16 const b = static_cast<u16>(bit);
		if (value) {
            gRegisterValues[RegToID(Register::fl)] |= (1 << b);
		} else {
            gRegisterValues[RegToID(Register::fl)] &= ~(1 << b);
		}
	}

	void printSet(FILE* outFile, u16 const flags) {
		for (Bit const bit: BitList) {
			if (getBit(bit, flags)) {
				fprintf(outFile, "%c", getLetter(bit));
			}
		}
	}

	void printSet(FILE* outFile) {
		printSet(outFile, get());
	}

	u16 get() {
		return gRegisterValues[RegToID(Register::fl)];
	}

	bool getBit(Bit const& bit, u16 const flags) {
		u16 const b = static_cast<u16>(bit);
		return (flags >> b) & 1;
	}

	bool getBit(Bit const& bit) {
		return getBit(bit, get());
	}

    char getLetter(Bit const& bit) {
        switch (bit) {
            case Bit::CF: return 'C'; case Bit::PF: return 'P'; case Bit::AF: return 'A';
            case Bit::ZF: return 'Z'; case Bit::SF: return 'S'; case Bit::OF: return 'O';
            case Bit::IF: return 'I'; case Bit::DF: return 'D'; case Bit::TF: return 'T';
            default: unreachable();
        }
    }

    const char* getFullName(Bit const& bit) {
        switch (bit) {
            case Bit::CF: return "Carry Flag";
            case Bit::PF: return "Parity Flag";
            case Bit::AF: return "Auxiliary Carry Flag";
            case Bit::ZF: return "Zero Flag";
            case Bit::SF: return "Sign Flag";
            case Bit::OF: return "Overflow Flag";
            case Bit::IF: return "Interrupt-Enable Flag";
            case Bit::DF: return "Direction Flag";
            case Bit::TF: return "Trap Flag";
            default: unreachable();
        }
    }
}

u16 getRegisterValue(RegisterInfo const& reg) {
	u8 const idx = RegToID(reg.type);
	switch (reg.usage) {
	case RegisterUsage::x: return gRegisterValues[idx];
	case RegisterUsage::h: return reinterpret_cast<u8*>(gRegisterValues + idx)[1];
	case RegisterUsage::l: return reinterpret_cast<u8*>(gRegisterValues + idx)[0];
	default: unreachable();
	}
}

void setRegisterValue(RegisterInfo const& reg, u16 const value) {
	u8 const idx = RegToID(reg.type);
	switch (reg.usage) {
	case RegisterUsage::x: gRegisterValues[idx] = value; break;
	case RegisterUsage::h: reinterpret_cast<u8*>(gRegisterValues + idx)[1] = static_cast<u8>(value); break;
	case RegisterUsage::l: reinterpret_cast<u8*>(gRegisterValues + idx)[0] = static_cast<u8>(value); break;
	default: unreachable();
	}
}

void incrementRegister(RegisterInfo const& reg, i16 const increment) {
	u16 const old = getRegisterValue(reg);
	setRegisterValue(reg, old + increment);
}

const char* getRegisterName(RegisterInfo const& reg) {
	u8 const idx = RegToID(reg.type);
	switch (reg.type) {
		case Register::a: case Register::b:
		case Register::c: case Register::d:
			switch (reg.usage) {
				case RegisterUsage::x: return RegisterNames[idx];
				case RegisterUsage::h: return RegisterExtraNames[2*idx+0];
				case RegisterUsage::l: return RegisterExtraNames[2*idx+1];
                default: return nullptr;
			}
        case Register::sp: case Register::bp:
        case Register::si: case Register::di:
        case Register::cs: case Register::ds:
        case Register::ss: case Register::es:
        case Register::ip: case Register::fl:
			return RegisterNames[idx];
        default: return nullptr;
	}
}

namespace EffectiveAddress {
    const char* base2string(Base const base) {
    	switch (base) {
            case Base::Direct: return "";
            case Base::bx_si:  return "bx + si";
            case Base::bx_di:  return "bx + di";
            case Base::bp_si:  return "bp + si";
            case Base::bp_di:  return "bp + di";
            case Base::si:     return "si";
            case Base::di:     return "di";
            case Base::bp:     return "bp";
            case Base::bx:     return "bx";
            default: return nullptr;
    	}
    }

	u16 getInnerValue(Info const& info) {
    	u16 result;
    	switch (info.base) {
			case Base::bx_si: result = getRegisterValue(RegX(b))  + getRegisterValue(RegX(si)); break;
			case Base::bx_di: result = getRegisterValue(RegX(b))  + getRegisterValue(RegX(di)); break;
			case Base::bp_si: result = getRegisterValue(RegX(bp)) + getRegisterValue(RegX(si)); break;
			case Base::bp_di: result = getRegisterValue(RegX(bp)) + getRegisterValue(RegX(di)); break;
			case Base::si:    result = getRegisterValue(RegX(si)); break;
			case Base::di:    result = getRegisterValue(RegX(di)); break;
			case Base::bp:    result = getRegisterValue(RegX(bp)); break;
			case Base::bx:    result = getRegisterValue(RegX(b));  break;
			case Base::Direct: default: result = 0;
    	}
    	if (info.displacement.wide) {
    		result += info.displacement.word;
    	} else {
    		result += info.displacement.byte;
    	}
    	return result;
    }
}
int Decoder_Context::printEffectiveAddressBase(EffectiveAddress::Base const base) const {
	using namespace EffectiveAddress;
	if (shouldDecorateOutput()) {
		switch (base) {
			case Base::Direct: return 0;
			#define X(R) REGISTER_COLOR R ASCII_COLOR_END
			case Base::bx_si: print(X("bx") " + " X("si")); return sizeof("?? + ??")-1;
			case Base::bx_di: print(X("bx") " + " X("di")); return sizeof("?? + ??")-1;
			case Base::bp_si: print(X("bp") " + " X("si")); return sizeof("?? + ??")-1;
			case Base::bp_di: print(X("bp") " + " X("di")); return sizeof("?? + ??")-1;
			#undef X

			case Base::si: case Base::di: case Base::bp: case Base::bx: {
				print(REGISTER_COLOR);
				int const n = _print(base2string(base));
				print(ASCII_COLOR_END);
				return n;
			}

			default: return 0;
		}
	}
	return _print(base2string(base));
}

int Decoder_Context::printInstOperand(Instruction_Operand const& operand, Instruction_Operand_Prefix prefix) const {
	if (operand.type == Instruction_Operand_Type::None) return 0;
	int n = 0;

	switch (prefix) {
        case Instruction_Operand_Prefix::Byte:
        case Instruction_Operand_Prefix::Word:
			if (shouldDecorateOutput()) print(INST_PREFIX_COLOR);
			n+=_print("%s ", InstOpPrefixName[static_cast<u8>(prefix)]);
			if (shouldDecorateOutput()) print(ASCII_COLOR_END);
			break;
		default: break;
	}

	switch (operand.type) {
        case Instruction_Operand_Type::Immediate: {
        	if (shouldDecorateOutput()) print(NUMBER_COLOR);
        	if (operand.immediate.wide) {
        		n+=_print("%" PRIi16, operand.immediate.word);
        	} else {
        		n+=_print("%" PRIi8, operand.immediate.byte);
        	}
        	if (shouldDecorateOutput()) print(ASCII_COLOR_END);
        } break;

        case Instruction_Operand_Type::Register: {
        	if (shouldDecorateOutput()) print(REGISTER_COLOR);
			n+=_print(getRegisterName(operand.reg));
        	if (shouldDecorateOutput()) print(ASCII_COLOR_END);
        } break;

        case Instruction_Operand_Type::EffectiveAddress: {
			fputc('[', outFile); n++;
        	bool const hasBase = operand.address.base != EffectiveAddress::Base::Direct;
        	if (hasBase) {
        		n += printEffectiveAddressBase(operand.address.base);
        	}
        	if (operand.address.displacement.wide) {
                if (operand.address.displacement.word > 0) {
					if (hasBase) n+=_print(" + ");
					if (shouldDecorateOutput()) print(NUMBER_COLOR);
					n+=_print("%" PRIi16, operand.address.displacement.word);
					if (shouldDecorateOutput()) print(ASCII_COLOR_END);
                } else if (operand.address.displacement.word < 0) {
					n+=_print(hasBase ? " - " : "-");
					if (shouldDecorateOutput()) print(NUMBER_COLOR);
					n+=_print("%" PRIi16, -operand.address.displacement.word);
					if (shouldDecorateOutput()) print(ASCII_COLOR_END);
                }
        	} else {
                if (operand.address.displacement.byte > 0) {
					if (hasBase) n+=_print(" + ");
					if (shouldDecorateOutput()) print(NUMBER_COLOR);
					n+=_print("%" PRIi8, operand.address.displacement.byte);
					if (shouldDecorateOutput()) print(ASCII_COLOR_END);
                } else if (operand.address.displacement.byte < 0) {
					n+=_print(hasBase ? " - " : "-");
					if (shouldDecorateOutput()) print(NUMBER_COLOR);
					n+=_print("%" PRIi16, -operand.address.displacement.byte);
					if (shouldDecorateOutput()) print(ASCII_COLOR_END);
                }
        	}
			fputc(']', outFile); n++;
        } break;

		case Instruction_Operand_Type::Jump: {
			// The offset gets added 2 because the jump instructions take up 2 bytes.
			i8 const disp = static_cast<i8>(operand.jump.offset + 2);
			if (disp > 0) {
				n+=_print("$+");
				if (shouldDecorateOutput()) print(NUMBER_COLOR);
				n+=_print("%" PRIi8, disp);
				if (shouldDecorateOutput()) print(ASCII_COLOR_END);
				n+=_print("+");
				if (shouldDecorateOutput()) print(NUMBER_COLOR);
				n+=_print("%" PRIi8, 0);
				if (shouldDecorateOutput()) print(ASCII_COLOR_END);
			} else if (disp < 0) {
				n+=_print("$-");
				if (shouldDecorateOutput()) print(NUMBER_COLOR);
				n+=_print("%" PRIi8, -disp);
				if (shouldDecorateOutput()) print(ASCII_COLOR_END);
				n+=_print("+");
				if (shouldDecorateOutput()) print(NUMBER_COLOR);
				n+=_print("%" PRIi8, 0);
				if (shouldDecorateOutput()) print(ASCII_COLOR_END);
			} else {
				n+=_print("$+");
				if (shouldDecorateOutput()) print(NUMBER_COLOR);
				n+=_print("%" PRIi8, 0);
				if (shouldDecorateOutput()) print(ASCII_COLOR_END);
			}
		} break;

        default: unreachable();
	}
	return n;
}

bool decodeOrSimulate(FILE* outFile, Slice<u8> const binaryBytes, bool const exec) {
	Decoder_Context decoder(outFile, binaryBytes, exec);
	decoder.printBitsHeader();

	memset(gMemory, 0, sizeof(gMemory));
	memset(gRegisterValues, 0, sizeof(gRegisterValues));

	while (decoder.bytesRead < binaryBytes.count) {
		u8 byte; decoder.advance(byte);

		if (isByte_MOV(byte)) {
			decode_MOV(decoder, byte);
		}
		else if (isByte_Jump(byte)) {
			decode_Jump(decoder, byte);
		}
		else if (isByte_ADD_SUB_CMP(decoder, byte)) {
			decode_ADD_SUB_CMP(decoder, byte);
		}
		else {
			eprintf(LOG_ERROR_STRING": Had an unrecognized byte (" ASCII_COLOR_B_RED);
			printBits(stderr, byte, 8);
			eprintfln(ASCII_COLOR_END")");
			return false;
		}
		incrementIP(decoder.byteStack.count);
		decoder.resetByteStack();
	}

	if (decoder.exec) {
		decoder.println("\nFinal registers:");
		decoder.printRegistersLN();
	}

	return true;
}

void printBits(FILE* outFile, u8 const byte, int const count) {
	assertTrue(count >= 1 && count <= 8);
	for (int i = count - 1; i >= 0; i--) {
		fprintf(outFile, "%d", (byte >> i) & 1);
	}
}

void printBits(FILE* outFile, u8 const byte, int const count, char const ending) {
	printBits(outFile, byte, count);
	fputc(ending, outFile);
}
