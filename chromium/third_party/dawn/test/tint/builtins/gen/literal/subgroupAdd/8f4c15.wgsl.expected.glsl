SKIP: INVALID


@group(0) @binding(0) var<storage, read_write> prevent_dce : vec4<f32>;

fn subgroupAdd_8f4c15() -> vec4<f32> {
  var res : vec4<f32> = subgroupAdd(vec4<f32>(1.0f));
  return res;
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = subgroupAdd_8f4c15();
}

Failed to generate: error: Unknown builtin method: 0x560a73a9b230
