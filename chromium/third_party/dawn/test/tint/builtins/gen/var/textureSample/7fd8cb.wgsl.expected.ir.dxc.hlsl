
RWByteAddressBuffer prevent_dce : register(u0);
TextureCubeArray arg_0 : register(t0, space1);
SamplerState arg_1 : register(s1, space1);
float textureSample_7fd8cb() {
  float3 arg_2 = (1.0f).xxx;
  uint arg_3 = 1u;
  TextureCubeArray v = arg_0;
  SamplerState v_1 = arg_1;
  float3 v_2 = arg_2;
  float res = v.Sample(v_1, float4(v_2, float(arg_3)));
  return res;
}

void fragment_main() {
  prevent_dce.Store(0u, asuint(textureSample_7fd8cb()));
}

