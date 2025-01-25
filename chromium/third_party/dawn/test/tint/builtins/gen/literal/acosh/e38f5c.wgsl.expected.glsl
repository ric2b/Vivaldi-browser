#version 310 es
precision highp float;
precision highp int;

layout(binding = 0, std430) buffer prevent_dce_block_ssbo {
  vec3 inner;
  uint pad;
} prevent_dce;

vec3 acosh_e38f5c() {
  vec3 res = vec3(1.0f);
  return res;
}

struct VertexOutput {
  vec4 pos;
  vec3 prevent_dce;
};

void fragment_main() {
  prevent_dce.inner = acosh_e38f5c();
}

void main() {
  fragment_main();
  return;
}
#version 310 es

layout(binding = 0, std430) buffer prevent_dce_block_ssbo {
  vec3 inner;
  uint pad;
} prevent_dce;

vec3 acosh_e38f5c() {
  vec3 res = vec3(1.0f);
  return res;
}

struct VertexOutput {
  vec4 pos;
  vec3 prevent_dce;
};

void compute_main() {
  prevent_dce.inner = acosh_e38f5c();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}
#version 310 es

layout(location = 0) flat out vec3 prevent_dce_1;
vec3 acosh_e38f5c() {
  vec3 res = vec3(1.0f);
  return res;
}

struct VertexOutput {
  vec4 pos;
  vec3 prevent_dce;
};

VertexOutput vertex_main() {
  VertexOutput tint_symbol = VertexOutput(vec4(0.0f, 0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, 0.0f));
  tint_symbol.pos = vec4(0.0f);
  tint_symbol.prevent_dce = acosh_e38f5c();
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
