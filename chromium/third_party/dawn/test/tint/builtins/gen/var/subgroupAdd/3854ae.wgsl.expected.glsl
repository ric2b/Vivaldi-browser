SKIP: INVALID


@group(0) @binding(0) var<storage, read_write> prevent_dce : f32;

fn subgroupAdd_3854ae() -> f32 {
  var arg_0 = 1.0f;
  var res : f32 = subgroupAdd(arg_0);
  return res;
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = subgroupAdd_3854ae();
}

Failed to generate: error: Unknown builtin method: 0x555aee42c1c0
