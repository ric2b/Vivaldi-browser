#version 310 es
precision highp float;
precision highp int;

layout(binding = 0, std430)
buffer prevent_dce_block_1_ssbo {
  vec4 inner;
} v;
uniform highp samplerCube arg_0_arg_1;
vec4 textureSample_e53267() {
  vec3 arg_2 = vec3(1.0f);
  vec4 res = texture(arg_0_arg_1, arg_2);
  return res;
}
void main() {
  v.inner = textureSample_e53267();
}
