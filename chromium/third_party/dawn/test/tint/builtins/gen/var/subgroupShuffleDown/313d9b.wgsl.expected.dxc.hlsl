RWByteAddressBuffer prevent_dce : register(u0);

int4 subgroupShuffleDown_313d9b() {
  int4 arg_0 = (1).xxxx;
  uint arg_1 = 1u;
  int4 res = WaveReadLaneAt(arg_0, (WaveGetLaneIndex() + arg_1));
  return res;
}

void fragment_main() {
  prevent_dce.Store4(0u, asuint(subgroupShuffleDown_313d9b()));
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store4(0u, asuint(subgroupShuffleDown_313d9b()));
  return;
}
