SKIP: INVALID


@group(0) @binding(0) var<storage, read_write> prevent_dce : vec2<i32>;

fn subgroupAdd_1eb429() -> vec2<i32> {
  var res : vec2<i32> = subgroupAdd(vec2<i32>(1i));
  return res;
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = subgroupAdd_1eb429();
}

Failed to generate: error: Unknown builtin method: 0x55c8ea9d2230
