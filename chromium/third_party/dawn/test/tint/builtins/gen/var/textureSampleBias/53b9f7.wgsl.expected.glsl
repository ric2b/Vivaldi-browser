#version 310 es
precision highp float;
precision highp int;

layout(binding = 0, std430)
buffer prevent_dce_block_1_ssbo {
  vec4 inner;
} v;
uniform highp samplerCube arg_0_arg_1;
vec4 textureSampleBias_53b9f7() {
  vec3 arg_2 = vec3(1.0f);
  float arg_3 = 1.0f;
  vec3 v_1 = arg_2;
  vec4 res = texture(arg_0_arg_1, v_1, clamp(arg_3, -16.0f, 15.9899997711181640625f));
  return res;
}
void main() {
  v.inner = textureSampleBias_53b9f7();
}
