#version 310 es
precision highp float;
precision highp int;

layout(binding = 0, std430) buffer prevent_dce_block_ssbo {
  vec3 inner;
  uint pad;
} prevent_dce;

vec3 fwidth_5d1b39() {
  vec3 res = fwidth(vec3(1.0f));
  return res;
}

void fragment_main() {
  prevent_dce.inner = fwidth_5d1b39();
}

void main() {
  fragment_main();
  return;
}
