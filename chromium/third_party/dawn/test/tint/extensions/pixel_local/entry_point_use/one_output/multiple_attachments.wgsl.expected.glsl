SKIP: INVALID


enable chromium_experimental_pixel_local;

struct PixelLocal {
  a : u32,
  b : i32,
  c : f32,
}

var<pixel_local> P : PixelLocal;

@fragment
fn f() -> @location(0) vec4f {
  P.a += 42;
  return vec4f(2);
}

Failed to generate: <dawn>/test/tint/extensions/pixel_local/entry_point_use/one_output/multiple_attachments.wgsl:2:8 error: GLSL backend does not support extension 'chromium_experimental_pixel_local'
enable chromium_experimental_pixel_local;
       ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

