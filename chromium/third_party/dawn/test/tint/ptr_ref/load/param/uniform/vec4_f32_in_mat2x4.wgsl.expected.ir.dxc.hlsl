
cbuffer cbuffer_S : register(b0) {
  uint4 S[2];
};
float4 func(uint pointer_indices[1]) {
  return asfloat(S[((16u * uint(pointer_indices[0u])) / 16u)]);
}

[numthreads(1, 1, 1)]
void main() {
  uint v[1] = (uint[1])0;
  float4 r = func(v);
}

