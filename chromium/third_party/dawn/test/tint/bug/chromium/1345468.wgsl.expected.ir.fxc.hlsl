
void f() {
  int i = 1;
  float2 a = float4x2((0.0f).xx, (0.0f).xx, float2(4.0f, 0.0f), (0.0f).xx)[i];
  int b = int2(0, 1)[i];
}

[numthreads(1, 1, 1)]
void unused_entry_point() {
}

