#version 310 es

layout(binding = 0, std140) uniform u_block_std140_ubo {
  vec2 inner_0;
  vec2 inner_1;
  vec2 inner_2;
} u;

mat3x2 p = mat3x2(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
mat3x2 load_u_inner() {
  return mat3x2(u.inner_0, u.inner_1, u.inner_2);
}

void f() {
  p = load_u_inner();
  p[1] = u.inner_0;
  p[1] = u.inner_0.yx;
  p[0][1] = u.inner_1[0u];
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  f();
  return;
}
