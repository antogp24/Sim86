#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <cmath>

#include "decoder.h"
#include "mov.h"
#include "add_sub_cmp.h"
#include "jumps.h"
#include "util.h"
#include "string_builder.h"

u16 gRegisterValues[RegisterCount] = {0};
u8 gMemory[1024 * 1024] = {0};
u64 gClocks = 0;

Disp_Type get_Disp_Type(u8 const MOD, u8 const R_M) {
	switch (MOD) {
		case 0b00: return (R_M == 0b110) ? Disp_16_bit : Disp_None;
		case 0b01: return Disp_08_bit;
		case 0b10: return Disp_16_bit;
		case 0b11: return Disp_None;
		default: unreachable();
	}
}

Immediate makeImmediateDisp(u16 const disp, u8 const MOD, u8 const R_M) {
	switch (get_Disp_Type(MOD, R_M)) {
		case Disp_None:   return makeImmediateByte(0);
		case Disp_08_bit: return makeImmediateByte(signExtendByte(disp));
		case Disp_16_bit: return makeImmediateWord(signExtendWord(disp));
		default: unreachable();
	}
}

Instruction_Operand_Prefix getInstDstPrefix(Instruction const& inst) {
	if (inst.dst.type == Instruction_Operand_Type::EffectiveAddress &&
	    inst.src.type == Instruction_Operand_Type::Immediate) {
		return inst.dst.address.wide
			? Instruction_Operand_Prefix::Word
			: Instruction_Operand_Prefix::Byte;
	}
	return Instruction_Operand_Prefix::None;
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
			u32 const idx = EffectiveAddress::getInnerValue(operand.address);
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
			u32 const idx = EffectiveAddress::getInnerValue(operand.address);
			StaticArrayBoundsCheck(idx, gMemory);
			if (operand.address.wide) {
				StaticArrayBoundsCheck(idx+1, gMemory);
				gMemory[idx+0] = value & 0xFF;
				gMemory[idx+1] = value >> 8;
			} else {
				gMemory[idx] = value;
			}
		} break;

		default: unreachable();
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

	u32 getInnerValue(Info const& info) {
    	u32 result;
    	switch (info.base) {
			case Base::bx_si:  result = getRegisterValue(RegX(b))  + getRegisterValue(RegX(si)); break;
			case Base::bx_di:  result = getRegisterValue(RegX(b))  + getRegisterValue(RegX(di)); break;
			case Base::bp_si:  result = getRegisterValue(RegX(bp)) + getRegisterValue(RegX(si)); break;
			case Base::bp_di:  result = getRegisterValue(RegX(bp)) + getRegisterValue(RegX(di)); break;
			case Base::si:     result = getRegisterValue(RegX(si)); break;
			case Base::di:     result = getRegisterValue(RegX(di)); break;
			case Base::bp:     result = getRegisterValue(RegX(bp)); break;
			case Base::bx:     result = getRegisterValue(RegX(b));  break;
			case Base::Direct: result = 0;
    	}
    	if (info.displacement.wide) {
    		result += info.displacement.word;
    	} else {
    		result += info.displacement.byte;
    	}
    	return result;
    }

	u8 getClocks(Info const& info) {
    	if (has_EA_Disp(info)) {
    		if (info.base == Base::Direct) {
    			return 6; // Displacement Only
    		}
    		if (is_EA_Base_or_Index(info)) {
    			return 9; // Displacement + (Base or Index)
    		}
    		if (is_EA_Base_Plus_Index(info)) {
    			// Displacement + Base + Index
    			return (info.base == Base::bp_di || info.base == Base::bx_si) ? 11 : 12;
    		}
    	} else {
    		if (is_EA_Base_or_Index(info)) {
    			return 5; // Base or Index Only
    		}
    		if (is_EA_Base_Plus_Index(info)) {
    			// Base + Index
    			return (info.base == Base::bp_di || info.base == Base::bx_si) ? 7 : 8;
    		}
    	}
    	unreachable();
    }
}

u8 getPartClocks(Clock_Calculation_Part const& part) {
	switch (part.type) {
		case Clock_Inst:  return part.value;
		case Clock_EA:    return EffectiveAddress::getClocks(part.address);
		case Clock_Range: return part.B;
		case Clock_AorB:  return Max(part.A, part.B);
		case Clock_SegmentOverride: return 2;
		case Clock_16bitTransfer:   return 4 * part.value;
		default: return 0;
	}
}

u16 getTotalClocks(Clock_Calculation const& calculation) {
	u16 total = 0;
	for (u8 i = 0; i < calculation.part_count; i++) {
		total += getPartClocks(calculation.parts[i]);
	}
	return total;
}

