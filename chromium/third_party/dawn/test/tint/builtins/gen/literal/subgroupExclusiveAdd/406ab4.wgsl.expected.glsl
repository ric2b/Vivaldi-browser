SKIP: INVALID


@group(0) @binding(0) var<storage, read_write> prevent_dce : vec4<i32>;

fn subgroupExclusiveAdd_406ab4() -> vec4<i32> {
  var res : vec4<i32> = subgroupExclusiveAdd(vec4<i32>(1i));
  return res;
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = subgroupExclusiveAdd_406ab4();
}

Failed to generate: error: Unknown builtin method: 0x56172edaf230
