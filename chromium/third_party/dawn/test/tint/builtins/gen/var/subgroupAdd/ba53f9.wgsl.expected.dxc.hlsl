RWByteAddressBuffer prevent_dce : register(u0);

int subgroupAdd_ba53f9() {
  int arg_0 = 1;
  int res = WaveActiveSum(arg_0);
  return res;
}

void fragment_main() {
  prevent_dce.Store(0u, asuint(subgroupAdd_ba53f9()));
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store(0u, asuint(subgroupAdd_ba53f9()));
  return;
}
