#version 310 es
#extension GL_AMD_gpu_shader_half_float: require
precision highp float;
precision highp int;

layout(binding = 0, std430)
buffer tint_symbol_1_1_ssbo {
  float16_t tint_symbol;
} v;
float16_t max_111ac0() {
  float16_t arg_0 = 1.0hf;
  float16_t arg_1 = 1.0hf;
  float16_t res = max(arg_0, arg_1);
  return res;
}
void main() {
  v.tint_symbol = max_111ac0();
}
#version 310 es
#extension GL_AMD_gpu_shader_half_float: require

layout(binding = 0, std430)
buffer tint_symbol_1_1_ssbo {
  float16_t tint_symbol;
} v;
float16_t max_111ac0() {
  float16_t arg_0 = 1.0hf;
  float16_t arg_1 = 1.0hf;
  float16_t res = max(arg_0, arg_1);
  return res;
}
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  v.tint_symbol = max_111ac0();
}
#version 310 es
#extension GL_AMD_gpu_shader_half_float: require


struct VertexOutput {
  vec4 pos;
  float16_t prevent_dce;
};

layout(location = 0) flat out float16_t vertex_main_loc0_Output;
float16_t max_111ac0() {
  float16_t arg_0 = 1.0hf;
  float16_t arg_1 = 1.0hf;
  float16_t res = max(arg_0, arg_1);
  return res;
}
VertexOutput vertex_main_inner() {
  VertexOutput tint_symbol = VertexOutput(vec4(0.0f), 0.0hf);
  tint_symbol.pos = vec4(0.0f);
  tint_symbol.prevent_dce = max_111ac0();
  return tint_symbol;
}
void main() {
  VertexOutput v = vertex_main_inner();
  gl_Position = v.pos;
  gl_Position[1u] = -(gl_Position.y);
  gl_Position[2u] = ((2.0f * gl_Position.z) - gl_Position.w);
  vertex_main_loc0_Output = v.prevent_dce;
  gl_PointSize = 1.0f;
}
