SKIP: INVALID


@group(0) @binding(0) var<storage, read_write> prevent_dce : f32;

fn subgroupExclusiveAdd_967e38() -> f32 {
  var res : f32 = subgroupExclusiveAdd(1.0f);
  return res;
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = subgroupExclusiveAdd_967e38();
}

Failed to generate: error: Unknown builtin method: 0x557874a96f58
