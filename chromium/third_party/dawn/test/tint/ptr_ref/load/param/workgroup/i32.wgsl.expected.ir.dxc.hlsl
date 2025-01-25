struct main_inputs {
  uint tint_local_index : SV_GroupIndex;
};


groupshared int S;
int func() {
  return S;
}

void main_inner(uint tint_local_index) {
  if ((tint_local_index == 0u)) {
    S = 0;
  }
  GroupMemoryBarrierWithGroupSync();
  int r = func();
}

[numthreads(1, 1, 1)]
void main(main_inputs inputs) {
  main_inner(inputs.tint_local_index);
}

