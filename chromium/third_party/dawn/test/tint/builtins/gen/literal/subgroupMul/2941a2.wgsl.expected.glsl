SKIP: INVALID


enable f16;

@group(0) @binding(0) var<storage, read_write> prevent_dce : f16;

fn subgroupMul_2941a2() -> f16 {
  var res : f16 = subgroupMul(1.0h);
  return res;
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = subgroupMul_2941a2();
}

Failed to generate: error: Unknown builtin method: 0x55ec5f3bbf58
