SKIP: INVALID


enable subgroups;
enable subgroups_f16;
enable f16;

@group(0) @binding(0) var<storage, read_write> prevent_dce : vec3<f16>;

fn subgroupShuffle_821df9() -> vec3<f16> {
  var arg_0 = vec3<f16>(1.0h);
  var arg_1 = 1i;
  var res : vec3<f16> = subgroupShuffle(arg_0, arg_1);
  return res;
}

@fragment
fn fragment_main() {
  prevent_dce = subgroupShuffle_821df9();
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = subgroupShuffle_821df9();
}

Failed to generate: <dawn>/test/tint/builtins/gen/var/subgroupShuffle/821df9.wgsl:41:8 error: GLSL backend does not support extension 'subgroups'
enable subgroups;
       ^^^^^^^^^


enable subgroups;
enable subgroups_f16;
enable f16;

@group(0) @binding(0) var<storage, read_write> prevent_dce : vec3<f16>;

fn subgroupShuffle_821df9() -> vec3<f16> {
  var arg_0 = vec3<f16>(1.0h);
  var arg_1 = 1i;
  var res : vec3<f16> = subgroupShuffle(arg_0, arg_1);
  return res;
}

@fragment
fn fragment_main() {
  prevent_dce = subgroupShuffle_821df9();
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = subgroupShuffle_821df9();
}

Failed to generate: <dawn>/test/tint/builtins/gen/var/subgroupShuffle/821df9.wgsl:41:8 error: GLSL backend does not support extension 'subgroups'
enable subgroups;
       ^^^^^^^^^

