RWByteAddressBuffer prevent_dce : register(u0);

uint3 subgroupExclusiveAdd_0ff95a() {
  uint3 arg_0 = (1u).xxx;
  uint3 res = WavePrefixSum(arg_0);
  return res;
}

void fragment_main() {
  prevent_dce.Store3(0u, asuint(subgroupExclusiveAdd_0ff95a()));
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store3(0u, asuint(subgroupExclusiveAdd_0ff95a()));
  return;
}
