
RWTexture2D<int4> arg_0 : register(u0, space1);
void textureStore_752da6() {
  arg_0[(1).xx] = (1).xxxx;
}

void fragment_main() {
  textureStore_752da6();
}

[numthreads(1, 1, 1)]
void compute_main() {
  textureStore_752da6();
}

