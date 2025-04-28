#pragma once

#include "decoder.h"

bool isByte_common_inst(Decoder_Context &decoder, u8 &byte);
void decode_common_inst(Decoder_Context &decoder, u8 &byte);
