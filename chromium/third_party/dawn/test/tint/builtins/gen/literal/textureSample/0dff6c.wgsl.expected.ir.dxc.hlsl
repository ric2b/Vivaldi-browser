SKIP: FAILED


RWByteAddressBuffer prevent_dce : register(u0);
Texture2D arg_0 : register(t0, space1);
SamplerState arg_1 : register(s1, space1);
float textureSample_0dff6c() {
  float res = arg_0.Sample(arg_1, (1.0f).xx, (1).xx);
  return res;
}

void fragment_main() {
  prevent_dce.Store(0u, asuint(textureSample_0dff6c()));
}


tint executable returned error: exit status 0xe0000001
