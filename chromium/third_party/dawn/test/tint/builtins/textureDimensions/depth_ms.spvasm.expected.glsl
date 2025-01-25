#version 310 es


struct vertex_main_out {
  vec4 tint_symbol_1_1;
};

vec4 tint_symbol_1 = vec4(0.0f);
uniform highp sampler2DMS arg_0;
void textureDimensions_f60bdb() {
  ivec2 res = ivec2(0);
  res = ivec2(uvec2(textureSize(arg_0)));
}
void tint_symbol_2(vec4 tint_symbol) {
  tint_symbol_1 = tint_symbol;
}
void vertex_main_1() {
  textureDimensions_f60bdb();
  tint_symbol_2(vec4(0.0f));
}
vertex_main_out vertex_main_inner() {
  vertex_main_1();
  return vertex_main_out(tint_symbol_1);
}
void main() {
  gl_Position = vertex_main_inner().tint_symbol_1_1;
  gl_Position[1u] = -(gl_Position.y);
  gl_Position[2u] = ((2.0f * gl_Position.z) - gl_Position.w);
  gl_PointSize = 1.0f;
}
#version 310 es
precision highp float;
precision highp int;

uniform highp sampler2DMS arg_0;
void textureDimensions_f60bdb() {
  ivec2 res = ivec2(0);
  res = ivec2(uvec2(textureSize(arg_0)));
}
void fragment_main_1() {
  textureDimensions_f60bdb();
}
void main() {
  fragment_main_1();
}
#version 310 es

uniform highp sampler2DMS arg_0;
void textureDimensions_f60bdb() {
  ivec2 res = ivec2(0);
  res = ivec2(uvec2(textureSize(arg_0)));
}
void compute_main_1() {
  textureDimensions_f60bdb();
}
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main_1();
}
