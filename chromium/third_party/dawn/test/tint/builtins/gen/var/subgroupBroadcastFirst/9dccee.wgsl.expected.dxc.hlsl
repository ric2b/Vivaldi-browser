RWByteAddressBuffer prevent_dce : register(u0);

int4 subgroupBroadcastFirst_9dccee() {
  int4 arg_0 = (1).xxxx;
  int4 res = WaveReadLaneFirst(arg_0);
  return res;
}

void fragment_main() {
  prevent_dce.Store4(0u, asuint(subgroupBroadcastFirst_9dccee()));
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store4(0u, asuint(subgroupBroadcastFirst_9dccee()));
  return;
}
