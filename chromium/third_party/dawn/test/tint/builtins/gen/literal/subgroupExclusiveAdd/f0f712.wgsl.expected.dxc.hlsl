RWByteAddressBuffer prevent_dce : register(u0);

int2 subgroupExclusiveAdd_f0f712() {
  int2 res = WavePrefixSum((1).xx);
  return res;
}

void fragment_main() {
  prevent_dce.Store2(0u, asuint(subgroupExclusiveAdd_f0f712()));
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store2(0u, asuint(subgroupExclusiveAdd_f0f712()));
  return;
}
