#version 310 es
precision highp float;
precision highp int;

layout(location = 0) out vec4 tint_symbol_loc0_Output;
vec4 tint_symbol_inner() {
  int v1 = 1;
  uint v2 = 1u;
  float v3 = 1.0f;
  ivec3 v4 = ivec3(1);
  uvec3 v5 = uvec3(1u);
  vec3 v6 = vec3(1.0f);
  mat3 v7 = mat3(vec3(1.0f), vec3(1.0f), vec3(1.0f));
  float v9[10] = float[10](0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
  return vec4(0.0f);
}
void main() {
  tint_symbol_loc0_Output = tint_symbol_inner();
}
