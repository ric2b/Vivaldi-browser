SKIP: INVALID


@group(0) @binding(0) var<storage, read_write> prevent_dce : f32;

fn subgroupAdd_3854ae() -> f32 {
  var res : f32 = subgroupAdd(1.0f);
  return res;
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = subgroupAdd_3854ae();
}

Failed to generate: error: Unknown builtin method: 0x55c7fc6f1f58
