RWByteAddressBuffer prevent_dce : register(u0);

int3 subgroupMul_5a8c86() {
  int3 res = WaveActiveProduct((1).xxx);
  return res;
}

void fragment_main() {
  prevent_dce.Store3(0u, asuint(subgroupMul_5a8c86()));
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store3(0u, asuint(subgroupMul_5a8c86()));
  return;
}
