RWTexture2DArray<float4> arg_0 : register(u0, space1);

void textureStore_dfa9a1() {
  arg_0[uint3((1u).xx, uint(1))] = (1.0f).xxxx;
}

void fragment_main() {
  textureStore_dfa9a1();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  textureStore_dfa9a1();
  return;
}
