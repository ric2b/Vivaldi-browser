
RWByteAddressBuffer prevent_dce : register(u0);
TextureCubeArray<float4> arg_0 : register(t0, space1);
SamplerState arg_1 : register(s1, space1);
float4 textureSampleBias_c6953d() {
  TextureCubeArray<float4> v = arg_0;
  SamplerState v_1 = arg_1;
  float4 res = v.SampleBias(v_1, float4((1.0f).xxx, float(1u)), 1.0f);
  return res;
}

void fragment_main() {
  prevent_dce.Store4(0u, asuint(textureSampleBias_c6953d()));
}

