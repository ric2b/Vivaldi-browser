SKIP: INVALID


enable f16;

@group(0) @binding(0) var<storage, read_write> prevent_dce : vec3<f16>;

fn subgroupExclusiveMul_13ba26() -> vec3<f16> {
  var res : vec3<f16> = subgroupExclusiveMul(vec3<f16>(1.0h));
  return res;
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = subgroupExclusiveMul_13ba26();
}

Failed to generate: error: Unknown builtin method: 0x564d1f8be230
