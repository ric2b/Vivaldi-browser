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

@fragment
fn f() -> Out {
  P.a += 42;
  return Out(vec4f(10), vec4f(20), vec4f(30));
}

Failed to generate: <dawn>/test/tint/extensions/pixel_local/entry_point_use/multiple_outputs/multiple_attachments.wgsl:2:8 error: GLSL backend does not support extension 'chromium_experimental_pixel_local'
enable chromium_experimental_pixel_local;
       ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

