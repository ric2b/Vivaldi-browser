struct S {
  int before;
  matrix<float16_t, 4, 3> m;
  int after;
};

struct f_inputs {
  uint tint_local_index : SV_GroupIndex;
};


cbuffer cbuffer_u : register(b0) {
  uint4 u[32];
};
groupshared S w[4];
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

matrix<float16_t, 4, 3> v_4(uint start_byte_offset) {
  vector<float16_t, 3> v_5 = tint_bitcast_to_f16(u[(start_byte_offset / 16u)]).xyz;
  vector<float16_t, 3> v_6 = tint_bitcast_to_f16(u[((8u + start_byte_offset) / 16u)]).xyz;
  vector<float16_t, 3> v_7 = tint_bitcast_to_f16(u[((16u + start_byte_offset) / 16u)]).xyz;
  return matrix<float16_t, 4, 3>(v_5, v_6, v_7, tint_bitcast_to_f16(u[((24u + start_byte_offset) / 16u)]).xyz);
}

S v_8(uint start_byte_offset) {
  int v_9 = asint(u[(start_byte_offset / 16u)][((start_byte_offset % 16u) / 4u)]);
  matrix<float16_t, 4, 3> v_10 = v_4((8u + start_byte_offset));
  S v_11 = {v_9, v_10, asint(u[((64u + start_byte_offset) / 16u)][(((64u + start_byte_offset) % 16u) / 4u)])};
  return v_11;
}

typedef S ary_ret[4];
ary_ret v_12(uint start_byte_offset) {
  S a[4] = (S[4])0;
  {
    uint v_13 = 0u;
    v_13 = 0u;
    while(true) {
      uint v_14 = v_13;
      if ((v_14 >= 4u)) {
        break;
      }
      S v_15 = v_8((start_byte_offset + (v_14 * 128u)));
      a[v_14] = v_15;
      {
        v_13 = (v_14 + 1u);
      }
      continue;
    }
  }
  S v_16[4] = a;
  return v_16;
}

void f_inner(uint tint_local_index) {
  {
    uint v_17 = 0u;
    v_17 = tint_local_index;
    while(true) {
      uint v_18 = v_17;
      if ((v_18 >= 4u)) {
        break;
      }
      S v_19 = (S)0;
      w[v_18] = v_19;
      {
        v_17 = (v_18 + 1u);
      }
      continue;
    }
  }
  GroupMemoryBarrierWithGroupSync();
  S v_20[4] = v_12(0u);
  w = v_20;
  S v_21 = v_8(256u);
  w[1] = v_21;
  w[3].m = v_4(264u);
  w[1].m[0] = tint_bitcast_to_f16(u[1u]).xyz.zxy;
}

[numthreads(1, 1, 1)]
void f(f_inputs inputs) {
  f_inner(inputs.tint_local_index);
}

