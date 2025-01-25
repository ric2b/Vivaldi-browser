SKIP: FAILED


@group(0) @binding(0) var<storage, read_write> prevent_dce : vec3<u32>;

@group(1) @binding(0) var arg_0 : texture_3d<u32>;

fn textureDimensions_e5a203() -> vec3<u32> {
  var res : vec3<u32> = textureDimensions(arg_0, 1i);
  return res;
}

@fragment
fn fragment_main() {
  prevent_dce = textureDimensions_e5a203();
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = textureDimensions_e5a203();
}

struct VertexOutput {
  @builtin(position)
  pos : vec4<f32>,
  @location(0) @interpolate(flat)
  prevent_dce : vec3<u32>,
}

@vertex
fn vertex_main() -> VertexOutput {
  var tint_symbol : VertexOutput;
  tint_symbol.pos = vec4<f32>();
  tint_symbol.prevent_dce = textureDimensions_e5a203();
  return tint_symbol;
}

Failed to generate: :28:49 error: var: initializer type 'vec2<u32>' does not match store type 'vec3<u32>'
    %res:ptr<function, vec3<u32>, read_write> = var, %13
                                                ^^^

:17:3 note: in block
  $B2: {
  ^^^

note: # Disassembly
VertexOutput = struct @align(16) {
  pos:vec4<f32> @offset(0)
  prevent_dce:vec3<u32> @offset(16)
}

vertex_main_outputs = struct @align(16) {
  VertexOutput_prevent_dce:vec3<u32> @offset(0), @location(0), @interpolate(flat)
  VertexOutput_pos:vec4<f32> @offset(16), @builtin(position)
}

$B1: {  # root
  %prevent_dce:hlsl.byte_address_buffer<read_write> = var @binding_point(0, 0)
  %arg_0:ptr<handle, texture_3d<u32>, read> = var @binding_point(1, 0)
}

%textureDimensions_e5a203 = func():vec3<u32> {
  $B2: {
    %4:texture_3d<u32> = load %arg_0
    %5:u32 = convert 1i
    %6:ptr<function, vec4<u32>, read_write> = var
    %7:ptr<function, u32, read_write> = access %6, 0u
    %8:ptr<function, u32, read_write> = access %6, 1u
    %9:ptr<function, u32, read_write> = access %6, 2u
    %10:ptr<function, u32, read_write> = access %6, 3u
    %11:void = %4.GetDimensions %5, %7, %8, %9, %10
    %12:vec4<u32> = load %6
    %13:vec2<u32> = swizzle %12, xyz
    %res:ptr<function, vec3<u32>, read_write> = var, %13
    %15:vec3<u32> = load %res
    ret %15
  }
}
%fragment_main = @fragment func():void {
  $B3: {
    %17:vec3<u32> = call %textureDimensions_e5a203
    %18:void = %prevent_dce.Store3 0u, %17
    ret
  }
}
%compute_main = @compute @workgroup_size(1, 1, 1) func():void {
  $B4: {
    %20:vec3<u32> = call %textureDimensions_e5a203
    %21:void = %prevent_dce.Store3 0u, %20
    ret
  }
}
%vertex_main_inner = func():VertexOutput {
  $B5: {
    %tint_symbol:ptr<function, VertexOutput, read_write> = var
    %24:ptr<function, vec4<f32>, read_write> = access %tint_symbol, 0u
    store %24, vec4<f32>(0.0f)
    %25:ptr<function, vec3<u32>, read_write> = access %tint_symbol, 1u
    %26:vec3<u32> = call %textureDimensions_e5a203
    store %25, %26
    %27:VertexOutput = load %tint_symbol
    ret %27
  }
}
%vertex_main = @vertex func():vertex_main_outputs {
  $B6: {
    %29:VertexOutput = call %vertex_main_inner
    %30:vec4<f32> = access %29, 0u
    %31:vec3<u32> = access %29, 1u
    %32:vertex_main_outputs = construct %31, %30
    ret %32
  }
}


tint executable returned error: exit status 1
