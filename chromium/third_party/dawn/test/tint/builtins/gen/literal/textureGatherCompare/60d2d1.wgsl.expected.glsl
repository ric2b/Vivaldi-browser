#version 460
precision highp float;
precision highp int;

layout(binding = 0, std430)
buffer prevent_dce_block_1_ssbo {
  vec4 inner;
} v;
uniform highp samplerCubeArrayShadow arg_0_arg_1;
vec4 textureGatherCompare_60d2d1() {
  vec4 res = textureGather(arg_0_arg_1, vec4(vec3(1.0f), float(1)), 1.0f);
  return res;
}
void main() {
  v.inner = textureGatherCompare_60d2d1();
}
#version 460

layout(binding = 0, std430)
buffer prevent_dce_block_1_ssbo {
  vec4 inner;
} v;
uniform highp samplerCubeArrayShadow arg_0_arg_1;
vec4 textureGatherCompare_60d2d1() {
  vec4 res = textureGather(arg_0_arg_1, vec4(vec3(1.0f), float(1)), 1.0f);
  return res;
}
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  v.inner = textureGatherCompare_60d2d1();
}
#version 460


struct VertexOutput {
  vec4 pos;
  vec4 prevent_dce;
};

uniform highp samplerCubeArrayShadow arg_0_arg_1;
layout(location = 0) flat out vec4 vertex_main_loc0_Output;
vec4 textureGatherCompare_60d2d1() {
  vec4 res = textureGather(arg_0_arg_1, vec4(vec3(1.0f), float(1)), 1.0f);
  return res;
}
VertexOutput vertex_main_inner() {
  VertexOutput tint_symbol = VertexOutput(vec4(0.0f), vec4(0.0f));
  tint_symbol.pos = vec4(0.0f);
  tint_symbol.prevent_dce = textureGatherCompare_60d2d1();
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
