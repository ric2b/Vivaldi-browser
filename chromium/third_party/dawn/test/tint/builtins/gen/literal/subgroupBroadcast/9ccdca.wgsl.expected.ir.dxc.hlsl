
RWByteAddressBuffer prevent_dce : register(u0);
int subgroupBroadcast_9ccdca() {
  int res = WaveReadLaneAt(int(1), int(1));
  return res;
}

void fragment_main() {
  prevent_dce.Store(0u, asuint(subgroupBroadcast_9ccdca()));
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store(0u, asuint(subgroupBroadcast_9ccdca()));
}

