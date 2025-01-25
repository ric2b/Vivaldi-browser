struct main1_inputs {
  uint tint_local_index : SV_GroupIndex;
};

struct main2_inputs {
  uint tint_local_index : SV_GroupIndex;
};

struct main3_inputs {
  uint tint_local_index : SV_GroupIndex;
};


groupshared int a;
groupshared int b;
groupshared int c;
void uses_a() {
  a = (a + 1);
}

void uses_b() {
  b = (b * 2);
}

void uses_a_and_b() {
  b = a;
}

void no_uses() {
}

void outer() {
  a = 0;
  uses_a();
  uses_a_and_b();
  uses_b();
  no_uses();
}

void main1_inner(uint tint_local_index) {
  if ((tint_local_index == 0u)) {
    a = 0;
  }
  GroupMemoryBarrierWithGroupSync();
  a = 42;
  uses_a();
}

void main2_inner(uint tint_local_index) {
  if ((tint_local_index == 0u)) {
    b = 0;
  }
  GroupMemoryBarrierWithGroupSync();
  b = 7;
  uses_b();
}

void main3_inner(uint tint_local_index) {
  if ((tint_local_index == 0u)) {
    a = 0;
    b = 0;
  }
  GroupMemoryBarrierWithGroupSync();
  outer();
  no_uses();
}

[numthreads(1, 1, 1)]
void main4() {
  no_uses();
}

[numthreads(1, 1, 1)]
void main1(main1_inputs inputs) {
  main1_inner(inputs.tint_local_index);
}

[numthreads(1, 1, 1)]
void main2(main2_inputs inputs) {
  main2_inner(inputs.tint_local_index);
}

[numthreads(1, 1, 1)]
void main3(main3_inputs inputs) {
  main3_inner(inputs.tint_local_index);
}

