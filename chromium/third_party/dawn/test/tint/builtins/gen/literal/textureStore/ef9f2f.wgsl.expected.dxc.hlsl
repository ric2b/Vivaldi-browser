RWTexture3D<uint4> arg_0 : register(u0, space1);

void textureStore_ef9f2f() {
  arg_0[(1).xxx] = (1u).xxxx;
}

void fragment_main() {
  textureStore_ef9f2f();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  textureStore_ef9f2f();
  return;
}
