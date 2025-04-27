#include "jumps.h"
#include "string_builder.h"


static Instruction_Type jumpTypeFromByte(u8 const byte) {
	switch (byte) {
		case 0b01110000: return Inst_jo;
		case 0b01110001: return Inst_jno;
		case 0b01110010: return Inst_jb;
		case 0b01110011: return Inst_jnb;
		case 0b01110100: return Inst_je;
		case 0b01110101: return Inst_jne;
		case 0b01110110: return Inst_jbe;
		case 0b01110111: return Inst_ja;
		case 0b01111000: return Inst_js;
		case 0b01111001: return Inst_jns;
		case 0b01111010: return Inst_jp;
		case 0b01111011: return Inst_jnp;
		case 0b01111100: return Inst_jl;
		case 0b01111101: return Inst_jnl;
		case 0b01111110: return Inst_jle;
		case 0b01111111: return Inst_jg;
		case 0b11100000: return Inst_loopnz;
		case 0b11100001: return Inst_loopz;
		case 0b11100010: return Inst_loop;
		case 0b11100011: return Inst_jcxz;
		default:         return Inst_None;
	}
}

static Jumps::Outcome runJump(Decoder_Context &decoder, u8 &byte, Instruction const& inst) {
	using namespace FlagsRegister;
	bool jumped = false;
	switch (inst.type) {
		case Inst_jo:  jumped =  getBit(Bit::OF); break;
		case Inst_jno: jumped = !getBit(Bit::OF); break;
		case Inst_jb:  jumped =  getBit(Bit::CF); break;
		case Inst_jnb: jumped = !getBit(Bit::CF); break;
		case Inst_je:  jumped =  getBit(Bit::ZF); break;
		case Inst_jne: jumped = !getBit(Bit::ZF); break;
		case Inst_jbe: jumped =  (getBit(Bit::ZF) || getBit(Bit::CF)); break;
		case Inst_ja:  jumped = !(getBit(Bit::ZF) || getBit(Bit::CF)); break;
		case Inst_js:  jumped =  getBit(Bit::SF); break;
		case Inst_jns: jumped = !getBit(Bit::SF); break;
		case Inst_jp:  jumped =  getBit(Bit::PF); break;
		case Inst_jnp: jumped = !getBit(Bit::PF); break;
		case Inst_jl:  jumped = (getBit(Bit::OF) != getBit(Bit::SF)); break;
		case Inst_jnl: jumped = (getBit(Bit::OF) == getBit(Bit::SF)); break;
		case Inst_jle: jumped =  (getBit(Bit::OF) != getBit(Bit::SF) || getBit(Bit::ZF)); break;
		case Inst_jg:  jumped = !(getBit(Bit::OF) != getBit(Bit::SF) || getBit(Bit::ZF)); break;
		case Inst_loop:
			incrementRegister(RegX(c), -1);
			jumped = (getRegisterValue(RegX(c)) != 0);
			break;
		case Inst_loopz:
			incrementRegister(RegX(c), -1);
			jumped = getBit(Bit::ZF);
			break;
		case Inst_loopnz:
			incrementRegister(RegX(c), -1);
			jumped = !getBit(Bit::ZF);
			break;
		case Inst_jcxz: jumped = (getRegisterValue(RegX(c)) == 0); break;
		default: {
			decoder.println(LOG_ERROR_STRING ": %s is unimplemented", GetInstMnemonic(inst));
			return Jumps::Outcome::error;
		}
	}
	if (jumped) {
		Jumps::Offset const offset = inst.dst.jump_offset;
		incrementIP(offset + 2);
		decoder.bytesRead += offset;
		byte = decoder.binaryBytes.ptr[decoder.bytesRead];
		decoder.resetByteStack();
	}
	return jumped ? Jumps::Outcome::jumped : Jumps::Outcome::stayed;
}

bool isByte_Jump(u8 const byte) {
	return jumpTypeFromByte(byte) != Inst_None;
}

void decode_Jump(Decoder_Context& decoder, u8& byte) {
	assertTrue(isByte_Jump(byte));
	u8 const typeByte = byte;
	u8 const data = decoder.advance08Bits(byte);

	Instruction inst = {
		.dst = InstOpJump(data),
		.src = InstOpNone,
		.type = jumpTypeFromByte(typeByte),
	};

	decoder.printInst(inst);
	if (decoder.exec) {
        u16 const oldIP = getIP();
		u16 const oldCX = getRegisterValue(RegX(c));
		if (auto const outcome = runJump(decoder, byte, inst);
			outcome != Jumps::Outcome::error) {
            u16 const ip = getIP();
			u16 const cx = getRegisterValue(RegX(c));
            decoder.print("%s ", GetJumpOutcomeName(outcome));
			if (JumpModifiesCX(inst.type)) {
				decoder.print("cx:%d->%d ", oldCX, cx);
			}
            decoder.println("ip:0x%x->0x%x", oldIP, ip);
			if (decoder.shouldDecorateOutput()) decoder.print(ASCII_COLOR_END);
		}
	} else {
		decoder.print("(disp:%3d) <- ", inst.dst.jump_offset);
		decoder.printByteStack();
	}
}
