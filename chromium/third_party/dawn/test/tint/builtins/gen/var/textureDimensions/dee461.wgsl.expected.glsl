#version 310 es
precision highp float;
precision highp int;

layout(binding = 0, std430)
buffer prevent_dce_block_1_ssbo {
  uvec2 inner;
} v;
layout(binding = 0, rgba8_snorm) uniform highp writeonly image2D arg_0;
uvec2 textureDimensions_dee461() {
  uvec2 res = uvec2(imageSize(arg_0));
  return res;
}
void main() {
  v.inner = textureDimensions_dee461();
}
#version 310 es

layout(binding = 0, std430)
buffer prevent_dce_block_1_ssbo {
  uvec2 inner;
} v;
layout(binding = 0, rgba8_snorm) uniform highp writeonly image2D arg_0;
uvec2 textureDimensions_dee461() {
  uvec2 res = uvec2(imageSize(arg_0));
  return res;
}
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  v.inner = textureDimensions_dee461();
}
