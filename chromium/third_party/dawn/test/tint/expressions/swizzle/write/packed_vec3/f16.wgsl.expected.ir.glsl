#version 310 es
#extension GL_AMD_gpu_shader_half_float: require


struct S {
  f16vec3 v;
};

layout(binding = 0, std430)
buffer tint_symbol_1_1_ssbo {
  S tint_symbol;
} v_1;
void f() {
  v_1.tint_symbol.v = f16vec3(1.0hf, 2.0hf, 3.0hf);
  v_1.tint_symbol.v[0u] = 1.0hf;
  v_1.tint_symbol.v[1u] = 2.0hf;
  v_1.tint_symbol.v[2u] = 3.0hf;
}
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
}
