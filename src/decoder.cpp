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

static force_inline RegisterUsage getGeneralPurposeRegisterUsage(const char* reg) {
	switch (reg[1]) {
	case 'x': return RegisterUsage::x;
	case 'l': return RegisterUsage::l;
	case 'h': return RegisterUsage::h;
	default: unreachable();
	}
}

static force_inline bool isRegisterGeneralPurpose(RegisterInfo const& reg) {
	return reg.type == Register::a ||
	       reg.type == Register::b ||
	       reg.type == Register::c ||
	       reg.type == Register::d;
}

static force_inline bool isRegisterGeneralPurpose(const char* reg) {
	return  0 == strncmp(reg, "ax", 2) ||
			0 == strncmp(reg, "al", 2) ||
			0 == strncmp(reg, "ah", 2) ||
			0 == strncmp(reg, "bx", 2) ||
			0 == strncmp(reg, "bl", 2) ||
			0 == strncmp(reg, "bh", 2) ||
			0 == strncmp(reg, "cx", 2) ||
			0 == strncmp(reg, "cl", 2) ||
			0 == strncmp(reg, "ch", 2) ||
			0 == strncmp(reg, "dx", 2) ||
			0 == strncmp(reg, "dl", 2) ||
			0 == strncmp(reg, "dh", 2);
}

RegisterInfo getRegisterInfo(const char* reg) {
	assertTrue(strlen(reg) == 2);
	RegisterInfo info = {.usage = RegisterUsage::x};
	if (isRegisterGeneralPurpose(reg)) {
		info.type = IDToReg(reg[0]-'a');
		info.usage = getGeneralPurposeRegisterUsage(reg);
	} else if (0 == strncmp(reg, "sp", 2)) {
		info.type = Register::sp;
	} else if (0 == strncmp(reg, "bp", 2)) {
		info.type = Register::bp;
	} else if (0 == strncmp(reg, "si", 2)) {
		info.type = Register::si;
	} else if (0 == strncmp(reg, "di", 2)) {
		info.type = Register::di;
	} else if (0 == strncmp(reg, "cs", 2)) {
		info.type = Register::cs;
	} else if (0 == strncmp(reg, "ds", 2)) {
		info.type = Register::ds;
	} else if (0 == strncmp(reg, "ss", 2)) {
		info.type = Register::ss;
	} else if (0 == strncmp(reg, "es", 2)) {
		info.type = Register::es;
	} else if (0 == strncmp(reg, "ip", 2)) {
		info.type = Register::ip;
	} else if (0 == strncmp(reg, "fl", 2)) {
		info.type = Register::fl;
	} else {
		panic("Invalid register '%s'", reg);
	}
	return info;
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

const char* getRegisterName(RegisterInfo const& reg) {
	switch (reg.type) {
		case Register::a: {
			switch (reg.usage) {
				case RegisterUsage::x: return "ax";
				case RegisterUsage::l: return "al";
				case RegisterUsage::h: return "ah";
                default: return nullptr;
			}
		}
        case Register::b: {
			switch (reg.usage) {
				case RegisterUsage::x: return "bx";
				case RegisterUsage::l: return "bl";
				case RegisterUsage::h: return "bh";
                default: return nullptr;
			}
		}
        case Register::c: {
			switch (reg.usage) {
				case RegisterUsage::x: return "cx";
				case RegisterUsage::l: return "cl";
				case RegisterUsage::h: return "ch";
                default: return nullptr;
			}
		}
        case Register::d: {
			switch (reg.usage) {
				case RegisterUsage::x: return "dx";
				case RegisterUsage::l: return "dl";
				case RegisterUsage::h: return "dh";
                default: return nullptr;
			}
		}
        case Register::sp: return "sp"; case Register::bp: return "bp";
        case Register::si: return "si"; case Register::di: return "di";
        case Register::cs: return "cs"; case Register::ds: return "ds";
        case Register::ss: return "ss"; case Register::es: return "es";
        case Register::ip: return "ip"; case Register::fl: return "fl";
        default: return nullptr;
	}
}


namespace EffectiveAddress {
    const char* base2string(Base const base) {
    	switch (base) {
            case Base::Direct: return "";
            case Base::bx_si: return "bx + si";
            case Base::bx_di: return "bx + di";
            case Base::bp_si: return "bp + si";
            case Base::bp_di: return "bp + di";
            case Base::si: return "si";
            case Base::di: return "di";
            case Base::bp: return "bp";
            case Base::bx: return "bx";
            default: return nullptr;
    	}
    }
}

String_Builder operand2string(Instruction_Operand const& operand) {
    String_Builder builder = string_builder_make();

	switch (operand.prefix) {
        case Instruction_Operand_Prefix::Byte: builder.append("byte "); break;
        case Instruction_Operand_Prefix::Word: builder.append("word "); break;
        case Instruction_Operand_Prefix::None: default: break;
	}

	switch (operand.type) {
        case Instruction_Operand_Type::Immediate: {
        	if (operand.immediate.wide) {
        		builder.append(operand.immediate.word);
        	} else {
        		builder.append(operand.immediate.byte);
        	}
        } break;

        case Instruction_Operand_Type::Register: {
        	builder.append(getRegisterName(operand.reg));
        } break;

        case Instruction_Operand_Type::EffectiveAddress: {
			builder.append('[');
        	bool const hasBase = operand.address.base != EffectiveAddress::Base::Direct;
        	if (hasBase) {
                builder.append(EffectiveAddress::base2string(operand.address.base));
        	}
        	if (operand.address.displacement.wide) {
                if (operand.address.displacement.word > 0) {
                    builder.append(hasBase ? " + " : "");
                    builder.append(operand.address.displacement.word);
                } else if (operand.address.displacement.word < 0) {
                    builder.append(hasBase ? " - " : "-");
                    builder.append(-operand.address.displacement.word);
                }
        	} else {
                if (operand.address.displacement.byte > 0) {
                    builder.append(hasBase ? " + " : "");
                    builder.append(operand.address.displacement.byte);
                } else if (operand.address.displacement.byte < 0) {
                    builder.append(hasBase ? " - " : "-");
                    builder.append(-operand.address.displacement.byte);
                }
        	}
			builder.append(']');
        } break;

        default: unreachable();
	}
    return builder;
}

bool decodeOrSimulate(FILE* outFile, Slice<u8> const binaryBytes, bool const exec) {
	Decoder_Context decoder(outFile, binaryBytes, exec);
	if (!exec) {
		decoder.println("bits 16");
	}

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

void printBits(FILE* outFile, u8 byte, int count) {
	assertTrue(count >= 1 && count <= 8);
	for (int i = count - 1; i >= 0; i--) {
		fprintf(outFile, "%d", (byte >> i) & 1);
	}
}

void printBits(FILE* outFile, u8 byte, int count, char ending) {
	printBits(outFile, byte, count);
	fputc(ending, outFile);
}
