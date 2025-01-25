struct S {
  int before;
  matrix<float16_t, 2, 3> m;
  int after;
};


cbuffer cbuffer_u : register(b0) {
  uint4 u[32];
};
void a(S a_1[4]) {
}

void b(S s) {
}

void c(matrix<float16_t, 2, 3> m) {
}

void d(vector<float16_t, 3> v) {
}

void e(float16_t f) {
}

vector<float16_t, 4> tint_bitcast_to_f16(uint4 src) {
  uint4 v = src;
  uint4 mask = (65535u).xxxx;
  uint4 shift = (16u).xxxx;
  float4 t_low = f16tof32((v & mask));
  float4 t_high = f16tof32(((v >> shift) & mask));
  float16_t v_1 = float16_t(t_low.x);
  float16_t v_2 = float16_t(t_high.x);
  float16_t v_3 = float16_t(t_low.y);
  return vector<float16_t, 4>(v_1, v_2, v_3, float16_t(t_high.y));
}

matrix<float16_t, 2, 3> v_4(uint start_byte_offset) {
  vector<float16_t, 3> v_5 = tint_bitcast_to_f16(u[(start_byte_offset / 16u)]).xyz;
  return matrix<float16_t, 2, 3>(v_5, tint_bitcast_to_f16(u[((8u + start_byte_offset) / 16u)]).xyz);
}

S v_6(uint start_byte_offset) {
  int v_7 = asint(u[(start_byte_offset / 16u)][((start_byte_offset % 16u) / 4u)]);
  matrix<float16_t, 2, 3> v_8 = v_4((8u + start_byte_offset));
  S v_9 = {v_7, v_8, asint(u[((64u + start_byte_offset) / 16u)][(((64u + start_byte_offset) % 16u) / 4u)])};
  return v_9;
}

typedef S ary_ret[4];
ary_ret v_10(uint start_byte_offset) {
  S a[4] = (S[4])0;
  {
    uint v_11 = 0u;
    v_11 = 0u;
    while(true) {
      uint v_12 = v_11;
      if ((v_12 >= 4u)) {
        break;
      }
      S v_13 = v_6((start_byte_offset + (v_12 * 128u)));
      a[v_12] = v_13;
      {
        v_11 = (v_12 + 1u);
      }
      continue;
    }
  }
  S v_14[4] = a;
  return v_14;
}

[numthreads(1, 1, 1)]
void f() {
  S v_15[4] = v_10(0u);
  a(v_15);
  S v_16 = v_6(256u);
  b(v_16);
  c(v_4(264u));
  d(tint_bitcast_to_f16(u[1u]).xyz.zxy);
  e(tint_bitcast_to_f16(u[1u]).xyz.zxy[0u]);
}

