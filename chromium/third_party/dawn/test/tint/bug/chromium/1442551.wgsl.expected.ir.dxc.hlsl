
void f() {
  int i = 1;
  int b = int2(1, 2)[i];
}

[numthreads(1, 1, 1)]
void unused_entry_point() {
}

