#version 310 es

layout(binding = 0, std140)
uniform u_block_1_ubo {
  mat4 inner;
} v;
shared mat4 w;
void f_inner(uint tint_local_index) {
  if ((tint_local_index == 0u)) {
    w = mat4(vec4(0.0f), vec4(0.0f), vec4(0.0f), vec4(0.0f));
  }
  barrier();
  w = v.inner;
  w[1] = v.inner[0];
  w[1] = v.inner[0].ywxz;
  w[0][1] = v.inner[1].x;
}
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  f_inner(gl_LocalInvocationIndex);
}
