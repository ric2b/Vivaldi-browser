#version 310 es
precision highp float;
precision highp int;

layout(binding = 0, std430) buffer prevent_dce_block_ssbo {
  vec4 inner;
} prevent_dce;

layout(binding = 0, r32f) uniform highp image2D arg_0;
vec4 textureLoad_8bf8c2() {
  uint arg_1 = 1u;
  vec4 res = imageLoad(arg_0, ivec2(uvec2(arg_1, 0u)));
  return res;
}

void fragment_main() {
  prevent_dce.inner = textureLoad_8bf8c2();
}

void main() {
  fragment_main();
  return;
}
#version 310 es

layout(binding = 0, std430) buffer prevent_dce_block_ssbo {
  vec4 inner;
} prevent_dce;

layout(binding = 0, r32f) uniform highp image2D arg_0;
vec4 textureLoad_8bf8c2() {
  uint arg_1 = 1u;
  vec4 res = imageLoad(arg_0, ivec2(uvec2(arg_1, 0u)));
  return res;
}

void compute_main() {
  prevent_dce.inner = textureLoad_8bf8c2();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}
