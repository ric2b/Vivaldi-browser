SKIP: INVALID


enable f16;

@group(0) @binding(0) var<storage, read_write> prevent_dce : vec3<f16>;

fn subgroupMul_53aee2() -> vec3<f16> {
  var res : vec3<f16> = subgroupMul(vec3<f16>(1.0h));
  return res;
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = subgroupMul_53aee2();
}

Failed to generate: error: Unknown builtin method: 0x5624f24e6230