void explainClocks(FILE* f, Clock_Calculation const& calculation) {
	if (calculation.part_count == 1 && calculation.parts[0].type == Clock_None) {
		fprintf(f, "Clocks: +0 = %llu (unimplemented) | ", gClocks);
		return;
	}

	u8 const totalClocks = getTotalClocks(calculation);
	gClocks += totalClocks;
	fprintf(f, "Clocks: +%d = %llu (", totalClocks, gClocks);

	for (u8 i = 0; i < calculation.part_count; i++) {
		if (i > 0) fprintf(f, " + ");
		auto const part = calculation.parts[i];
		u8 const clocks = getPartClocks(part);
		switch (part.type) {
			case Clock_Inst:  fprintf(f, "%d", clocks);                          break;
			case Clock_EA:    fprintf(f, "%dea", clocks);                        break;
			case Clock_Range: fprintf(f, "%d[%d,%d]", clocks, part.A, part.B);   break;
			case Clock_AorB:  fprintf(f, "%d{%d|%d}", clocks, part.A, part.B);   break;
			case Clock_SegmentOverride:  fprintf(f, "%dso", clocks); break;
			case Clock_16bitTransfer:    fprintf(f, "%dt", clocks);    break;
			default: unreachable();
		}
	}
	fprintf(f, ") | ");
}

Clock_Calculation getInstructionClocksCalculation(Instruction const& inst) {
	Clock_Calculation calculation = { .part_count = 0 };
	switch (inst.type) {
		case Inst_mov: {
			if ((IsOperandSegment(inst.dst) && IsOperandReg16(inst.src)) ||
			    (IsOperandReg16(inst.dst) && IsOperandSegment(inst.src))) {
				ClocksPushInst(calculation, 2);
			}
			else if (IsOperandSegment(inst.dst) && IsOperandMem16(inst.src)) {
				ClocksPushInst(calculation, 8);
				ClocksPushEA(calculation, inst.src.address, 1);
			}
			else if (IsOperandMem(inst.dst) && IsOperandSegment(inst.src)) {
				ClocksPushInst(calculation, 8);
				ClocksPushEA(calculation, inst.dst.address, 1);
			}
			else if (IsOperandMem(inst.dst) && IsOperandAccumulator(inst.src)) {
				ClocksPushInst(calculation, 10);
				ClocksPush16bitTransfer(calculation, inst.dst.address, 1);
			}
			else if (IsOperandAccumulator(inst.dst) && IsOperandMem(inst.src)) {
				ClocksPushInst(calculation, 10);
				ClocksPush16bitTransfer(calculation, inst.src.address, 1);
			}
			else if (IsOperandReg(inst.dst) && IsOperandReg(inst.src)) {
				ClocksPushInst(calculation, 2);
			}
			else if (IsOperandReg(inst.dst) && IsOperandMem(inst.src)) {
				ClocksPushInst(calculation, 8);
				ClocksPushEA(calculation, inst.src.address, 1);
			}
			else if (IsOperandMem(inst.dst) && IsOperandReg(inst.src)) {
				ClocksPushInst(calculation, 9);
				ClocksPushEA(calculation, inst.dst.address, 1);
			}
			else if (IsOperandReg(inst.dst) && IsOperandImm(inst.src)) {
				ClocksPushInst(calculation, 4);
			}
			else if (IsOperandMem(inst.dst) && IsOperandImm(inst.src)) {
				ClocksPushInst(calculation, 10);
				ClocksPushEA(calculation, inst.dst.address, 1);
			}
		} break;

		case Inst_add: {
			if (IsOperandReg(inst.dst) && IsOperandGeneralReg(inst.src)) {
				ClocksPushInst(calculation, 3);
			}
			else if (IsOperandReg(inst.dst) && IsOperandMem(inst.src)) {
				ClocksPushInst(calculation, 9);
				ClocksPushEA(calculation, inst.src.address, 1);
			}
			else if (IsOperandMem(inst.dst) && IsOperandReg(inst.src)) {
				ClocksPushInst(calculation, 16);
				ClocksPushEA(calculation, inst.dst.address, 2);
			}
			else if (IsOperandReg(inst.dst) && IsOperandImm(inst.src)) {
				ClocksPushInst(calculation, 4);
			}
			else if (IsOperandMem(inst.dst) && IsOperandImm(inst.src)) {
				ClocksPushInst(calculation, 17);
				ClocksPushEA(calculation, inst.dst.address, 2);
			}
		} break;

		default: ClocksPush(calculation, Clock_Calculation_Part{.type=Clock_None});
	}
	assertTrue(calculation.part_count > 0);
	return calculation;
}

int Decoder_Context::printEffectiveAddressBase(EffectiveAddress::Base const base) const {
	using namespace EffectiveAddress;
	int constexpr BASE_INDEX_LEN = sizeof("?? + ??")-1;

	if (shouldDecorateOutput()) {
		switch (base) {
			case Base::Direct: return 0;
			#define X(R) REGISTER_COLOR R ASCII_COLOR_END
			case Base::bx_si: print(X("bx") " + " X("si")); return BASE_INDEX_LEN;
			case Base::bx_di: print(X("bx") " + " X("di")); return BASE_INDEX_LEN;
			case Base::bp_si: print(X("bp") " + " X("si")); return BASE_INDEX_LEN;
			case Base::bp_di: print(X("bp") " + " X("di")); return BASE_INDEX_LEN;
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
			i8 const disp = static_cast<i8>(operand.jump_offset + 2);
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

bool decodeOrSimulate(FILE* outFile, Slice<u8> const binaryBytes, bool const exec, bool const showClocks) {
	Decoder_Context decoder(outFile, binaryBytes, exec, showClocks);
	decoder.printBitsHeader();

	memset(gMemory, 0, sizeof(gMemory));
	memset(gRegisterValues, 0, sizeof(gRegisterValues));
	gClocks = 0;

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
