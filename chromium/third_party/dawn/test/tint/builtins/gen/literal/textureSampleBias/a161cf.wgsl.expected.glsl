#version 310 es
precision highp float;
precision highp int;

layout(binding = 0, std430)
buffer prevent_dce_block_1_ssbo {
  vec4 inner;
} v;
uniform highp sampler2D arg_0_arg_1;
vec4 textureSampleBias_a161cf() {
  vec4 res = textureOffset(arg_0_arg_1, vec2(1.0f), ivec2(1), clamp(1.0f, -16.0f, 15.9899997711181640625f));
  return res;
}
void main() {
  v.inner = textureSampleBias_a161cf();
}
