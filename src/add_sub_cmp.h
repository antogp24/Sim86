#pragma once

#include "decoder.h"

bool isByte_ADD_SUB_CMP(Decoder_Context &decoder, u8 &byte);
void decode_ADD_SUB_CMP(Decoder_Context &decoder, u8 &byte);
