
void some_loop_body() {
}

void f() {
  {
    int i = 0;
    while(true) {
      if ((i < 5)) {
      } else {
        break;
      }
      some_loop_body();
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

