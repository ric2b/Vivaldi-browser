#version 310 es


struct Uniforms {
  uint i;
};

struct InnerS {
  int v;
};

struct S1 {
  InnerS s2;
};

struct OuterS {
  S1 a1[8];
};

layout(binding = 4, std140)
uniform tint_symbol_2_1_ubo {
  Uniforms tint_symbol_1;
} v_1;
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  InnerS v = InnerS(0);
  OuterS s1 = OuterS(S1[8](S1(InnerS(0)), S1(InnerS(0)), S1(InnerS(0)), S1(InnerS(0)), S1(InnerS(0)), S1(InnerS(0)), S1(InnerS(0)), S1(InnerS(0))));
  s1.a1[v_1.tint_symbol_1.i].s2 = v;
}
