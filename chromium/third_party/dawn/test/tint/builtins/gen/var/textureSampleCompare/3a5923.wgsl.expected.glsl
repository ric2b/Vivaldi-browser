#version 310 es
precision highp float;
precision highp int;

layout(binding = 0, std430)
buffer prevent_dce_block_1_ssbo {
  float inner;
} v;
uniform highp sampler2DShadow arg_0_arg_1;
float textureSampleCompare_3a5923() {
  vec2 arg_2 = vec2(1.0f);
  float arg_3 = 1.0f;
  float res = texture(arg_0_arg_1, vec3(arg_2, arg_3));
  return res;
}
void main() {
  v.inner = textureSampleCompare_3a5923();
}
