RWByteAddressBuffer prevent_dce : register(u0);

int2 subgroupAnd_4adc72() {
  int2 tint_tmp = (1).xx;
  int2 res = asint(WaveActiveBitAnd(asuint(tint_tmp)));
  return res;
}

void fragment_main() {
  prevent_dce.Store2(0u, asuint(subgroupAnd_4adc72()));
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store2(0u, asuint(subgroupAnd_4adc72()));
  return;
}
