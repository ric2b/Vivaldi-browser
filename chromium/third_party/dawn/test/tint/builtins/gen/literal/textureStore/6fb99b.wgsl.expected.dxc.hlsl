RWTexture3D<float4> arg_0 : register(u0, space1);

void textureStore_6fb99b() {
  arg_0[(1).xxx] = (1.0f).xxxx;
}

void fragment_main() {
  textureStore_6fb99b();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  textureStore_6fb99b();
  return;
}
