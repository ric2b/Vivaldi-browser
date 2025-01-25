RWByteAddressBuffer prevent_dce : register(u0);

RWByteAddressBuffer sb_rw : register(u1);

uint sb_rwatomicExchange(uint offset, uint value) {
  uint original_value = 0;
  sb_rw.InterlockedExchange(offset, value, original_value);
  return original_value;
}


uint atomicExchange_d59712() {
  uint arg_1 = 1u;
  uint res = sb_rwatomicExchange(0u, arg_1);
  return res;
}

void fragment_main() {
  prevent_dce.Store(0u, asuint(atomicExchange_d59712()));
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store(0u, asuint(atomicExchange_d59712()));
  return;
}
