SKIP: INVALID


enable chromium_experimental_subgroups;

@group(0) @binding(0) var<storage, read_write> tint_symbol : array<u32>;

@fragment
fn tint_symbol_1(@builtin(subgroup_invocation_id) subgroup_invocation_id : u32, @builtin(subgroup_size) subgroup_size : u32) {
  tint_symbol[subgroup_invocation_id] = subgroup_size;
}

Failed to generate: <dawn>/test/tint/types/functions/shader_io/fragment_subgroup_builtins.wgsl:1:8 error: GLSL backend does not support extension 'chromium_experimental_subgroups'
enable chromium_experimental_subgroups;
       ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^


tint executable returned error: exit status 1
