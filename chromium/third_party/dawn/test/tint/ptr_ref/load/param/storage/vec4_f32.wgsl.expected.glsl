#version 310 es

layout(binding = 0, std430) buffer S_block_ssbo {
  vec4 inner;
} S;

vec4 func_S_inner() {
  return S.inner;
}

void tint_symbol() {
  vec4 r = func_S_inner();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  tint_symbol();
  return;
}
