SKIP: INVALID


@group(0) @binding(0) var<storage, read_write> prevent_dce : f32;

fn subgroupMul_0de9d3() -> f32 {
  var arg_0 = 1.0f;
  var res : f32 = subgroupMul(arg_0);
  return res;
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = subgroupMul_0de9d3();
}

Failed to generate: error: Unknown builtin method: 0x5652d5c8e1c0
