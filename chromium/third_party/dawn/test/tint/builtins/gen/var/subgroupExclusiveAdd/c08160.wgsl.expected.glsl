SKIP: INVALID


@group(0) @binding(0) var<storage, read_write> prevent_dce : vec3<i32>;

fn subgroupExclusiveAdd_c08160() -> vec3<i32> {
  var arg_0 = vec3<i32>(1i);
  var res : vec3<i32> = subgroupExclusiveAdd(arg_0);
  return res;
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = subgroupExclusiveAdd_c08160();
}

Failed to generate: error: Unknown builtin method: 0x563e505b2498
