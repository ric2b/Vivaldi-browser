RWTexture1D<float4> arg_0 : register(u0, space1);

void textureStore_178e69() {
  arg_0[1] = (1.0f).xxxx;
}

void fragment_main() {
  textureStore_178e69();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  textureStore_178e69();
  return;
}
