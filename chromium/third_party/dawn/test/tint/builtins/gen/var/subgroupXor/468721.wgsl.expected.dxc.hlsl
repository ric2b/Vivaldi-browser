RWByteAddressBuffer prevent_dce : register(u0);

uint3 subgroupXor_468721() {
  uint3 arg_0 = (1u).xxx;
  uint3 res = WaveActiveBitXor(arg_0);
  return res;
}

void fragment_main() {
  prevent_dce.Store3(0u, asuint(subgroupXor_468721()));
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store3(0u, asuint(subgroupXor_468721()));
  return;
}
