
RWByteAddressBuffer prevent_dce : register(u0);
float3 subgroupAdd_7d1215() {
  float3 arg_0 = (1.0f).xxx;
  float3 res = WaveActiveSum(arg_0);
  return res;
}

void fragment_main() {
  prevent_dce.Store3(0u, asuint(subgroupAdd_7d1215()));
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store3(0u, asuint(subgroupAdd_7d1215()));
}

