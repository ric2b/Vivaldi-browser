#version 310 es

layout(binding = 1, std430)
buffer tint_symbol_2_1_ssbo {
  uint tint_symbol_1[1];
} v;
int g() {
  return 0;
}
int f() {
  {
    while(true) {
      g();
      break;
    }
  }
  int o = g();
  return 0;
}
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  {
    while(true) {
      if ((v.tint_symbol_1[0] == 0u)) {
        break;
      }
      int s = f();
      v.tint_symbol_1[0] = 0u;
      {
      }
      continue;
    }
  }
}
