RWTexture3D<uint4> arg_0 : register(u0, space1);

void textureStore_53a68b() {
  arg_0[(1).xxx] = (1u).xxxx;
}

void fragment_main() {
  textureStore_53a68b();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  textureStore_53a68b();
  return;
}
