
ByteAddressBuffer tint_symbol : register(t0);
RWByteAddressBuffer tint_symbol_1 : register(u1);
[numthreads(1, 1, 1)]
void main() {
  tint_symbol_1.Store2(0u, asuint(asfloat(tint_symbol.Load2(0u))));
}

