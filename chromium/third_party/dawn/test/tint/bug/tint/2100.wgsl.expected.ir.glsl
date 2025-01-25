#version 310 es


struct S_std140 {
  mat4 matrix_view;
  vec3 matrix_normal_col0;
  vec3 matrix_normal_col1;
  vec3 matrix_normal_col2;
};

layout(binding = 0, std140)
uniform tint_symbol_3_std140_1_ubo {
  S_std140 tint_symbol_2;
} v;
vec4 tint_symbol_1_inner() {
  float x = v.tint_symbol_2.matrix_view[0].z;
  return vec4(x, 0.0f, 0.0f, 1.0f);
}
void main() {
  gl_Position = tint_symbol_1_inner();
  gl_Position[1u] = -(gl_Position.y);
  gl_Position[2u] = ((2.0f * gl_Position.z) - gl_Position.w);
  gl_PointSize = 1.0f;
}
