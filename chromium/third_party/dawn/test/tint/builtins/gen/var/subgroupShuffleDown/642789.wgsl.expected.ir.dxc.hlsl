
RWByteAddressBuffer prevent_dce : register(u0);
uint3 subgroupShuffleDown_642789() {
  uint3 arg_0 = (1u).xxx;
  uint arg_1 = 1u;
  uint3 v = arg_0;
  uint v_1 = arg_1;
  uint3 res = WaveReadLaneAt(v, (WaveGetLaneIndex() + v_1));
  return res;
}

void fragment_main() {
  prevent_dce.Store3(0u, subgroupShuffleDown_642789());
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store3(0u, subgroupShuffleDown_642789());
}

