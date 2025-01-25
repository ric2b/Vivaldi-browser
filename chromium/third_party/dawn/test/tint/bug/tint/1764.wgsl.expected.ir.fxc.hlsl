struct main_inputs {
  uint tint_local_index : SV_GroupIndex;
};


groupshared int W[246];
void main_inner(uint tint_local_index) {
  {
    uint v = 0u;
    v = tint_local_index;
    while(true) {
      uint v_1 = v;
      if ((v_1 >= 246u)) {
        break;
      }
      W[v_1] = 0;
      {
        v = (v_1 + 1u);
      }
      continue;
    }
  }
  GroupMemoryBarrierWithGroupSync();
  int p[246] = W;
  p[0] = 42;
}

[numthreads(1, 1, 1)]
void main(main_inputs inputs) {
  main_inner(inputs.tint_local_index);
}

