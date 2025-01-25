#version 310 es
#extension GL_AMD_gpu_shader_half_float: require

float16_t t = 0.0hf;
f16vec4 m() {
  t = 1.0hf;
  return f16vec4(t);
}
uvec4 tint_v4f16_to_v4u32(f16vec4 value) {
  uvec4 v_1 = uvec4(value);
  uvec4 v_2 = mix(uvec4(0u), v_1, greaterThanEqual(value, f16vec4(0.0hf)));
  return mix(uvec4(4294967295u), v_2, lessThanEqual(value, f16vec4(65504.0hf)));
}
void f() {
  uvec4 v = tint_v4f16_to_v4u32(m());
}
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
}
