struct main_inputs {
  uint idx : SV_GroupIndex;
};


ByteAddressBuffer sb : register(t0);
RWByteAddressBuffer s : register(u1);
int tint_f32_to_i32(float value) {
  return (((value <= 2147483520.0f)) ? ((((value >= -2147483648.0f)) ? (int(value)) : (-2147483648))) : (2147483647));
}

typedef float3 ary_ret[2];
ary_ret v(uint offset) {
  float3 a[2] = (float3[2])0;
  {
    uint v_1 = 0u;
    v_1 = 0u;
    while(true) {
      uint v_2 = v_1;
      if ((v_2 >= 2u)) {
        break;
      }
      a[v_2] = asfloat(sb.Load3((offset + (v_2 * 16u))));
      {
        v_1 = (v_2 + 1u);
      }
      continue;
    }
  }
  float3 v_3[2] = a;
  return v_3;
}

float4x4 v_4(uint offset) {
  float4 v_5 = asfloat(sb.Load4((offset + 0u)));
  float4 v_6 = asfloat(sb.Load4((offset + 16u)));
  float4 v_7 = asfloat(sb.Load4((offset + 32u)));
  return float4x4(v_5, v_6, v_7, asfloat(sb.Load4((offset + 48u))));
}

float4x3 v_8(uint offset) {
  float3 v_9 = asfloat(sb.Load3((offset + 0u)));
  float3 v_10 = asfloat(sb.Load3((offset + 16u)));
  float3 v_11 = asfloat(sb.Load3((offset + 32u)));
  return float4x3(v_9, v_10, v_11, asfloat(sb.Load3((offset + 48u))));
}

float4x2 v_12(uint offset) {
  float2 v_13 = asfloat(sb.Load2((offset + 0u)));
  float2 v_14 = asfloat(sb.Load2((offset + 8u)));
  float2 v_15 = asfloat(sb.Load2((offset + 16u)));
  return float4x2(v_13, v_14, v_15, asfloat(sb.Load2((offset + 24u))));
}

float3x4 v_16(uint offset) {
  float4 v_17 = asfloat(sb.Load4((offset + 0u)));
  float4 v_18 = asfloat(sb.Load4((offset + 16u)));
  return float3x4(v_17, v_18, asfloat(sb.Load4((offset + 32u))));
}

float3x3 v_19(uint offset) {
  float3 v_20 = asfloat(sb.Load3((offset + 0u)));
  float3 v_21 = asfloat(sb.Load3((offset + 16u)));
  return float3x3(v_20, v_21, asfloat(sb.Load3((offset + 32u))));
}

float3x2 v_22(uint offset) {
  float2 v_23 = asfloat(sb.Load2((offset + 0u)));
  float2 v_24 = asfloat(sb.Load2((offset + 8u)));
  return float3x2(v_23, v_24, asfloat(sb.Load2((offset + 16u))));
}

float2x4 v_25(uint offset) {
  float4 v_26 = asfloat(sb.Load4((offset + 0u)));
  return float2x4(v_26, asfloat(sb.Load4((offset + 16u))));
}

float2x3 v_27(uint offset) {
  float3 v_28 = asfloat(sb.Load3((offset + 0u)));
  return float2x3(v_28, asfloat(sb.Load3((offset + 16u))));
}

float2x2 v_29(uint offset) {
  float2 v_30 = asfloat(sb.Load2((offset + 0u)));
  return float2x2(v_30, asfloat(sb.Load2((offset + 8u))));
}

void main_inner(uint idx) {
  float scalar_f32 = asfloat(sb.Load((0u + (uint(idx) * 544u))));
  int scalar_i32 = asint(sb.Load((4u + (uint(idx) * 544u))));
  uint scalar_u32 = sb.Load((8u + (uint(idx) * 544u)));
  float2 vec2_f32 = asfloat(sb.Load2((16u + (uint(idx) * 544u))));
  int2 vec2_i32 = asint(sb.Load2((24u + (uint(idx) * 544u))));
  uint2 vec2_u32 = sb.Load2((32u + (uint(idx) * 544u)));
  float3 vec3_f32 = asfloat(sb.Load3((48u + (uint(idx) * 544u))));
  int3 vec3_i32 = asint(sb.Load3((64u + (uint(idx) * 544u))));
  uint3 vec3_u32 = sb.Load3((80u + (uint(idx) * 544u)));
  float4 vec4_f32 = asfloat(sb.Load4((96u + (uint(idx) * 544u))));
  int4 vec4_i32 = asint(sb.Load4((112u + (uint(idx) * 544u))));
  uint4 vec4_u32 = sb.Load4((128u + (uint(idx) * 544u)));
  float2x2 mat2x2_f32 = v_29((144u + (uint(idx) * 544u)));
  float2x3 mat2x3_f32 = v_27((160u + (uint(idx) * 544u)));
  float2x4 mat2x4_f32 = v_25((192u + (uint(idx) * 544u)));
  float3x2 mat3x2_f32 = v_22((224u + (uint(idx) * 544u)));
  float3x3 mat3x3_f32 = v_19((256u + (uint(idx) * 544u)));
  float3x4 mat3x4_f32 = v_16((304u + (uint(idx) * 544u)));
  float4x2 mat4x2_f32 = v_12((352u + (uint(idx) * 544u)));
  float4x3 mat4x3_f32 = v_8((384u + (uint(idx) * 544u)));
  float4x4 mat4x4_f32 = v_4((448u + (uint(idx) * 544u)));
  float3 v_31[2] = v((512u + (uint(idx) * 544u)));
  int v_32 = (tint_f32_to_i32(scalar_f32) + scalar_i32);
  int v_33 = (v_32 + int(scalar_u32));
  int v_34 = ((v_33 + tint_f32_to_i32(vec2_f32[0u])) + vec2_i32[0u]);
  int v_35 = (v_34 + int(vec2_u32[0u]));
  int v_36 = ((v_35 + tint_f32_to_i32(vec3_f32[1u])) + vec3_i32[1u]);
  int v_37 = (v_36 + int(vec3_u32[1u]));
  int v_38 = ((v_37 + tint_f32_to_i32(vec4_f32[2u])) + vec4_i32[2u]);
  int v_39 = (v_38 + int(vec4_u32[2u]));
  int v_40 = (v_39 + tint_f32_to_i32(mat2x2_f32[0][0u]));
  int v_41 = (v_40 + tint_f32_to_i32(mat2x3_f32[0][0u]));
  int v_42 = (v_41 + tint_f32_to_i32(mat2x4_f32[0][0u]));
  int v_43 = (v_42 + tint_f32_to_i32(mat3x2_f32[0][0u]));
  int v_44 = (v_43 + tint_f32_to_i32(mat3x3_f32[0][0u]));
  int v_45 = (v_44 + tint_f32_to_i32(mat3x4_f32[0][0u]));
  int v_46 = (v_45 + tint_f32_to_i32(mat4x2_f32[0][0u]));
  int v_47 = (v_46 + tint_f32_to_i32(mat4x3_f32[0][0u]));
  int v_48 = (v_47 + tint_f32_to_i32(mat4x4_f32[0][0u]));
  float3 arr2_vec3_f32[2] = v_31;
  s.Store(0u, asuint((v_48 + tint_f32_to_i32(arr2_vec3_f32[0][0u]))));
}

[numthreads(1, 1, 1)]
void main(main_inputs inputs) {
  main_inner(inputs.idx);
}

