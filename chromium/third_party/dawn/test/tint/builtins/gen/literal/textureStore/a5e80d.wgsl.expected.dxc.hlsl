RWTexture3D<float4> arg_0 : register(u0, space1);

void textureStore_a5e80d() {
  arg_0[(1u).xxx] = (1.0f).xxxx;
}

void fragment_main() {
  textureStore_a5e80d();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  textureStore_a5e80d();
  return;
}
