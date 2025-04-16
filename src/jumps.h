#pragma once

#include "decoder.h"

bool isByte_Jump(u8 byte);
void decode_Jump(Decoder_Context& decoder, u8& byte);
