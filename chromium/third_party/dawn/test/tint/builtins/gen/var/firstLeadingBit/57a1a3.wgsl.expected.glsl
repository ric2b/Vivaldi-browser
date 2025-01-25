#version 310 es
precision highp float;
precision highp int;

layout(binding = 0, std430)
buffer prevent_dce_block_1_ssbo {
  int inner;
} v;
int firstLeadingBit_57a1a3() {
  int arg_0 = 1;
  uint v_1 = uint(arg_0);
  uint v_2 = mix(~(v_1), v_1, (v_1 < 2147483648u));
  uint v_3 = mix(16u, 0u, ((v_2 & 4294901760u) == 0u));
  uint v_4 = mix(8u, 0u, (((v_2 >> v_3) & 65280u) == 0u));
  uint v_5 = mix(4u, 0u, ((((v_2 >> v_3) >> v_4) & 240u) == 0u));
  uint v_6 = mix(2u, 0u, (((((v_2 >> v_3) >> v_4) >> v_5) & 12u) == 0u));
  int res = int(mix((v_3 | (v_4 | (v_5 | (v_6 | mix(1u, 0u, ((((((v_2 >> v_3) >> v_4) >> v_5) >> v_6) & 2u) == 0u)))))), 4294967295u, (((((v_2 >> v_3) >> v_4) >> v_5) >> v_6) == 0u)));
  return res;
}
void main() {
  v.inner = firstLeadingBit_57a1a3();
}
#version 310 es

layout(binding = 0, std430)
buffer prevent_dce_block_1_ssbo {
  int inner;
} v;
int firstLeadingBit_57a1a3() {
  int arg_0 = 1;
  uint v_1 = uint(arg_0);
  uint v_2 = mix(~(v_1), v_1, (v_1 < 2147483648u));
  uint v_3 = mix(16u, 0u, ((v_2 & 4294901760u) == 0u));
  uint v_4 = mix(8u, 0u, (((v_2 >> v_3) & 65280u) == 0u));
  uint v_5 = mix(4u, 0u, ((((v_2 >> v_3) >> v_4) & 240u) == 0u));
  uint v_6 = mix(2u, 0u, (((((v_2 >> v_3) >> v_4) >> v_5) & 12u) == 0u));
  int res = int(mix((v_3 | (v_4 | (v_5 | (v_6 | mix(1u, 0u, ((((((v_2 >> v_3) >> v_4) >> v_5) >> v_6) & 2u) == 0u)))))), 4294967295u, (((((v_2 >> v_3) >> v_4) >> v_5) >> v_6) == 0u)));
  return res;
}
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  v.inner = firstLeadingBit_57a1a3();
}
#version 310 es


struct VertexOutput {
  vec4 pos;
  int prevent_dce;
};

layout(location = 0) flat out int vertex_main_loc0_Output;
int firstLeadingBit_57a1a3() {
  int arg_0 = 1;
  uint v = uint(arg_0);
  uint v_1 = mix(~(v), v, (v < 2147483648u));
  uint v_2 = mix(16u, 0u, ((v_1 & 4294901760u) == 0u));
  uint v_3 = mix(8u, 0u, (((v_1 >> v_2) & 65280u) == 0u));
  uint v_4 = mix(4u, 0u, ((((v_1 >> v_2) >> v_3) & 240u) == 0u));
  uint v_5 = mix(2u, 0u, (((((v_1 >> v_2) >> v_3) >> v_4) & 12u) == 0u));
  int res = int(mix((v_2 | (v_3 | (v_4 | (v_5 | mix(1u, 0u, ((((((v_1 >> v_2) >> v_3) >> v_4) >> v_5) & 2u) == 0u)))))), 4294967295u, (((((v_1 >> v_2) >> v_3) >> v_4) >> v_5) == 0u)));
  return res;
}
VertexOutput vertex_main_inner() {
  VertexOutput tint_symbol = VertexOutput(vec4(0.0f), 0);
  tint_symbol.pos = vec4(0.0f);
  tint_symbol.prevent_dce = firstLeadingBit_57a1a3();
  return tint_symbol;
}
void main() {
  VertexOutput v_6 = vertex_main_inner();
  gl_Position = v_6.pos;
  gl_Position[1u] = -(gl_Position.y);
  gl_Position[2u] = ((2.0f * gl_Position.z) - gl_Position.w);
  vertex_main_loc0_Output = v_6.prevent_dce;
  gl_PointSize = 1.0f;
}
