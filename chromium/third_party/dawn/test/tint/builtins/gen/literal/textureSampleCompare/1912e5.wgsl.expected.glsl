#version 460
precision highp float;
precision highp int;

layout(binding = 0, std430)
buffer prevent_dce_block_1_ssbo {
  float inner;
} v;
uniform highp samplerCubeArrayShadow arg_0_arg_1;
float textureSampleCompare_1912e5() {
  float res = texture(arg_0_arg_1, vec4(vec3(1.0f), float(1u)), 1.0f);
  return res;
}
void main() {
  v.inner = textureSampleCompare_1912e5();
}
