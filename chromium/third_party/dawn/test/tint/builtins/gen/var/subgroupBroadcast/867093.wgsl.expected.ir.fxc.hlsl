SKIP: INVALID


RWByteAddressBuffer prevent_dce : register(u0);
float subgroupBroadcast_867093() {
  float arg_0 = 1.0f;
  float res = WaveReadLaneAt(arg_0, 1);
  return res;
}

void fragment_main() {
  prevent_dce.Store(0u, asuint(subgroupBroadcast_867093()));
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store(0u, asuint(subgroupBroadcast_867093()));
}

FXC validation failure:
<scrubbed_path>(5,15-38): error X3004: undeclared identifier 'WaveReadLaneAt'


tint executable returned error: exit status 1
