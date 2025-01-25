#version 310 es
#extension GL_AMD_gpu_shader_half_float : require

shared f16mat3 w;
void tint_zero_workgroup_memory(uint local_idx) {
  if ((local_idx < 1u)) {
    w = f16mat3(f16vec3(0.0hf), f16vec3(0.0hf), f16vec3(0.0hf));
  }
  barrier();
}

layout(binding = 0, std140) uniform u_block_std140_ubo {
  f16vec3 inner_0;
  f16vec3 inner_1;
  f16vec3 inner_2;
} u;

f16mat3 load_u_inner() {
  return f16mat3(u.inner_0, u.inner_1, u.inner_2);
}

void f(uint local_invocation_index) {
  tint_zero_workgroup_memory(local_invocation_index);
  w = load_u_inner();
  w[1] = u.inner_0;
  w[1] = u.inner_0.zxy;
  w[0][1] = u.inner_1[0u];
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  f(gl_LocalInvocationIndex);
  return;
}
