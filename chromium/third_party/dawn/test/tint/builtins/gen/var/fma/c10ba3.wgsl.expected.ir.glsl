SKIP: FAILED

#version 310 es
precision highp float;
precision highp int;


struct VertexOutput {
  vec4 pos;
  float prevent_dce;
};

float prevent_dce;
float fma_c10ba3() {
  float arg_0 = 1.0f;
  float arg_1 = 1.0f;
  float arg_2 = 1.0f;
  float res = fma(arg_0, arg_1, arg_2);
  return res;
}
void main() {
  prevent_dce = fma_c10ba3();
}
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  prevent_dce = fma_c10ba3();
}
VertexOutput main() {
  VertexOutput tint_symbol = VertexOutput(vec4(0.0f), 0.0f);
  tint_symbol.pos = vec4(0.0f);
  tint_symbol.prevent_dce = fma_c10ba3();
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
  float prevent_dce;
};

float prevent_dce;
float fma_c10ba3() {
  float arg_0 = 1.0f;
  float arg_1 = 1.0f;
  float arg_2 = 1.0f;
  float res = fma(arg_0, arg_1, arg_2);
  return res;
}
void main() {
  prevent_dce = fma_c10ba3();
}
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  prevent_dce = fma_c10ba3();
}
VertexOutput main() {
  VertexOutput tint_symbol = VertexOutput(vec4(0.0f), 0.0f);
  tint_symbol.pos = vec4(0.0f);
  tint_symbol.prevent_dce = fma_c10ba3();
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
  float prevent_dce;
};

float prevent_dce;
float fma_c10ba3() {
  float arg_0 = 1.0f;
  float arg_1 = 1.0f;
  float arg_2 = 1.0f;
  float res = fma(arg_0, arg_1, arg_2);
  return res;
}
void main() {
  prevent_dce = fma_c10ba3();
}
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  prevent_dce = fma_c10ba3();
}
VertexOutput main() {
  VertexOutput tint_symbol = VertexOutput(vec4(0.0f), 0.0f);
  tint_symbol.pos = vec4(0.0f);
  tint_symbol.prevent_dce = fma_c10ba3();
  return tint_symbol;
}
error: Error parsing GLSL shader:
ERROR: 0:16: 'fma' : required extension not requested: Possible extensions include:
GL_EXT_gpu_shader5
GL_OES_gpu_shader5
ERROR: 0:16: '' : compilation terminated 
ERROR: 2 compilation errors.  No code generated.




tint executable returned error: exit status 1
