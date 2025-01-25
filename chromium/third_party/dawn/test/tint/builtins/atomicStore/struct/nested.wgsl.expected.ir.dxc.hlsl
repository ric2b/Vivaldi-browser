struct S0 {
  int x;
  uint a;
  int y;
  int z;
};

struct S1 {
  int x;
  S0 a;
  int y;
  int z;
};

struct S2 {
  int x;
  int y;
  int z;
  S1 a;
};

struct compute_main_inputs {
  uint tint_local_index : SV_GroupIndex;
};


groupshared S2 wg;
void compute_main_inner(uint tint_local_index) {
  if ((tint_local_index == 0u)) {
    wg.x = 0;
    wg.y = 0;
    wg.z = 0;
    wg.a.x = 0;
    wg.a.a.x = 0;
    uint v = 0u;
    InterlockedExchange(wg.a.a.a, 0u, v);
    wg.a.a.y = 0;
    wg.a.a.z = 0;
    wg.a.y = 0;
    wg.a.z = 0;
  }
  GroupMemoryBarrierWithGroupSync();
  uint v_1 = 0u;
  InterlockedExchange(wg.a.a.a, 1u, v_1);
}

[numthreads(1, 1, 1)]
void compute_main(compute_main_inputs inputs) {
  compute_main_inner(inputs.tint_local_index);
}

