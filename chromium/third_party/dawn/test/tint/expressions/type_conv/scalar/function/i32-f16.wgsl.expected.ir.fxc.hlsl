SKIP: INVALID


static int t = 0;
int m() {
  t = 1;
  return int(t);
}

void f() {
  float16_t v = float16_t(m());
}

[numthreads(1, 1, 1)]
void unused_entry_point() {
}

FXC validation failure:
<scrubbed_path>(9,3-11): error X3000: unrecognized identifier 'float16_t'
<scrubbed_path>(9,13): error X3000: unrecognized identifier 'v'


tint executable returned error: exit status 1
