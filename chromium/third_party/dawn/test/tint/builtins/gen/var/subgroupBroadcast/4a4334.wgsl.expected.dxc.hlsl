RWByteAddressBuffer prevent_dce : register(u0);

uint2 subgroupBroadcast_4a4334() {
  uint2 arg_0 = (1u).xx;
  uint2 res = WaveReadLaneAt(arg_0, 1u);
  return res;
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store2(0u, asuint(subgroupBroadcast_4a4334()));
  return;
}
