
RWByteAddressBuffer sb_rw : register(u0);
void atomicStore_d1e9a6() {
  int v = 0;
  sb_rw.InterlockedExchange(int(0u), 1, v);
}

void fragment_main_1() {
  atomicStore_d1e9a6();
}

void fragment_main() {
  fragment_main_1();
}

void compute_main_1() {
  atomicStore_d1e9a6();
}

[numthreads(1, 1, 1)]
void compute_main() {
  compute_main_1();
}

