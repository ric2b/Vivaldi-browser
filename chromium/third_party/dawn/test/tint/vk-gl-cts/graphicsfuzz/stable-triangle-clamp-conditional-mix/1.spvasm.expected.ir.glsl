SKIP: FAILED

#version 310 es

struct buf0 {
  vec2 resolution;
};

struct main_out {
  vec4 x_GLF_color_1;
};
precision highp float;
precision highp int;


uniform buf0 x_15;
vec4 tint_symbol = vec4(0.0f);
vec4 x_GLF_color = vec4(0.0f);
float cross2d_vf2_vf2_(inout vec2 a, inout vec2 b) {
  float x_85 = a.x;
  float x_87 = b.y;
  float x_90 = b.x;
  float x_92 = a.y;
  return ((x_85 * x_87) - (x_90 * x_92));
}
int pointInTriangle_vf2_vf2_vf2_vf2_(inout vec2 p, inout vec2 a_1, inout vec2 b_1, inout vec2 c) {
  float var_y = 0.0f;
  float x_96 = 0.0f;
  float x_97 = 0.0f;
  float clamp_y = 0.0f;
  float pab = 0.0f;
  vec2 param = vec2(0.0f);
  vec2 param_1 = vec2(0.0f);
  float pbc = 0.0f;
  vec2 param_2 = vec2(0.0f);
  vec2 param_3 = vec2(0.0f);
  float pca = 0.0f;
  vec2 param_4 = vec2(0.0f);
  vec2 param_5 = vec2(0.0f);
  bool x_173 = false;
  bool x_174 = false;
  bool x_205 = false;
  bool x_206 = false;
  if ((x_15.resolution.x == x_15.resolution.y)) {
    float x_107 = c.y;
    vec2 x_108 = vec2(0.0f, x_107);
    if (true) {
      x_97 = c.y;
    } else {
      x_97 = 1.0f;
    }
    vec2 x_116 = vec2(1.0f, max(x_97, c.y));
    vec2 x_117 = x_108.xy;
    x_96 = x_107;
  } else {
    x_96 = -1.0f;
  }
  var_y = x_96;
  clamp_y = clamp(c.y, c.y, var_y);
  float x_136 = b_1.x;
  float x_137 = a_1.x;
  float x_140 = b_1.y;
  float x_141 = a_1.y;
  param = vec2((p.x - a_1.x), (p.y - a_1.y));
  param_1 = vec2((x_136 - x_137), (x_140 - x_141));
  float x_144 = cross2d_vf2_vf2_(param, param_1);
  pab = x_144;
  float x_153 = c.x;
  float x_154 = b_1.x;
  float x_156 = clamp_y;
  float x_157 = b_1.y;
  param_2 = vec2((p.x - b_1.x), (p.y - b_1.y));
  param_3 = vec2((x_153 - x_154), (x_156 - x_157));
  float x_160 = cross2d_vf2_vf2_(param_2, param_3);
  pbc = x_160;
  bool x_165 = ((pab < 0.0f) & (pbc < 0.0f));
  x_174 = x_165;
  if (!(x_165)) {
    x_173 = ((pab >= 0.0f) & (pbc >= 0.0f));
    x_174 = x_173;
  }
  if (!(x_174)) {
    return 0;
  }
  float x_185 = a_1.x;
  float x_186 = c.x;
  float x_188 = a_1.y;
  float x_189 = c.y;
  param_4 = vec2((p.x - c.x), (p.y - c.y));
  param_5 = vec2((x_185 - x_186), (x_188 - x_189));
  float x_192 = cross2d_vf2_vf2_(param_4, param_5);
  pca = x_192;
  bool x_197 = ((pab < 0.0f) & (pca < 0.0f));
  x_206 = x_197;
  if (!(x_197)) {
    x_205 = ((pab >= 0.0f) & (pca >= 0.0f));
    x_206 = x_205;
  }
  if (!(x_206)) {
    return 0;
  }
  return 1;
}
void main_1() {
  vec2 pos = vec2(0.0f);
  vec2 param_6 = vec2(0.0f);
  vec2 param_7 = vec2(0.0f);
  vec2 param_8 = vec2(0.0f);
  vec2 param_9 = vec2(0.0f);
  pos = (tint_symbol.xy / x_15.resolution);
  param_6 = pos;
  param_7 = vec2(0.69999998807907104492f, 0.30000001192092895508f);
  param_8 = vec2(0.5f, 0.89999997615814208984f);
  param_9 = vec2(0.10000000149011611938f, 0.40000000596046447754f);
  int x_78 = pointInTriangle_vf2_vf2_vf2_vf2_(param_6, param_7, param_8, param_9);
  if ((x_78 == 1)) {
    x_GLF_color = vec4(1.0f, 0.0f, 0.0f, 1.0f);
  } else {
    x_GLF_color = vec4(0.0f, 0.0f, 0.0f, 1.0f);
  }
}
main_out main(vec4 tint_symbol_2) {
  tint_symbol = tint_symbol_2;
  main_1();
  return main_out(x_GLF_color);
}
error: Error parsing GLSL shader:
ERROR: 0:4: 'float' : type requires declaration of default precision qualifier 
ERROR: 0:4: '' : compilation terminated 
ERROR: 2 compilation errors.  No code generated.




tint executable returned error: exit status 1
