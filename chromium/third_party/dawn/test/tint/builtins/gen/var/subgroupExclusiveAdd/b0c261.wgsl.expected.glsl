SKIP: INVALID


@group(0) @binding(0) var<storage, read_write> prevent_dce : i32;

fn subgroupExclusiveAdd_b0c261() -> i32 {
  var arg_0 = 1i;
  var res : i32 = subgroupExclusiveAdd(arg_0);
  return res;
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = subgroupExclusiveAdd_b0c261();
}

Failed to generate: error: Unknown builtin method: 0x55cff0f391c0
