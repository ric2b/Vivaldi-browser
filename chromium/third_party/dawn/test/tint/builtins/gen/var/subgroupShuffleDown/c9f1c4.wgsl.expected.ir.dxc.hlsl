
RWByteAddressBuffer prevent_dce : register(u0);
uint2 subgroupShuffleDown_c9f1c4() {
  uint2 arg_0 = (1u).xx;
  uint arg_1 = 1u;
  uint2 v = arg_0;
  uint v_1 = arg_1;
  uint2 res = WaveReadLaneAt(v, (WaveGetLaneIndex() + v_1));
  return res;
}

void fragment_main() {
  prevent_dce.Store2(0u, subgroupShuffleDown_c9f1c4());
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store2(0u, subgroupShuffleDown_c9f1c4());
}

