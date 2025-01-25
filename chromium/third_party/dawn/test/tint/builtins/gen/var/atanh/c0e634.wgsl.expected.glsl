#version 310 es
precision highp float;
precision highp int;

vec2 tint_select(vec2 param_0, vec2 param_1, bvec2 param_2) {
    return vec2(param_2[0] ? param_1[0] : param_0[0], param_2[1] ? param_1[1] : param_0[1]);
}


vec2 tint_atanh(vec2 x) {
  return tint_select(atanh(x), vec2(0.0f), greaterThanEqual(x, vec2(1.0f)));
}

layout(binding = 0, std430) buffer prevent_dce_block_ssbo {
  vec2 inner;
} prevent_dce;

vec2 atanh_c0e634() {
  vec2 arg_0 = vec2(0.5f);
  vec2 res = tint_atanh(arg_0);
  return res;
}

struct VertexOutput {
  vec4 pos;
  vec2 prevent_dce;
};

void fragment_main() {
  prevent_dce.inner = atanh_c0e634();
}

void main() {
  fragment_main();
  return;
}
#version 310 es

vec2 tint_select(vec2 param_0, vec2 param_1, bvec2 param_2) {
    return vec2(param_2[0] ? param_1[0] : param_0[0], param_2[1] ? param_1[1] : param_0[1]);
}


vec2 tint_atanh(vec2 x) {
  return tint_select(atanh(x), vec2(0.0f), greaterThanEqual(x, vec2(1.0f)));
}

layout(binding = 0, std430) buffer prevent_dce_block_ssbo {
  vec2 inner;
} prevent_dce;

vec2 atanh_c0e634() {
  vec2 arg_0 = vec2(0.5f);
  vec2 res = tint_atanh(arg_0);
  return res;
}

struct VertexOutput {
  vec4 pos;
  vec2 prevent_dce;
};

void compute_main() {
  prevent_dce.inner = atanh_c0e634();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}
#version 310 es

vec2 tint_select(vec2 param_0, vec2 param_1, bvec2 param_2) {
    return vec2(param_2[0] ? param_1[0] : param_0[0], param_2[1] ? param_1[1] : param_0[1]);
}


vec2 tint_atanh(vec2 x) {
  return tint_select(atanh(x), vec2(0.0f), greaterThanEqual(x, vec2(1.0f)));
}

layout(location = 0) flat out vec2 prevent_dce_1;
vec2 atanh_c0e634() {
  vec2 arg_0 = vec2(0.5f);
  vec2 res = tint_atanh(arg_0);
  return res;
}

struct VertexOutput {
  vec4 pos;
  vec2 prevent_dce;
};

VertexOutput vertex_main() {
  VertexOutput tint_symbol = VertexOutput(vec4(0.0f, 0.0f, 0.0f, 0.0f), vec2(0.0f, 0.0f));
  tint_symbol.pos = vec4(0.0f);
  tint_symbol.prevent_dce = atanh_c0e634();
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
