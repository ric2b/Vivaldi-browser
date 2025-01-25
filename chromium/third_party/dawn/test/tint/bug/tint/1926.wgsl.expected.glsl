#version 310 es

shared uint sh_atomic_failed;
layout(binding = 4, std430)
buffer tint_symbol_block_1_ssbo {
  uint inner;
} v;
void tint_symbol_1_inner(uvec3 global_id, uvec3 local_id, uint tint_local_index) {
  if ((tint_local_index == 0u)) {
    sh_atomic_failed = 0u;
  }
  barrier();
  barrier();
  uint v_1 = sh_atomic_failed;
  barrier();
  uint failed = v_1;
  if ((local_id[0u] == 0u)) {
    v.inner = failed;
  }
}
layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;
void main() {
  tint_symbol_1_inner(gl_GlobalInvocationID, gl_LocalInvocationID, gl_LocalInvocationIndex);
}
