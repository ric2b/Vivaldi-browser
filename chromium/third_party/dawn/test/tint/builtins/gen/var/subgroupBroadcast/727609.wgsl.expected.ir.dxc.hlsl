
RWByteAddressBuffer prevent_dce : register(u0);
uint4 subgroupBroadcast_727609() {
  uint4 arg_0 = (1u).xxxx;
  uint4 res = WaveReadLaneAt(arg_0, 1);
  return res;
}

void fragment_main() {
  prevent_dce.Store4(0u, subgroupBroadcast_727609());
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store4(0u, subgroupBroadcast_727609());
}

