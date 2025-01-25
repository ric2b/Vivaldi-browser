struct S {
  int before;
  float2x3 m;
  int after;
};


cbuffer cbuffer_u : register(b0) {
  uint4 u[32];
};
void a(S a_1[4]) {
}

void b(S s) {
}

void c(float2x3 m) {
}

void d(float3 v) {
}

void e(float f) {
}

float2x3 v_1(uint start_byte_offset) {
  float3 v_2 = asfloat(u[(start_byte_offset / 16u)].xyz);
  return float2x3(v_2, asfloat(u[((16u + start_byte_offset) / 16u)].xyz));
}

S v_3(uint start_byte_offset) {
  int v_4 = asint(u[(start_byte_offset / 16u)][((start_byte_offset % 16u) / 4u)]);
  float2x3 v_5 = v_1((16u + start_byte_offset));
  S v_6 = {v_4, v_5, asint(u[((64u + start_byte_offset) / 16u)][(((64u + start_byte_offset) % 16u) / 4u)])};
  return v_6;
}

typedef S ary_ret[4];
ary_ret v_7(uint start_byte_offset) {
  S a[4] = (S[4])0;
  {
    uint v_8 = 0u;
    v_8 = 0u;
    while(true) {
      uint v_9 = v_8;
      if ((v_9 >= 4u)) {
        break;
      }
      S v_10 = v_3((start_byte_offset + (v_9 * 128u)));
      a[v_9] = v_10;
      {
        v_8 = (v_9 + 1u);
      }
      continue;
    }
  }
  S v_11[4] = a;
  return v_11;
}

[numthreads(1, 1, 1)]
void f() {
  S v_12[4] = v_7(0u);
  a(v_12);
  S v_13 = v_3(256u);
  b(v_13);
  c(v_1(272u));
  d(asfloat(u[2u].xyz).zxy);
  e(asfloat(u[2u].xyz).zxy[0u]);
}

