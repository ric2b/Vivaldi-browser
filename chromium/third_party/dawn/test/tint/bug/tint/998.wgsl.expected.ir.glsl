#version 310 es


struct Constants {
  uint zero;
};

struct S {
  uint data[3];
};

layout(binding = 0, std140)
uniform tint_symbol_2_1_ubo {
  Constants tint_symbol_1;
} v;
S s = S(uint[3](0u, 0u, 0u));
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  s.data[v.tint_symbol_1.zero] = 0u;
}
