SKIP: INVALID


@group(0) @binding(0) var<storage, read_write> prevent_dce : vec3<u32>;

fn subgroupExclusiveAdd_0ff95a() -> vec3<u32> {
  var res : vec3<u32> = subgroupExclusiveAdd(vec3<u32>(1u));
  return res;
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = subgroupExclusiveAdd_0ff95a();
}

Failed to generate: error: Unknown builtin method: 0x55bc8779a230
