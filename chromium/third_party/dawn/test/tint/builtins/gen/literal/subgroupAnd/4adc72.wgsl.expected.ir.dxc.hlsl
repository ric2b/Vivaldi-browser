
RWByteAddressBuffer prevent_dce : register(u0);
int2 subgroupAnd_4adc72() {
  int2 arg = (1).xx;
  int2 res = asint(WaveActiveBitAnd(asuint(arg)));
  return res;
}

void fragment_main() {
  prevent_dce.Store2(0u, asuint(subgroupAnd_4adc72()));
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store2(0u, asuint(subgroupAnd_4adc72()));
}

