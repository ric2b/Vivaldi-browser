
RWByteAddressBuffer sb_rw : register(u0);
void atomicOr_5e95d4() {
  uint arg_1 = 0u;
  uint res = 0u;
  arg_1 = 1u;
  uint x_18 = arg_1;
  uint v = 0u;
  sb_rw.InterlockedOr(uint(0u), x_18, v);
  uint x_13 = v;
  res = x_13;
}

void fragment_main_1() {
  atomicOr_5e95d4();
}

void fragment_main() {
  fragment_main_1();
}

void compute_main_1() {
  atomicOr_5e95d4();
}

[numthreads(1, 1, 1)]
void compute_main() {
  compute_main_1();
}

