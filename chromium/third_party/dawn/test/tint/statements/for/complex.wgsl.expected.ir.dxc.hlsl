
void some_loop_body() {
}

void f() {
  int j = 0;
  {
    int i = 0;
    while(true) {
      bool v = false;
      if ((i < 5)) {
        v = (j < 10);
      } else {
        v = false;
      }
      if (v) {
      } else {
        break;
      }
      some_loop_body();
      j = (i * 30);
      {
        i = (i + 1);
      }
      continue;
    }
  }
}

[numthreads(1, 1, 1)]
void unused_entry_point() {
}

