
RWByteAddressBuffer prevent_dce : register(u0);
uint4 quadBroadcast_b68331() {
  uint4 arg_0 = (1u).xxxx;
  uint4 res = QuadReadLaneAt(arg_0, 1);
  return res;
}

void fragment_main() {
  prevent_dce.Store4(0u, quadBroadcast_b68331());
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store4(0u, quadBroadcast_b68331());
}

