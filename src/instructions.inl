#ifndef Inst
#define Inst(x) Inst_##x,
#endif

Inst(mov)
Inst(add)
Inst(sub)
Inst(cmp)
Inst(jo)
Inst(jno)
Inst(jb)
Inst(jnb)
Inst(je)
Inst(jne)
Inst(jbe)
Inst(ja)
Inst(js)
Inst(jns)
Inst(jp)
Inst(jnp)
Inst(jl)
Inst(jnl)
Inst(jle)
Inst(jg)
Inst(loopnz)
Inst(loopz)
Inst(loop)
Inst(jcxz)

#undef Inst