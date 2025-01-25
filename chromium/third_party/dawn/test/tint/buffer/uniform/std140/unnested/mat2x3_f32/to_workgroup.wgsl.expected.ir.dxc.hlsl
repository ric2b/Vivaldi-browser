struct f_inputs {
  uint tint_local_index : SV_GroupIndex;
};


cbuffer cbuffer_u : register(b0) {
  uint4 u[2];
};
groupshared float2x3 w;
float2x3 v(uint start_byte_offset) {
  float3 v_1 = asfloat(u[(start_byte_offset / 16u)].xyz);
  return float2x3(v_1, asfloat(u[((16u + start_byte_offset) / 16u)].xyz));
}

void f_inner(uint tint_local_index) {
  if ((tint_local_index == 0u)) {
    w = float2x3((0.0f).xxx, (0.0f).xxx);
  }
  GroupMemoryBarrierWithGroupSync();
  w = v(0u);
  w[1] = asfloat(u[0u].xyz);
  w[1] = asfloat(u[0u].xyz).zxy;
  w[0][1] = asfloat(u[1u].x);
}

[numthreads(1, 1, 1)]
void f(f_inputs inputs) {
  f_inner(inputs.tint_local_index);
}

