RWByteAddressBuffer prevent_dce : register(u0);

uint4 subgroupBroadcast_279027() {
  uint4 res = WaveReadLaneAt((1u).xxxx, 1u);
  return res;
}

void fragment_main() {
  prevent_dce.Store4(0u, asuint(subgroupBroadcast_279027()));
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store4(0u, asuint(subgroupBroadcast_279027()));
  return;
}
