
RWByteAddressBuffer sb_rw : register(u0);
void atomicOr_8d96a0() {
  int res = 0;
  int v = 0;
  sb_rw.InterlockedOr(int(0u), 1, v);
  int x_9 = v;
  res = x_9;
}

void fragment_main_1() {
  atomicOr_8d96a0();
}

void fragment_main() {
  fragment_main_1();
}

void compute_main_1() {
  atomicOr_8d96a0();
}

[numthreads(1, 1, 1)]
void compute_main() {
  compute_main_1();
}

