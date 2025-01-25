
void deref_const() {
  int a[10] = (int[10])0;
  int p[10] = a;
  int b = p[0];
  p[0] = 42;
}

void no_deref_const() {
  int a[10] = (int[10])0;
  int p[10] = a;
  int b = p[0];
  p[0] = 42;
}

void deref_let() {
  int a[10] = (int[10])0;
  int p[10] = a;
  int i = 0;
  int b = p[i];
  p[0] = 42;
}

void no_deref_let() {
  int a[10] = (int[10])0;
  int p[10] = a;
  int i = 0;
  int b = p[i];
  p[0] = 42;
}

void deref_var() {
  int a[10] = (int[10])0;
  int p[10] = a;
  int i = 0;
  int b = p[i];
  p[0] = 42;
}

void no_deref_var() {
  int a[10] = (int[10])0;
  int p[10] = a;
  int i = 0;
  int b = p[i];
  p[0] = 42;
}

[numthreads(1, 1, 1)]
void main() {
  deref_const();
  no_deref_const();
  deref_let();
  no_deref_let();
  deref_var();
  no_deref_var();
}

