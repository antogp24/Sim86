#ifndef InstOpType
#define InstOpType(x) x,
#endif

InstOpType(Immediate)
InstOpType(Register)
InstOpType(RegisterPair)
InstOpType(EffectiveAddress)
InstOpType(Jump)

#undef InstOpType