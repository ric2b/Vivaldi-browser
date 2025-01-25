struct Inner {
  matrix<float16_t, 3, 4> m;
};

struct Outer {
  Inner a[4];
};


cbuffer cbuffer_a : register(b0) {
  uint4 a[64];
};
static int counter = 0;
int i() {
  counter = (counter + 1);
  return counter;
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

matrix<float16_t, 3, 4> v_4(uint start_byte_offset) {
  vector<float16_t, 4> v_5 = tint_bitcast_to_f16(a[(start_byte_offset / 16u)]);
  vector<float16_t, 4> v_6 = tint_bitcast_to_f16(a[((8u + start_byte_offset) / 16u)]);
  return matrix<float16_t, 3, 4>(v_5, v_6, tint_bitcast_to_f16(a[((16u + start_byte_offset) / 16u)]));
}

Inner v_7(uint start_byte_offset) {
  Inner v_8 = {v_4(start_byte_offset)};
  return v_8;
}

typedef Inner ary_ret[4];
ary_ret v_9(uint start_byte_offset) {
  Inner a[4] = (Inner[4])0;
  {
    uint v_10 = 0u;
    v_10 = 0u;
    while(true) {
      uint v_11 = v_10;
      if ((v_11 >= 4u)) {
        break;
      }
      Inner v_12 = v_7((start_byte_offset + (v_11 * 64u)));
      a[v_11] = v_12;
      {
        v_10 = (v_11 + 1u);
      }
      continue;
    }
  }
  Inner v_13[4] = a;
  return v_13;
}

Outer v_14(uint start_byte_offset) {
  Inner v_15[4] = v_9(start_byte_offset);
  Outer v_16 = {v_15};
  return v_16;
}

typedef Outer ary_ret_1[4];
ary_ret_1 v_17(uint start_byte_offset) {
  Outer a[4] = (Outer[4])0;
  {
    uint v_18 = 0u;
    v_18 = 0u;
    while(true) {
      uint v_19 = v_18;
      if ((v_19 >= 4u)) {
        break;
      }
      Outer v_20 = v_14((start_byte_offset + (v_19 * 256u)));
      a[v_19] = v_20;
      {
        v_18 = (v_19 + 1u);
      }
      continue;
    }
  }
  Outer v_21[4] = a;
  return v_21;
}

[numthreads(1, 1, 1)]
void f() {
  uint v_22 = (256u * uint(i()));
  uint v_23 = (64u * uint(i()));
  uint v_24 = (8u * uint(i()));
  Outer l_a[4] = v_17(0u);
  Outer l_a_i = v_14(v_22);
  Inner l_a_i_a[4] = v_9(v_22);
  Inner l_a_i_a_i = v_7((v_22 + v_23));
  matrix<float16_t, 3, 4> l_a_i_a_i_m = v_4((v_22 + v_23));
  vector<float16_t, 4> l_a_i_a_i_m_i = tint_bitcast_to_f16(a[(((v_22 + v_23) + v_24) / 16u)]);
  uint v_25 = (((v_22 + v_23) + v_24) + (uint(i()) * 2u));
  uint v_26 = a[(v_25 / 16u)][((v_25 % 16u) / 4u)];
  float16_t l_a_i_a_i_m_i_i = float16_t(f16tof32((v_26 >> ((((v_25 % 4u) == 0u)) ? (0u) : (16u)))));
}

