#version 310 es

mat3 m = mat3(vec3(0.0f), vec3(0.0f), vec3(0.0f));
layout(binding = 0, std430) buffer tint_symbol_block_ssbo {
  mat3 inner;
} tint_symbol;

void assign_and_preserve_padding_tint_symbol_inner(mat3 value) {
  tint_symbol.inner[0] = value[0u];
  tint_symbol.inner[1] = value[1u];
  tint_symbol.inner[2] = value[2u];
}

void f() {
  assign_and_preserve_padding_tint_symbol_inner(m);
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  f();
  return;
}
