struct S {
  int before;
  float4x4 m;
  int after;
};


cbuffer cbuffer_u : register(b0) {
  uint4 u[48];
};
void a(S a_1[4]) {
}

void b(S s) {
}

void c(float4x4 m) {
}

void d(float4 v) {
}

void e(float f) {
}

float4x4 v_1(uint start_byte_offset) {
  float4 v_2 = asfloat(u[(start_byte_offset / 16u)]);
  float4 v_3 = asfloat(u[((16u + start_byte_offset) / 16u)]);
  float4 v_4 = asfloat(u[((32u + start_byte_offset) / 16u)]);
  return float4x4(v_2, v_3, v_4, asfloat(u[((48u + start_byte_offset) / 16u)]));
}

S v_5(uint start_byte_offset) {
  int v_6 = asint(u[(start_byte_offset / 16u)][((start_byte_offset % 16u) / 4u)]);
  float4x4 v_7 = v_1((16u + start_byte_offset));
  S v_8 = {v_6, v_7, asint(u[((128u + start_byte_offset) / 16u)][(((128u + start_byte_offset) % 16u) / 4u)])};
  return v_8;
}

typedef S ary_ret[4];
ary_ret v_9(uint start_byte_offset) {
  S a[4] = (S[4])0;
  {
    uint v_10 = 0u;
    v_10 = 0u;
    while(true) {
      uint v_11 = v_10;
      if ((v_11 >= 4u)) {
        break;
      }
      S v_12 = v_5((start_byte_offset + (v_11 * 192u)));
      a[v_11] = v_12;
      {
        v_10 = (v_11 + 1u);
      }
      continue;
    }
  }
  S v_13[4] = a;
  return v_13;
}

[numthreads(1, 1, 1)]
void f() {
  S v_14[4] = v_9(0u);
  a(v_14);
  S v_15 = v_5(384u);
  b(v_15);
  c(v_1(400u));
  d(asfloat(u[2u]).ywxz);
  e(asfloat(u[2u]).ywxz[0u]);
}

