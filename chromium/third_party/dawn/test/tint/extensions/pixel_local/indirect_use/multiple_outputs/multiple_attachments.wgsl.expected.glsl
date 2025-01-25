SKIP: INVALID


enable chromium_experimental_pixel_local;

struct PixelLocal {
  a : u32,
  b : i32,
  c : f32,
}

var<pixel_local> P : PixelLocal;

struct Out {
  @location(0)
  x : vec4f,
  @location(2)
  y : vec4f,
  @location(4)
  z : vec4f,
}

fn f0() {
  P.a += 9;
}

fn f1() {
  f0();
  P.a += 8;
}

fn f2() {
  P.a += 7;
  f1();
}

@fragment
fn f() -> Out {
  f2();
  return Out(vec4f(10), vec4f(20), vec4f(30));
}

Failed to generate: <dawn>/test/tint/extensions/pixel_local/indirect_use/multiple_outputs/multiple_attachments.wgsl:2:8 error: GLSL backend does not support extension 'chromium_experimental_pixel_local'
enable chromium_experimental_pixel_local;
       ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

