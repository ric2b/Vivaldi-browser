#version 310 es
precision highp float;
precision highp int;

ivec2 tint_select(ivec2 param_0, ivec2 param_1, bvec2 param_2) {
    return ivec2(param_2[0] ? param_1[0] : param_0[0], param_2[1] ? param_1[1] : param_0[1]);
}


layout(binding = 0, std430) buffer prevent_dce_block_ssbo {
  ivec2 inner;
} prevent_dce;

ivec2 select_00b848() {
  ivec2 arg_0 = ivec2(1);
  ivec2 arg_1 = ivec2(1);
  bvec2 arg_2 = bvec2(true);
  ivec2 res = tint_select(arg_0, arg_1, arg_2);
  return res;
}

struct VertexOutput {
  vec4 pos;
  ivec2 prevent_dce;
};

void fragment_main() {
  prevent_dce.inner = select_00b848();
}

void main() {
  fragment_main();
  return;
}
#version 310 es

ivec2 tint_select(ivec2 param_0, ivec2 param_1, bvec2 param_2) {
    return ivec2(param_2[0] ? param_1[0] : param_0[0], param_2[1] ? param_1[1] : param_0[1]);
}


layout(binding = 0, std430) buffer prevent_dce_block_ssbo {
  ivec2 inner;
} prevent_dce;

ivec2 select_00b848() {
  ivec2 arg_0 = ivec2(1);
  ivec2 arg_1 = ivec2(1);
  bvec2 arg_2 = bvec2(true);
  ivec2 res = tint_select(arg_0, arg_1, arg_2);
  return res;
}

struct VertexOutput {
  vec4 pos;
  ivec2 prevent_dce;
};

void compute_main() {
  prevent_dce.inner = select_00b848();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}
#version 310 es

ivec2 tint_select(ivec2 param_0, ivec2 param_1, bvec2 param_2) {
    return ivec2(param_2[0] ? param_1[0] : param_0[0], param_2[1] ? param_1[1] : param_0[1]);
}


layout(location = 0) flat out ivec2 prevent_dce_1;
ivec2 select_00b848() {
  ivec2 arg_0 = ivec2(1);
  ivec2 arg_1 = ivec2(1);
  bvec2 arg_2 = bvec2(true);
  ivec2 res = tint_select(arg_0, arg_1, arg_2);
  return res;
}

struct VertexOutput {
  vec4 pos;
  ivec2 prevent_dce;
};

VertexOutput vertex_main() {
  VertexOutput tint_symbol = VertexOutput(vec4(0.0f, 0.0f, 0.0f, 0.0f), ivec2(0, 0));
  tint_symbol.pos = vec4(0.0f);
  tint_symbol.prevent_dce = select_00b848();
  return tint_symbol;
}

void main() {
  gl_PointSize = 1.0;
  VertexOutput inner_result = vertex_main();
  gl_Position = inner_result.pos;
  prevent_dce_1 = inner_result.prevent_dce;
  gl_Position.y = -(gl_Position.y);
  gl_Position.z = ((2.0f * gl_Position.z) - gl_Position.w);
  return;
}
