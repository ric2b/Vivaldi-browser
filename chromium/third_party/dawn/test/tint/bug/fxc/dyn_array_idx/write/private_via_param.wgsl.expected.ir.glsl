#version 310 es


struct UBO {
  int dynamic_idx;
};

struct Result {
  int tint_symbol;
};

struct S {
  int data[64];
};

layout(binding = 0, std140)
uniform tint_symbol_2_1_ubo {
  UBO tint_symbol_1;
} v;
layout(binding = 1, std430)
buffer tint_symbol_4_1_ssbo {
  Result tint_symbol_3;
} v_1;
S s = S(int[64](0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0));
void x(inout S p) {
  p.data[v.tint_symbol_1.dynamic_idx] = 1;
}
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  x(s);
  v_1.tint_symbol_3.tint_symbol = s.data[3];
}
