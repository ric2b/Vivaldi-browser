#version 310 es

layout(binding = 0, std140)
uniform tint_symbol_1_std140_1_ubo {
  vec2 tint_symbol_col0;
  vec2 tint_symbol_col1;
  vec2 tint_symbol_col2;
  vec2 tint_symbol_col3;
} v;
mat4x2 p = mat4x2(vec2(0.0f), vec2(0.0f), vec2(0.0f), vec2(0.0f));
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  p = mat4x2(v.tint_symbol_col0, v.tint_symbol_col1, v.tint_symbol_col2, v.tint_symbol_col3);
  p[1] = mat4x2(v.tint_symbol_col0, v.tint_symbol_col1, v.tint_symbol_col2, v.tint_symbol_col3)[0];
  p[1] = mat4x2(v.tint_symbol_col0, v.tint_symbol_col1, v.tint_symbol_col2, v.tint_symbol_col3)[0].yx;
  p[0][1] = mat4x2(v.tint_symbol_col0, v.tint_symbol_col1, v.tint_symbol_col2, v.tint_symbol_col3)[1][0];
}
