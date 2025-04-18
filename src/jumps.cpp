#include "jumps.h"
#include "string_builder.h"

const char* Jumps::getMnemonic(Type const type) {
	switch (type) {
		case Type::jo:     return "jo";
		case Type::jno:    return "jno";
		case Type::jb:     return "jb";
		case Type::jnb:    return "jnb";
		case Type::je:     return "je";
		case Type::jne:    return "jne";
		case Type::jbe:    return "jbe";
		case Type::ja:     return "ja";
		case Type::js:     return "js";
		case Type::jns:    return "jns";
		case Type::jp:     return "jp";
		case Type::jnp:    return "jnp";
		case Type::jl:     return "jl";
		case Type::jnl:    return "jnl";
		case Type::jle:    return "jle";
		case Type::jg:     return "jg";
		case Type::loop:   return "loop";
		case Type::loopz:  return "loopz";
		case Type::loopnz: return "loopnz";
		case Type::jcxz:   return "jcxz";
		default: return nullptr;
	}
}

const char* Jumps::getOutcomeName(Outcome const outcome) {
	switch (outcome) {
		case Outcome::error: return "error";
		case Outcome::jumped: return "jumped";
		case Outcome::stayed: return "stayed";
        default: return nullptr;
	}
}

static Jumps::Outcome runJump(Decoder_Context &decoder, u8 &byte, Jumps::Info const& jump) {
	using namespace Jumps;
	using namespace FlagsRegister;
	bool jumped = false;
	switch (jump.type) {
		case Type::jo:  jumped =  getBit(Bit::OF); break;
		case Type::jno: jumped = !getBit(Bit::OF); break;
		case Type::jb:  jumped =  getBit(Bit::CF); break;
		case Type::jnb: jumped = !getBit(Bit::CF); break;
		case Type::je:  jumped =  getBit(Bit::ZF); break;
		case Type::jne: jumped = !getBit(Bit::ZF); break;
		case Type::jbe: jumped =  (getBit(Bit::ZF) || getBit(Bit::CF)); break;
		case Type::ja:  jumped = !(getBit(Bit::ZF) || getBit(Bit::CF)); break;
		case Type::js:  jumped =  getBit(Bit::SF); break;
		case Type::jns: jumped = !getBit(Bit::SF); break;
		case Type::jp:  jumped =  getBit(Bit::PF); break;
		case Type::jnp: jumped = !getBit(Bit::PF); break;
		case Type::jl:  jumped = (getBit(Bit::OF) != getBit(Bit::SF)); break;
		case Type::jnl: jumped = (getBit(Bit::OF) == getBit(Bit::SF)); break;
		case Type::jle: jumped =  (getBit(Bit::OF) != getBit(Bit::SF) || getBit(Bit::ZF)); break;
		case Type::jg:  jumped = !(getBit(Bit::OF) != getBit(Bit::SF) || getBit(Bit::ZF)); break;
		case Type::loop:
			incrementRegister(RegX(c), -1);
			jumped = (getRegisterValue(RegX(c)) == 0);
			break;
		case Type::loopz:
			incrementRegister(RegX(c), -1);
			jumped = getBit(Bit::ZF);
			break;
		case Type::loopnz:
			incrementRegister(RegX(c), -1);
			jumped = !getBit(Bit::ZF);
			break;
		case Type::jcxz: jumped = (getRegisterValue(RegX(c)) == 0); break;
		default: {
			decoder.println("; " LOG_ERROR_STRING ": %s is unimplemented", getMnemonic(jump.type));
			return Outcome::error;
		}
	}
	if (jumped) {
		incrementIP(jump.offset + 2);
		decoder.bytesRead += jump.offset;
		byte = decoder.binaryBytes.ptr[decoder.bytesRead];
		decoder.resetByteStack();
	}
	return jumped ? Jumps::Outcome::jumped : Jumps::Outcome::stayed;
}

bool isByte_Jump(u8 const byte) {
    const bool result = (byte >= 112 && byte <= 127) ||
                        (byte >= 224 && byte <= 227);
    return result;
}

void decode_Jump(Decoder_Context& decoder, u8& byte) {
	assertTrue(isByte_Jump(byte));
	u8 const typeByte = byte;
	u8 const data = decoder.advance08Bits(byte);

	auto const operand = InstJump(typeByte, data);

	decoder.printInst(Jumps::getMnemonic(operand.jump.type), operand);
	if (decoder.exec) {
        u16 const oldIP = getIP();
		if (auto const outcome = runJump(decoder, byte, operand.jump);
			outcome != Jumps::Outcome::error) {
            decoder.print("; %s ", getOutcomeName(outcome));
            u16 const ip = getIP();
            decoder.print("ip:0x%x->0x%x\n", oldIP, ip);
			if (decoder.outFile == stdout) decoder.print(ASCII_COLOR_END);
		}
	} else {
		decoder.print("; (disp:%3d) <- ", operand.jump.offset);
		decoder.printByteStack();
	}
}
