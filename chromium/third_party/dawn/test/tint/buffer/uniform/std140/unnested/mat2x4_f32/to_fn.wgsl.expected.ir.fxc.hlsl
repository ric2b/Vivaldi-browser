
cbuffer cbuffer_u : register(b0) {
  uint4 u[2];
};
void a(float2x4 m) {
}

void b(float4 v) {
}

void c(float f) {
}

float2x4 v_1(uint start_byte_offset) {
  float4 v_2 = asfloat(u[(start_byte_offset / 16u)]);
  return float2x4(v_2, asfloat(u[((16u + start_byte_offset) / 16u)]));
}

[numthreads(1, 1, 1)]
void f() {
  a(v_1(0u));
  b(asfloat(u[1u]));
  b(asfloat(u[1u]).ywxz);
  c(asfloat(u[1u].x));
  c(asfloat(u[1u]).ywxz[0u]);
}

