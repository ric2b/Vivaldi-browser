SKIP: FAILED

#version 310 es
precision highp float;
precision highp int;


struct VertexOutput {
  vec4 pos;
  vec2 prevent_dce;
};

vec2 prevent_dce;
vec2 fma_26a7a9() {
  vec2 arg_0 = vec2(1.0f);
  vec2 arg_1 = vec2(1.0f);
  vec2 arg_2 = vec2(1.0f);
  vec2 res = fma(arg_0, arg_1, arg_2);
  return res;
}
void main() {
  prevent_dce = fma_26a7a9();
}
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  prevent_dce = fma_26a7a9();
}
VertexOutput main() {
  VertexOutput tint_symbol = VertexOutput(vec4(0.0f), vec2(0.0f));
  tint_symbol.pos = vec4(0.0f);
  tint_symbol.prevent_dce = fma_26a7a9();
  return tint_symbol;
}
error: Error parsing GLSL shader:
ERROR: 0:16: 'fma' : required extension not requested: Possible extensions include:
GL_EXT_gpu_shader5
GL_OES_gpu_shader5
ERROR: 0:16: '' : compilation terminated 
ERROR: 2 compilation errors.  No code generated.



#version 310 es
precision highp float;
precision highp int;


struct VertexOutput {
  vec4 pos;
  vec2 prevent_dce;
};

vec2 prevent_dce;
vec2 fma_26a7a9() {
  vec2 arg_0 = vec2(1.0f);
  vec2 arg_1 = vec2(1.0f);
  vec2 arg_2 = vec2(1.0f);
  vec2 res = fma(arg_0, arg_1, arg_2);
  return res;
}
void main() {
  prevent_dce = fma_26a7a9();
}
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  prevent_dce = fma_26a7a9();
}
VertexOutput main() {
  VertexOutput tint_symbol = VertexOutput(vec4(0.0f), vec2(0.0f));
  tint_symbol.pos = vec4(0.0f);
  tint_symbol.prevent_dce = fma_26a7a9();
  return tint_symbol;
}
error: Error parsing GLSL shader:
ERROR: 0:16: 'fma' : required extension not requested: Possible extensions include:
GL_EXT_gpu_shader5
GL_OES_gpu_shader5
ERROR: 0:16: '' : compilation terminated 
ERROR: 2 compilation errors.  No code generated.



#version 310 es
precision highp float;
precision highp int;


struct VertexOutput {
  vec4 pos;
  vec2 prevent_dce;
};

vec2 prevent_dce;
vec2 fma_26a7a9() {
  vec2 arg_0 = vec2(1.0f);
  vec2 arg_1 = vec2(1.0f);
  vec2 arg_2 = vec2(1.0f);
  vec2 res = fma(arg_0, arg_1, arg_2);
  return res;
}
void main() {
  prevent_dce = fma_26a7a9();
}
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  prevent_dce = fma_26a7a9();
}
VertexOutput main() {
  VertexOutput tint_symbol = VertexOutput(vec4(0.0f), vec2(0.0f));
  tint_symbol.pos = vec4(0.0f);
  tint_symbol.prevent_dce = fma_26a7a9();
  return tint_symbol;
}
error: Error parsing GLSL shader:
ERROR: 0:16: 'fma' : required extension not requested: Possible extensions include:
GL_EXT_gpu_shader5
GL_OES_gpu_shader5
ERROR: 0:16: '' : compilation terminated 
ERROR: 2 compilation errors.  No code generated.




tint executable returned error: exit status 1
