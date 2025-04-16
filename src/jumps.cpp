#include "jumps.h"

enum struct JumpType {
	jo = 112,     // 0b01110000 = 112
	jno,          // 0b01110001 = 113
	jb,           // 0b01110010 = 114
	jnb,          // 0b01110011 = 115
	je,           // 0b01110100 = 116
	jne,          // 0b01110101 = 117
	jbe,          // 0b01110110 = 118
	ja,           // 0b01110111 = 119
	js,           // 0b01111000 = 120
	jns,          // 0b01111001 = 121
	jp,           // 0b01111010 = 122
	jnp,          // 0b01111011 = 123
	jl,           // 0b01111100 = 124
	jnl,          // 0b01111101 = 125
	jle,          // 0b01111110 = 126
	jg,           // 0b01111111 = 127
	loop = 224,   // 0b11100010 = 224
	loopz,        // 0b11100001 = 225
	loopnz,       // 0b11100000 = 226
	jcxz,         // 0b11100011 = 227
};

struct Jump {
	JumpType type;
	i8 offset;
};

static const char* getMnemonic(JumpType const& jump) {
	switch (jump) {
		case JumpType::jo:     return "jo";
		case JumpType::jno:    return "jno";
		case JumpType::jb:     return "jb";
		case JumpType::jnb:    return "jnb";
		case JumpType::je:     return "je";
		case JumpType::jne:    return "jne";
		case JumpType::jbe:    return "jbe";
		case JumpType::ja:     return "ja";
		case JumpType::js:     return "js";
		case JumpType::jns:    return "jns";
		case JumpType::jp:     return "jp";
		case JumpType::jnp:    return "jnp";
		case JumpType::jl:     return "jl";
		case JumpType::jnl:    return "jnl";
		case JumpType::jle:    return "jle";
		case JumpType::jg:     return "jg";
		case JumpType::loop:   return "loop";
		case JumpType::loopz:  return "loopz";
		case JumpType::loopnz: return "loopnz";
		case JumpType::jcxz:   return "jcxz";
		default: return nullptr;
	}
}

enum struct JumpOutcome { error, jumped, stayed };

static const char* getOutcomeName(JumpOutcome const& outcome) {
	switch (outcome) {
		case JumpOutcome::error: return "error";
		case JumpOutcome::jumped: return "jumped";
		case JumpOutcome::stayed: return "stayed";
        default: return nullptr;
	}
}

static JumpOutcome runJump(Decoder_Context &decoder, u8 &byte, Jump const& jump) {
	using namespace FlagsRegister;
	bool jumped = false;
	switch (jump.type) {
		case JumpType::jne: jumped = !getBit(Bit::ZF); break;
		default: {
			decoder.print("; " ASCII_COLOR_RED "error" ASCII_COLOR_END": %s is unimplemented", getMnemonic(jump.type));
			return JumpOutcome::error;
		}
	}
	if (jumped) {
		incrementIP(jump.offset + 2);
		decoder.bytesRead += jump.offset;
		byte = decoder.binaryBytes.ptr[decoder.bytesRead];
		decoder.resetByteStack();
	}
	return jumped ? JumpOutcome::jumped : JumpOutcome::stayed;
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
	Jump const jump = {.type=static_cast<JumpType>(typeByte), .offset=signExtendByte(data)};

	// The offset gets added 2 because the jump instructions take up 2 bytes.
	char rhs[sizeof("$+XXXX+0")] = {0};
	if (jump.offset + 2 > 0) {
		snprintf(rhs, sizeof(rhs), "$+%d+0", jump.offset + 2);
	}
	else if (jump.offset + 2 == 0) {
		snprintf(rhs, sizeof(rhs), "$+0");
	}
	else {
		snprintf(rhs, sizeof(rhs), "$%d+0", jump.offset + 2);
	}

	decoder.printInst(getMnemonic(jump.type), rhs);
	if (decoder.exec) {
        u16 const oldIP = getIP();
		if (auto const outcome = runJump(decoder, byte, jump);
			outcome != JumpOutcome::error) {
            decoder.print("; %s ", getOutcomeName(outcome));
            u16 const ip = getIP();
            decoder.print("ip:0x%x->0x%x\n", oldIP, ip);
		}
	} else {
		decoder.print("; (disp:%3d) ", jump.offset);
		decoder.printByteStack();
	}
}
