
cbuffer cbuffer_u : register(b0) {
  uint4 u[2];
};
void a(float2x3 m) {
}

void b(float3 v) {
}

void c(float f) {
}

float2x3 v_1(uint start_byte_offset) {
  float3 v_2 = asfloat(u[(start_byte_offset / 16u)].xyz);
  return float2x3(v_2, asfloat(u[((16u + start_byte_offset) / 16u)].xyz));
}

[numthreads(1, 1, 1)]
void f() {
  a(v_1(0u));
  b(asfloat(u[1u].xyz));
  b(asfloat(u[1u].xyz).zxy);
  c(asfloat(u[1u].x));
  c(asfloat(u[1u].xyz).zxy[0u]);
}

