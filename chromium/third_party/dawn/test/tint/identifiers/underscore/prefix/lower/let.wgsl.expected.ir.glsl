#version 310 es

layout(binding = 0, std430)
buffer tint_symbol_1_1_ssbo {
  int tint_symbol;
} v;
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  int a = 1;
  int _a = a;
  int b = a;
  int _b = _a;
  v.tint_symbol = (((a + _a) + b) + _b);
}
