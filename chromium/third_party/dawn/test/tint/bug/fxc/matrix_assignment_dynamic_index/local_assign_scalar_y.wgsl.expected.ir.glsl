#version 310 es


struct Uniforms {
  uint i;
  uint j;
};

layout(binding = 4, std140)
uniform tint_symbol_2_1_ubo {
  Uniforms tint_symbol_1;
} v;
mat2x4 m1 = mat2x4(vec4(0.0f), vec4(0.0f));
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  m1[0][v.tint_symbol_1.j] = 1.0f;
}
