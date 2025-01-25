<dawn>/test/tint/diagnostic_filtering/for_loop_body_attribute.wgsl:8:9 warning: 'textureSample' must only be called from uniform control flow
    v = textureSample(t, s, vec2(0, 0));
        ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

<dawn>/test/tint/diagnostic_filtering/for_loop_body_attribute.wgsl:7:3 note: control flow depends on possibly non-uniform value
  for (; x > v.x; ) @diagnostic(warning, derivative_uniformity) {
  ^^^

<dawn>/test/tint/diagnostic_filtering/for_loop_body_attribute.wgsl:8:9 note: return value of 'textureSample' may be non-uniform
    v = textureSample(t, s, vec2(0, 0));
        ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

struct main_inputs {
  float x : TEXCOORD0;
};


Texture2D<float4> t : register(t1);
SamplerState s : register(s2);
void main_inner(float x) {
  float4 v = (0.0f).xxxx;
  {
    while(true) {
      if ((x > v.x)) {
      } else {
        break;
      }
      v = t.Sample(s, (0.0f).xx);
      {
      }
      continue;
    }
  }
}

void main(main_inputs inputs) {
  main_inner(inputs.x);
}

