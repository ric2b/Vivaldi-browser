SKIP: INVALID


@group(0) @binding(0) var<storage, read_write> prevent_dce : vec3<i32>;

fn subgroupMul_5a8c86() -> vec3<i32> {
  var res : vec3<i32> = subgroupMul(vec3<i32>(1i));
  return res;
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = subgroupMul_5a8c86();
}

Failed to generate: error: Unknown builtin method: 0x55d485901230
