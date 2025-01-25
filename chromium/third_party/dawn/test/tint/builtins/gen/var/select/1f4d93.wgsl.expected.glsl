#version 310 es
precision highp float;
precision highp int;

vec2 tint_select(vec2 param_0, vec2 param_1, bvec2 param_2) {
    return vec2(param_2[0] ? param_1[0] : param_0[0], param_2[1] ? param_1[1] : param_0[1]);
}


void select_1f4d93() {
  bvec2 arg_2 = bvec2(true);
  vec2 res = tint_select(vec2(1.0f), vec2(1.0f), arg_2);
}

struct VertexOutput {
  vec4 pos;
};

void fragment_main() {
  select_1f4d93();
}

void main() {
  fragment_main();
  return;
}
#version 310 es

vec2 tint_select(vec2 param_0, vec2 param_1, bvec2 param_2) {
    return vec2(param_2[0] ? param_1[0] : param_0[0], param_2[1] ? param_1[1] : param_0[1]);
}


void select_1f4d93() {
  bvec2 arg_2 = bvec2(true);
  vec2 res = tint_select(vec2(1.0f), vec2(1.0f), arg_2);
}

struct VertexOutput {
  vec4 pos;
};

void compute_main() {
  select_1f4d93();
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


void select_1f4d93() {
  bvec2 arg_2 = bvec2(true);
  vec2 res = tint_select(vec2(1.0f), vec2(1.0f), arg_2);
}

struct VertexOutput {
  vec4 pos;
};

VertexOutput vertex_main() {
  VertexOutput tint_symbol = VertexOutput(vec4(0.0f, 0.0f, 0.0f, 0.0f));
  tint_symbol.pos = vec4(0.0f);
  select_1f4d93();
  return tint_symbol;
}

void main() {
  gl_PointSize = 1.0;
  VertexOutput inner_result = vertex_main();
  gl_Position = inner_result.pos;
  gl_Position.y = -(gl_Position.y);
  gl_Position.z = ((2.0f * gl_Position.z) - gl_Position.w);
  return;
}
