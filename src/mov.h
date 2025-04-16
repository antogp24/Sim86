#pragma once

#include "decoder.h"

bool isByte_MOV(u8 byte);
void decode_MOV(Decoder_Context& decoder, u8& byte);
