SKIP: FAILED

#version 310 es

struct main_out {
  vec4 x_GLF_color_1;
};
precision highp float;
precision highp int;


vec4 x_GLF_color = vec4(0.0f);
void main_1() {
  bool a = false;
  a = false;
  bool x_19 = a;
  if ((true & x_19)) {
    x_GLF_color = vec4(0.0f);
  } else {
    x_GLF_color = vec4(1.0f, 0.0f, 0.0f, 1.0f);
  }
}
main_out main() {
  main_1();
  return main_out(x_GLF_color);
}
error: Error parsing GLSL shader:
ERROR: 0:4: 'float' : type requires declaration of default precision qualifier 
ERROR: 0:4: '' : compilation terminated 
ERROR: 2 compilation errors.  No code generated.




tint executable returned error: exit status 1
