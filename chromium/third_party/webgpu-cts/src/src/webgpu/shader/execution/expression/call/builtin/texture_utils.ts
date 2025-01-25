import { assert, range, unreachable } from '../../../../../../common/util/util.js';
import {
  EncodableTextureFormat,
  isCompressedFloatTextureFormat,
  isCompressedTextureFormat,
  isDepthOrStencilTextureFormat,
  isDepthTextureFormat,
  isStencilTextureFormat,
  kEncodableTextureFormats,
  kTextureFormatInfo,
} from '../../../../../format_info.js';
import {
  GPUTest,
  GPUTestSubcaseBatchState,
  TextureTestMixinType,
} from '../../../../../gpu_test.js';
import {
  align,
  clamp,
  dotProduct,
  hashU32,
  lcm,
  lerp,
  quantizeToF32,
} from '../../../../../util/math.js';
import {
  effectiveViewDimensionForDimension,
  physicalMipSizeFromTexture,
  reifyTextureDescriptor,
  SampleCoord,
  virtualMipSize,
} from '../../../../../util/texture/base.js';
import {
  kTexelRepresentationInfo,
  NumericRange,
  PerComponentNumericRange,
  PerTexelComponent,
  TexelComponent,
  TexelRepresentationInfo,
} from '../../../../../util/texture/texel_data.js';
import { TexelView } from '../../../../../util/texture/texel_view.js';
import { createTextureFromTexelViews } from '../../../../../util/texture.js';
import { reifyExtent3D } from '../../../../../util/unions.js';

export type SampledType = 'f32' | 'i32' | 'u32';

export const kSampleTypeInfo = {
  f32: {
    format: 'rgba8unorm',
  },
  i32: {
    format: 'rgba8sint',
  },
  u32: {
    format: 'rgba8uint',
  },
} as const;

/**
 * Return the texture type for a given view dimension
 */
export function getTextureTypeForTextureViewDimension(viewDimension: GPUTextureViewDimension) {
  switch (viewDimension) {
    case '1d':
      return 'texture_1d<f32>';
    case '2d':
      return 'texture_2d<f32>';
    case '2d-array':
      return 'texture_2d_array<f32>';
    case '3d':
      return 'texture_3d<f32>';
    case 'cube':
      return 'texture_cube<f32>';
    case 'cube-array':
      return 'texture_cube_array<f32>';
    default:
      unreachable();
  }
}

const is32Float = (format: GPUTextureFormat) =>
  format === 'r32float' || format === 'rg32float' || format === 'rgba32float';

/**
 * Skips a subcase if the filter === 'linear' and the format is type
 * 'unfilterable-float' and we cannot enable filtering.
 */
export function skipIfNeedsFilteringAndIsUnfilterableOrSelectDevice(
  t: GPUTestSubcaseBatchState,
  filter: GPUFilterMode,
  format: GPUTextureFormat
) {
  const features = new Set<GPUFeatureName | undefined>();
  features.add(kTextureFormatInfo[format].feature);

  if (filter === 'linear') {
    t.skipIf(isDepthTextureFormat(format), 'depth texture are unfilterable');

    const type = kTextureFormatInfo[format].color?.type;
    if (type === 'unfilterable-float') {
      assert(is32Float(format));
      features.add('float32-filterable');
    }
  }

  if (features.size > 0) {
    t.selectDeviceOrSkipTestCase(Array.from(features));
  }
}

/**
 * Returns if a texture format can be filled with random data.
 */
export function isFillable(format: GPUTextureFormat) {
  // We can't easily put random bytes into compressed textures if they are float formats
  // since we want the range to be +/- 1000 and not +/- infinity or NaN.
  return !isCompressedTextureFormat(format) || !format.endsWith('float');
}

/**
 * Returns if a texture format can potentially be filtered and can be filled with random data.
 */
export function isPotentiallyFilterableAndFillable(format: GPUTextureFormat) {
  const type = kTextureFormatInfo[format].color?.type;
  const canPotentiallyFilter = type === 'float' || type === 'unfilterable-float';
  return canPotentiallyFilter && isFillable(format);
}

/**
 * skips the test if the texture format is not supported or not available or not filterable.
 */
export function skipIfTextureFormatNotSupportedNotAvailableOrNotFilterable(
  t: GPUTestSubcaseBatchState,
  format: GPUTextureFormat
) {
  t.skipIfTextureFormatNotSupported(format);
  const info = kTextureFormatInfo[format];
  if (info.color?.type === 'unfilterable-float') {
    t.selectDeviceOrSkipTestCase('float32-filterable');
  } else {
    t.selectDeviceForTextureFormatOrSkipTestCase(format);
  }
}

async function queryMipGradientValuesForDevice(t: GPUTest) {
  const { device } = t;
  const module = device.createShaderModule({
    code: `
      @group(0) @binding(0) var tex: texture_2d<f32>;
      @group(0) @binding(1) var smp: sampler;
      @group(0) @binding(2) var<storage, read_write> result: array<f32>;

      @vertex fn vs(@builtin(vertex_index) vNdx: u32) -> @builtin(position) vec4f {
        let pos = array(
          vec2f(-1,  3),
          vec2f( 3, -1),
          vec2f(-1, -1),
        );
        return vec4f(pos[vNdx], 0, 1);
      }
      @fragment fn fs(@builtin(position) pos: vec4f) -> @location(0) vec4f {
        let mipLevel = floor(pos.x) / ${kMipGradientSteps};
        result[u32(pos.x)] = textureSampleLevel(tex, smp, vec2f(0.5), mipLevel).r;
        return vec4f(0);
      }
    `,
  });

  const pipeline = device.createRenderPipeline({
    layout: 'auto',
    vertex: { module },
    fragment: { module, targets: [{ format: 'rgba8unorm' }] },
  });

  const target = t.createTextureTracked({
    size: [kMipGradientSteps + 1, 1, 1],
    format: 'rgba8unorm',
    usage: GPUTextureUsage.RENDER_ATTACHMENT,
  });

  const texture = t.createTextureTracked({
    size: [2, 2, 1],
    format: 'r8unorm',
    usage: GPUTextureUsage.TEXTURE_BINDING | GPUTextureUsage.COPY_DST,
    mipLevelCount: 2,
  });

  device.queue.writeTexture(
    { texture, mipLevel: 1 },
    new Uint8Array([255]),
    { bytesPerRow: 1 },
    [1, 1]
  );

  const sampler = device.createSampler({
    minFilter: 'linear',
    magFilter: 'linear',
    mipmapFilter: 'linear',
  });

  const storageBuffer = t.createBufferTracked({
    size: 4 * (kMipGradientSteps + 1),
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_SRC,
  });

  const resultBuffer = t.createBufferTracked({
    size: storageBuffer.size,
    usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ,
  });

  const bindGroup = device.createBindGroup({
    layout: pipeline.getBindGroupLayout(0),
    entries: [
      { binding: 0, resource: texture.createView() },
      { binding: 1, resource: sampler },
      { binding: 2, resource: { buffer: storageBuffer } },
    ],
  });

  const encoder = device.createCommandEncoder();
  const pass = encoder.beginRenderPass({
    colorAttachments: [
      {
        view: target.createView(),
        loadOp: 'clear',
        storeOp: 'store',
      },
    ],
  });
  pass.setPipeline(pipeline);
  pass.setBindGroup(0, bindGroup);
  pass.draw(3);
  pass.end();
  encoder.copyBufferToBuffer(storageBuffer, 0, resultBuffer, 0, resultBuffer.size);
  device.queue.submit([encoder.finish()]);

  await resultBuffer.mapAsync(GPUMapMode.READ);
  const weights = Array.from(new Float32Array(resultBuffer.getMappedRange()));
  resultBuffer.unmap();

  texture.destroy();
  storageBuffer.destroy();
  resultBuffer.destroy();

  const showWeights = () => weights.map((v, i) => `${i.toString().padStart(2)}: ${v}`).join('\n');

  // Validate the weights
  assert(weights[0] === 0, `weight 0 expected 0 but was ${weights[0]}\n${showWeights()}`);
  assert(
    weights[kMipGradientSteps] === 1,
    `top weight expected 1 but was ${weights[kMipGradientSteps]}\n${showWeights()}`
  );
  assert(
    Math.abs(weights[kMipGradientSteps / 2] - 0.5) < 0.0001,
    `middle weight expected approximately 0.5 but was ${
      weights[kMipGradientSteps / 2]
    }\n${showWeights()}`
  );

  // Note: for 16 steps, these are the AMD weights
  //
  //                 standard
  // step  mipLevel    gpu        AMD
  // ----  --------  --------  ----------
  //  0:   0         0           0
  //  1:   0.0625    0.0625      0
  //  2:   0.125     0.125       0.03125
  //  3:   0.1875    0.1875      0.109375
  //  4:   0.25      0.25        0.1875
  //  5:   0.3125    0.3125      0.265625
  //  6:   0.375     0.375       0.34375
  //  7:   0.4375    0.4375      0.421875
  //  8:   0.5       0.5         0.5
  //  9:   0.5625    0.5625      0.578125
  // 10:   0.625     0.625       0.65625
  // 11:   0.6875    0.6875      0.734375
  // 12:   0.75      0.75        0.8125
  // 13:   0.8125    0.8125      0.890625
  // 14:   0.875     0.875       0.96875
  // 15:   0.9375    0.9375      1
  // 16:   1         1           1
  //
  // notice step 1 is 0 and step 15 is 1.
  // so we only check the 1 through 14.
  for (let i = 1; i < kMipGradientSteps - 1; ++i) {
    assert(
      weights[i] < weights[i + 1],
      `weight[${i}] was not less than < weight[${i + 1}]\n${showWeights()}`
    );
  }

  s_deviceToMipGradientValues.set(device, weights);
}

/**
 * Gets the mip gradient values for the current device.
 * The issue is, different GPUs have different ways of mixing between mip levels.
 * For most GPUs it's linear but for AMD GPUs on Mac in particular, it's something
 * else (which AFAICT is against all the specs).
 *
 * We seemingly have 3 options:
 *
 * 1. Increase the tolerances of tests so they pass on AMD.
 * 2. Mark AMD as failing
 * 3. Try to figure out how the GPU converts mip levels into weights
 *
 * We're doing 3.
 *
 * There's an assumption that the gradient will be the same for all formats
 * and usages.
 *
 * Note: The code below has 2 maps. One device->Promise, the other device->weights
 * device->weights is meant to be used synchronously by other code so we don't
 * want to leave initMipGradientValuesForDevice until the weights have been read.
 * But, multiple subcases will run because this function is async. So, subcase 1
 * runs, hits this init code, this code waits for the weights. Then, subcase 2
 * runs and hits this init code. The weights will not be in the device->weights map
 * yet which is why we have the device->Promise map. This is so subcase 2 waits
 * for subcase 1's "query the weights" step. Otherwise, all subcases would do the
 * "get the weights" step separately.
 */
const kMipGradientSteps = 16;
const s_deviceToMipGradientValuesPromise = new WeakMap<GPUDevice, Promise<void>>();
const s_deviceToMipGradientValues = new WeakMap<GPUDevice, number[]>();
async function initMipGradientValuesForDevice(t: GPUTest) {
  const { device } = t;
  let weightsP = s_deviceToMipGradientValuesPromise.get(device);
  if (!weightsP) {
    weightsP = queryMipGradientValuesForDevice(t);
    s_deviceToMipGradientValuesPromise.set(device, weightsP);
  }
  return await weightsP;
}

function getWeightForMipLevel(t: GPUTest, mipLevelCount: number, mipLevel: number) {
  if (mipLevel < 0 || mipLevel >= mipLevelCount) {
    return 1;
  }
  // linear interpolate between weights
  const weights = s_deviceToMipGradientValues.get(t.device);
  assert(
    !!weights,
    'you must use WGSLTextureSampleTest or call initializeDeviceMipWeights before calling this function'
  );
  const steps = weights.length - 1;
  const w = (mipLevel % 1) * steps;
  const lowerNdx = Math.floor(w);
  const upperNdx = Math.ceil(w);
  const mix = w % 1;
  return lerp(weights[lowerNdx], weights[upperNdx], mix);
}

/**
 * Used for textureDimension, textureNumLevels, textureNumLayers
 */
export class WGSLTextureQueryTest extends GPUTest {
  executeAndExpectResult(code: string, view: GPUTextureView, expected: number[]) {
    const { device } = this;
    const module = device.createShaderModule({ code });
    const pipeline = device.createComputePipeline({
      layout: 'auto',
      compute: {
        module,
      },
    });

    const resultBuffer = this.createBufferTracked({
      size: 16,
      usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_SRC,
    });

    const bindGroup = device.createBindGroup({
      layout: pipeline.getBindGroupLayout(0),
      entries: [
        { binding: 0, resource: view },
        { binding: 1, resource: { buffer: resultBuffer } },
      ],
    });

    const encoder = device.createCommandEncoder();
    const pass = encoder.beginComputePass();
    pass.setPipeline(pipeline);
    pass.setBindGroup(0, bindGroup);
    pass.dispatchWorkgroups(1);
    pass.end();
    device.queue.submit([encoder.finish()]);

    const e = new Uint32Array(4);
    e.set(expected);
    this.expectGPUBufferValuesEqual(resultBuffer, e);
  }
}

/**
 * Used for textureSampleXXX
 */
export class WGSLTextureSampleTest extends GPUTest {
  override async init(): Promise<void> {
    await super.init();
    await initMipGradientValuesForDevice(this);
  }
}

/**
 * Used to specify a range from [0, num)
 * The type is used to determine if values should be integers and if they can be negative.
 */
export type RangeDef = {
  num: number;
  type: 'f32' | 'i32' | 'u32';
};

function getLimitValue(v: number) {
  switch (v) {
    case Number.POSITIVE_INFINITY:
      return 1000;
    case Number.NEGATIVE_INFINITY:
      return -1000;
    default:
      return v;
  }
}

function getValueBetweenMinAndMaxTexelValueInclusive(
  rep: TexelRepresentationInfo,
  component: TexelComponent,
  normalized: number
) {
  assert(!!rep.numericRange);
  const perComponentRanges = rep.numericRange as PerComponentNumericRange;
  const perComponentRange = perComponentRanges[component];
  const range = rep.numericRange as NumericRange;
  const { min, max } = perComponentRange ? perComponentRange : range;
  return lerp(getLimitValue(min), getLimitValue(max), normalized);
}

/**
 * We need the software rendering to do the same interpolation as the hardware
 * rendered so for -srgb formats we set the TexelView to an -srgb format as
 * TexelView handles this case. Note: It might be nice to add rgba32float-srgb
 * or something similar to TexelView.
 */
export function getTexelViewFormatForTextureFormat(format: GPUTextureFormat) {
  return format.endsWith('-srgb') ? 'rgba8unorm-srgb' : 'rgba32float';
}

const kTextureTypeInfo = {
  depth: {
    componentType: 'f32',
    resultType: 'vec4f',
    resultFormat: 'rgba32float',
  },
  float: {
    componentType: 'f32',
    resultType: 'vec4f',
    resultFormat: 'rgba32float',
  },
  'unfilterable-float': {
    componentType: 'f32',
    resultType: 'vec4f',
    resultFormat: 'rgba32float',
  },
  sint: {
    componentType: 'i32',
    resultType: 'vec4i',
    resultFormat: 'rgba32sint',
  },
  uint: {
    componentType: 'u32',
    resultType: 'vec4u',
    resultFormat: 'rgba32uint',
  },
} as const;

function getTextureFormatTypeInfo(format: GPUTextureFormat) {
  const info = kTextureFormatInfo[format];
  const type = info.color?.type ?? info.depth?.type ?? info.stencil?.type;
  assert(!!type);
  return kTextureTypeInfo[type];
}

/**
 * given a texture type 'base', returns the base with the correct component for the given texture format.
 * eg: `getTextureType('texture_2d', someUnsignedIntTextureFormat)` -> `texture_2d<u32>`
 */
export function appendComponentTypeForFormatToTextureType(base: string, format: GPUTextureFormat) {
  return base.includes('depth')
    ? base
    : `${base}<${getTextureFormatTypeInfo(format).componentType}>`;
}

/**
 * Creates a TexelView filled with random values.
 */
export function createRandomTexelView(info: {
  format: GPUTextureFormat;
  size: GPUExtent3D;
  mipLevel: number;
}): TexelView {
  const rep = kTexelRepresentationInfo[info.format as EncodableTextureFormat];
  const size = reifyExtent3D(info.size);
  const generator = (coords: SampleCoord): Readonly<PerTexelComponent<number>> => {
    const texel: PerTexelComponent<number> = {};
    for (const component of rep.componentOrder) {
      const rnd = hashU32(
        coords.x,
        coords.y,
        coords.z,
        coords.sampleIndex ?? 0,
        component.charCodeAt(0),
        info.mipLevel,
        size.width,
        size.height,
        size.depthOrArrayLayers
      );
      const normalized = clamp(rnd / 0xffffffff, { min: 0, max: 1 });
      texel[component] = getValueBetweenMinAndMaxTexelValueInclusive(rep, component, normalized);
    }
    return quantize(texel, rep);
  };
  return TexelView.fromTexelsAsColors(info.format as EncodableTextureFormat, generator);
}

/**
 * Creates a mip chain of TexelViews filled with random values
 */
export function createRandomTexelViewMipmap(info: {
  format: GPUTextureFormat;
  size: GPUExtent3D;
  mipLevelCount?: number;
  dimension?: GPUTextureDimension;
}): TexelView[] {
  const mipLevelCount = info.mipLevelCount ?? 1;
  const dimension = info.dimension ?? '2d';
  return range(mipLevelCount, i =>
    createRandomTexelView({
      format: info.format,
      size: virtualMipSize(dimension, info.size, i),
      mipLevel: i,
    })
  );
}

export type vec1 = [number]; // Because it's easy to deal with if these types are all array of number
export type vec2 = [number, number];
export type vec3 = [number, number, number];
export type vec4 = [number, number, number, number];
export type Dimensionality = vec1 | vec2 | vec3;

type TextureCallArgKeys = keyof TextureCallArgs<vec1>;
const kTextureCallArgNames: readonly TextureCallArgKeys[] = [
  'component',
  'coords',
  'arrayIndex',
  'sampleIndex',
  'mipLevel',
  'ddx',
  'ddy',
  'depthRef',
  'offset',
] as const;

export interface TextureCallArgs<T extends Dimensionality> {
  component?: number;
  coords?: T;
  mipLevel?: number;
  arrayIndex?: number;
  sampleIndex?: number;
  depthRef?: number;
  ddx?: T;
  ddy?: T;
  offset?: T;
}

export type TextureBuiltin =
  | 'textureGather'
  | 'textureGatherCompare'
  | 'textureLoad'
  | 'textureSample'
  | 'textureSampleBaseClampToEdge'
  | 'textureSampleLevel';

export interface TextureCall<T extends Dimensionality> extends TextureCallArgs<T> {
  builtin: TextureBuiltin;
  coordType: 'f' | 'i' | 'u';
  levelType?: 'i' | 'u' | 'f';
  arrayIndexType?: 'i' | 'u';
  sampleIndexType?: 'i' | 'u';
  componentType?: 'i' | 'u';
}

const isBuiltinComparison = (builtin: TextureBuiltin) => builtin === 'textureGatherCompare';
const isBuiltinGather = (builtin: TextureBuiltin | undefined) =>
  builtin === 'textureGather' || builtin === 'textureGatherCompare';
const builtinNeedsSampler = (builtin: TextureBuiltin) =>
  builtin.startsWith('textureSample') || builtin.startsWith('textureGather');

const isCubeViewDimension = (viewDescriptor?: GPUTextureViewDescriptor) =>
  viewDescriptor?.dimension === 'cube' || viewDescriptor?.dimension === 'cube-array';

const s_u32 = new Uint32Array(1);
const s_f32 = new Float32Array(s_u32.buffer);
const s_i32 = new Int32Array(s_u32.buffer);

const kBitCastFunctions = {
  f: (v: number) => {
    s_f32[0] = v;
    return s_u32[0];
  },
  i: (v: number) => {
    s_i32[0] = v;
    assert(s_i32[0] === v, 'check we are not casting non-int or out-of-range value');
    return s_u32[0];
  },
  u: (v: number) => {
    s_u32[0] = v;
    assert(s_u32[0] === v, 'check we are not casting non-uint or out-of-range value');
    return s_u32[0];
  },
};

function getCallArgType<T extends Dimensionality>(
  call: TextureCall<T>,
  argName: (typeof kTextureCallArgNames)[number]
) {
  switch (argName) {
    case 'coords':
      return call.coordType;
    case 'component':
      assert(call.componentType !== undefined);
      return call.componentType;
    case 'mipLevel':
      assert(call.levelType !== undefined);
      return call.levelType;
    case 'arrayIndex':
      assert(call.arrayIndexType !== undefined);
      return call.arrayIndexType;
    case 'sampleIndex':
      assert(call.sampleIndexType !== undefined);
      return call.sampleIndexType;
    case 'depthRef':
    case 'ddx':
    case 'ddy':
      return 'f';
    default:
      unreachable();
  }
}

function toArray(coords: Dimensionality): number[] {
  if (coords instanceof Array) {
    return coords;
  }
  return [coords];
}

function quantize(texel: PerTexelComponent<number>, repl: TexelRepresentationInfo) {
  return repl.bitsToNumber(repl.unpackBits(new Uint8Array(repl.pack(repl.encode(texel)))));
}

function apply(a: number[], b: number[], op: (x: number, y: number) => number) {
  assert(a.length === b.length, `apply(${a}, ${b}): arrays must have same length`);
  return a.map((v, i) => op(v, b[i]));
}

/**
 * At the corner of a cubemap we need to sample just 3 texels, not 4.
 * The texels are in
 *
 *   0:  (u,v)
 *   1:  (u + 1, v)
 *   2:  (u, v + 1)
 *   3:  (u + 1, v + 1)
 *
 * We pass in the original 2d (converted from cubemap) texture coordinate.
 * If it's within half a pixel of the edge in both directions then it's
 * a corner so we return the index of the one texel that's not needed.
 * Otherwise we return -1.
 */
function getUnusedCubeCornerSampleIndex(textureSize: number, coords: vec3) {
  const u = coords[0] * textureSize;
  const v = coords[1] * textureSize;
  if (v < 0.5) {
    if (u < 0.5) {
      return 0;
    } else if (u >= textureSize - 0.5) {
      return 1;
    }
  } else if (v >= textureSize - 0.5) {
    if (u < 0.5) {
      return 2;
    } else if (u >= textureSize - 0.5) {
      return 3;
    }
  }
  return -1;
}

const add = (a: number[], b: number[]) => apply(a, b, (x, y) => x + y);

export interface Texture {
  texels: TexelView[];
  descriptor: GPUTextureDescriptor;
  viewDescriptor: GPUTextureViewDescriptor;
}

/**
 * Converts the src texel representation to an RGBA representation.
 */
function convertPerTexelComponentToResultFormat(
  src: PerTexelComponent<number>,
  format: EncodableTextureFormat
): PerTexelComponent<number> {
  const rep = kTexelRepresentationInfo[format];
  const out: PerTexelComponent<number> = { R: 0, G: 0, B: 0, A: 1 };
  for (const component of rep.componentOrder) {
    switch (component) {
      case 'Stencil':
      case 'Depth':
        out.R = src[component];
        break;
      default:
        assert(out[component] !== undefined); // checks that component = R, G, B or A
        out[component] = src[component];
    }
  }
  return out;
}

/**
 * Convert RGBA result format to texel view format of src texture.
 * Effectively this converts something like { R: 0.1, G: 0, B: 0, A: 1 }
 * to { Depth: 0.1 }
 */
function convertResultFormatToTexelViewFormat(
  src: PerTexelComponent<number>,
  format: EncodableTextureFormat
): PerTexelComponent<number> {
  const rep = kTexelRepresentationInfo[format];
  const out: PerTexelComponent<number> = {};
  for (const component of rep.componentOrder) {
    out[component] = src[component] ?? src.R;
  }
  return out;
}

function zeroValuePerTexelComponent(components: TexelComponent[]) {
  const out: PerTexelComponent<number> = {};
  for (const component of components) {
    out[component] = 0;
  }
  return out;
}

const kSamplerFns: Record<GPUCompareFunction, (ref: number, v: number) => boolean> = {
  never: (ref: number, v: number) => false,
  less: (ref: number, v: number) => ref < v,
  equal: (ref: number, v: number) => ref === v,
  'less-equal': (ref: number, v: number) => ref <= v,
  greater: (ref: number, v: number) => ref > v,
  'not-equal': (ref: number, v: number) => ref !== v,
  'greater-equal': (ref: number, v: number) => ref >= v,
  always: (ref: number, v: number) => true,
} as const;

function applyCompare<T extends Dimensionality>(
  call: TextureCall<T>,
  sampler: GPUSamplerDescriptor | undefined,
  components: TexelComponent[],
  src: PerTexelComponent<number>
): PerTexelComponent<number> {
  if (isBuiltinComparison(call.builtin)) {
    assert(sampler !== undefined);
    assert(call.depthRef !== undefined);
    const out: PerTexelComponent<number> = {};
    const compareFn = kSamplerFns[sampler.compare!];
    for (const component of components) {
      out[component] = compareFn(call.depthRef, src[component]!) ? 1 : 0;
    }
    return out;
  } else {
    return src;
  }
}

/**
 * Returns the expect value for a WGSL builtin texture function for a single
 * mip level
 */
export function softwareTextureReadMipLevel<T extends Dimensionality>(
  call: TextureCall<T>,
  texture: Texture,
  sampler: GPUSamplerDescriptor | undefined,
  mipLevel: number
): PerTexelComponent<number> {
  assert(mipLevel % 1 === 0);
  const { format } = texture.texels[0];
  const rep = kTexelRepresentationInfo[format];
  const textureSize = virtualMipSize(
    texture.descriptor.dimension || '2d',
    texture.descriptor.size,
    mipLevel
  );
  const addressMode: GPUAddressMode[] =
    call.builtin === 'textureSampleBaseClampToEdge'
      ? ['clamp-to-edge', 'clamp-to-edge', 'clamp-to-edge']
      : [
          sampler?.addressModeU ?? 'clamp-to-edge',
          sampler?.addressModeV ?? 'clamp-to-edge',
          sampler?.addressModeW ?? 'clamp-to-edge',
        ];

  const isCube = isCubeViewDimension(texture.viewDescriptor);
  const arrayIndexMult = isCube ? 6 : 1;
  const numLayers = textureSize[2] / arrayIndexMult;
  assert(numLayers % 1 === 0);
  const textureSizeForCube = [textureSize[0], textureSize[1], 6];

  const load = (at: number[]) => {
    const zFromArrayIndex =
      call.arrayIndex !== undefined
        ? clamp(call.arrayIndex, { min: 0, max: numLayers - 1 }) * arrayIndexMult
        : 0;
    return texture.texels[mipLevel].color({
      x: Math.floor(at[0]),
      y: Math.floor(at[1] ?? 0),
      z: Math.floor(at[2] ?? 0) + zFromArrayIndex,
      sampleIndex: call.sampleIndex,
    });
  };

  switch (call.builtin) {
    case 'textureGather':
    case 'textureGatherCompare':
    case 'textureSample':
    case 'textureSampleBaseClampToEdge':
    case 'textureSampleLevel': {
      let coords = toArray(call.coords!);

      if (isCube) {
        coords = convertCubeCoordToNormalized3DTextureCoord(coords as vec3);
      }

      // convert normalized to absolute texel coordinate
      // ┌───┬───┬───┬───┐
      // │ a │   │   │   │  norm: a = 1/8, b = 7/8
      // ├───┼───┼───┼───┤   abs: a = 0,   b = 3
      // │   │   │   │   │
      // ├───┼───┼───┼───┤
      // │   │   │   │   │
      // ├───┼───┼───┼───┤
      // │   │   │   │ b │
      // └───┴───┴───┴───┘
      let at = coords.map((v, i) => v * (isCube ? textureSizeForCube : textureSize)[i] - 0.5);

      // Apply offset in whole texel units
      // This means the offset is added at each mip level in texels. There's no
      // scaling for each level.
      if (call.offset !== undefined) {
        at = add(at, toArray(call.offset));
      }

      const samples: { at: number[]; weight: number }[] = [];

      const filter = isBuiltinGather(call.builtin) ? 'linear' : sampler?.minFilter ?? 'nearest';
      switch (filter) {
        case 'linear': {
          // 'p0' is the lower texel for 'at'
          const p0 = at.map(v => Math.floor(v));
          // 'p1' is the higher texel for 'at'
          // If it's cube then don't advance Z.
          const p1 = p0.map((v, i) => v + (isCube ? (i === 2 ? 0 : 1) : 1));

          // interpolation weights for p0 and p1
          const p1W = at.map((v, i) => v - p0[i]);
          const p0W = p1W.map(v => 1 - v);

          switch (coords.length) {
            case 1:
              samples.push({ at: p0, weight: p0W[0] });
              samples.push({ at: p1, weight: p1W[0] });
              break;
            case 2: {
              // Note: These are ordered to match textureGather
              samples.push({ at: [p0[0], p1[1]], weight: p0W[0] * p1W[1] });
              samples.push({ at: p1, weight: p1W[0] * p1W[1] });
              samples.push({ at: [p1[0], p0[1]], weight: p1W[0] * p0W[1] });
              samples.push({ at: p0, weight: p0W[0] * p0W[1] });
              break;
            }
            case 3: {
              // cube sampling, here in the software renderer, is the same
              // as 2d sampling. We'll sample at most 4 texels. The weights are
              // the same as if it was just one plane. If the points fall outside
              // the slice they'll be wrapped by wrapFaceCoordToCubeFaceAtEdgeBoundaries
              // below.
              if (isCube) {
                // Note: These are ordered to match textureGather
                samples.push({ at: [p0[0], p1[1], p0[2]], weight: p0W[0] * p1W[1] });
                samples.push({ at: p1, weight: p1W[0] * p1W[1] });
                samples.push({ at: [p1[0], p0[1], p0[2]], weight: p1W[0] * p0W[1] });
                samples.push({ at: p0, weight: p0W[0] * p0W[1] });
                const ndx = getUnusedCubeCornerSampleIndex(textureSize[0], coords as vec3);
                if (ndx >= 0) {
                  // # Issues with corners of cubemaps
                  //
                  // note: I tried multiple things here
                  //
                  // 1. distribute 1/3 of the weight of the removed sample to each of the remaining samples
                  // 2. distribute 1/2 of the weight of the removed sample to the 2 samples that are not the "main" sample.
                  // 3. normalize the weights of the remaining 3 samples.
                  //
                  // none of them matched the M1 in all cases. Checking the dEQP I found this comment
                  //
                  // > If any of samples is out of both edges, implementations can do pretty much anything according to spec.
                  // https://github.com/KhronosGroup/VK-GL-CTS/blob/d2d6aa65607383bb29c8398fe6562c6b08b4de57/framework/common/tcuTexCompareVerifier.cpp#L882
                  //
                  // If I understand this correctly it matches the OpenGL ES 3.1 spec it says
                  // it's implementation defined.
                  //
                  // > OpenGL ES 3.1 section 8.12.1 Seamless Cubemap Filtering
                  // >
                  // > -  If a texture sample location would lie in the texture
                  // >    border in both u and v (in one of the corners of the
                  // >    cube), there is no unique neighboring face from which to
                  // >    extract one texel. The recommended method to generate this
                  // >    texel is to average the values of the three available
                  // >    samples. However, implementations are free to construct
                  // >    this fourth texel in another way, so long as, when the
                  // >    three available samples have the same value, this texel
                  // >    also has that value.
                  //
                  // I'm not sure what "average the values of the three available samples"
                  // means. To me that would be (a+b+c)/3 or in other words, set all the
                  // weights to 0.33333 but that's not what the M1 is doing.
                  //
                  // We could check that, given the 3 texels at the corner, if all 3 texels
                  // are the same value then the result must be the same value. Otherwise,
                  // the result must be between the 3 values. For now, the code that
                  // chooses test coordinates avoids corners. This has the restriction
                  // that the smallest mip level be at least 4x4 so there are some non
                  // corners to choose from.
                  unreachable(
                    `corners of cubemaps are not testable:\n   ${describeTextureCall(call)}`
                  );
                }
              } else {
                const p = [p0, p1];
                const w = [p0W, p1W];
                for (let z = 0; z < 2; ++z) {
                  for (let y = 0; y < 2; ++y) {
                    for (let x = 0; x < 2; ++x) {
                      samples.push({
                        at: [p[x][0], p[y][1], p[z][2]],
                        weight: w[x][0] * w[y][1] * w[z][2],
                      });
                    }
                  }
                }
              }
              break;
            }
          }
          break;
        }
        case 'nearest': {
          const p = at.map(v => Math.round(quantizeToF32(v)));
          samples.push({ at: p, weight: 1 });
          break;
        }
        default:
          unreachable();
      }

      if (isBuiltinGather(call.builtin)) {
        const componentNdx = call.component ?? 0;
        assert(componentNdx >= 0 && componentNdx < 4);
        assert(samples.length === 4);
        const component = kRGBAComponents[componentNdx];
        const out: PerTexelComponent<number> = {};
        samples.forEach((sample, i) => {
          const c = isCube
            ? wrapFaceCoordToCubeFaceAtEdgeBoundaries(textureSize[0], sample.at as vec3)
            : applyAddressModesToCoords(addressMode, textureSize, sample.at);
          const v = load(c);
          const postV = applyCompare(call, sampler, rep.componentOrder, v);
          const rgba = convertPerTexelComponentToResultFormat(postV, format);
          out[kRGBAComponents[i]] = rgba[component];
        });
        return out;
      }

      const out: PerTexelComponent<number> = {};
      for (const sample of samples) {
        const c = isCube
          ? wrapFaceCoordToCubeFaceAtEdgeBoundaries(textureSize[0], sample.at as vec3)
          : applyAddressModesToCoords(addressMode, textureSize, sample.at);
        const v = load(c);
        const postV = applyCompare(call, sampler, rep.componentOrder, v);
        for (const component of rep.componentOrder) {
          out[component] = (out[component] ?? 0) + postV[component]! * sample.weight;
        }
      }

      return convertPerTexelComponentToResultFormat(out, format);
    }
    case 'textureLoad': {
      const out: PerTexelComponent<number> = isOutOfBoundsCall(texture, call)
        ? zeroValuePerTexelComponent(rep.componentOrder)
        : load(call.coords!);
      return convertPerTexelComponentToResultFormat(out, format);
    }
    default:
      unreachable();
  }
}

/**
 * Reads a texture, optionally sampling between 2 mipLevels
 */
export function softwareTextureReadLevel<T extends Dimensionality>(
  t: GPUTest,
  call: TextureCall<T>,
  texture: Texture,
  sampler: GPUSamplerDescriptor | undefined,
  mipLevel: number
): PerTexelComponent<number> {
  const mipLevelCount = texture.texels.length;
  const maxLevel = mipLevelCount - 1;

  if (!sampler) {
    return softwareTextureReadMipLevel<T>(call, texture, sampler, mipLevel);
  }

  const effectiveMipmapFilter = isBuiltinGather(call.builtin) ? 'nearest' : sampler.mipmapFilter;
  switch (effectiveMipmapFilter) {
    case 'linear': {
      const clampedMipLevel = clamp(mipLevel, { min: 0, max: maxLevel });
      const baseMipLevel = Math.floor(clampedMipLevel);
      const nextMipLevel = Math.ceil(clampedMipLevel);
      const t0 = softwareTextureReadMipLevel<T>(call, texture, sampler, baseMipLevel);
      const t1 = softwareTextureReadMipLevel<T>(call, texture, sampler, nextMipLevel);
      const mix = getWeightForMipLevel(t, mipLevelCount, mipLevel);
      const values = [
        { v: t0, weight: 1 - mix },
        { v: t1, weight: mix },
      ];
      const out: PerTexelComponent<number> = {};
      for (const { v, weight } of values) {
        for (const component of kRGBAComponents) {
          out[component] = (out[component] ?? 0) + v[component]! * weight;
        }
      }
      return out;
    }
    default: {
      const baseMipLevel = Math.floor(
        clamp(mipLevel + 0.5, { min: 0, max: texture.texels.length - 1 })
      );
      return softwareTextureReadMipLevel<T>(call, texture, sampler, baseMipLevel);
    }
  }
}

/**
 * The software version of a texture builtin (eg: textureSample)
 * Note that this is not a complete implementation. Rather it's only
 * what's needed to generate the correct expected value for the tests.
 */
export function softwareTextureRead<T extends Dimensionality>(
  t: GPUTest,
  call: TextureCall<T>,
  texture: Texture,
  sampler: GPUSamplerDescriptor
): PerTexelComponent<number> {
  assert(call.ddx !== undefined);
  assert(call.ddy !== undefined);
  const texSize = reifyExtent3D(texture.descriptor.size);
  const textureSize = [texSize.width, texSize.height];

  // ddx and ddy are the values that would be passed to textureSampleGrad
  // If we're emulating textureSample then they're the computed derivatives
  // such that if we passed them to textureSampleGrad they'd produce the
  // same result.
  const ddx: readonly number[] = typeof call.ddx === 'number' ? [call.ddx] : call.ddx;
  const ddy: readonly number[] = typeof call.ddy === 'number' ? [call.ddy] : call.ddy;

  // Compute the mip level the same way textureSampleGrad does
  const scaledDdx = ddx.map((v, i) => v * textureSize[i]);
  const scaledDdy = ddy.map((v, i) => v * textureSize[i]);
  const dotDDX = dotProduct(scaledDdx, scaledDdx);
  const dotDDY = dotProduct(scaledDdy, scaledDdy);
  const deltaMax = Math.max(dotDDX, dotDDY);
  // MAINTENANCE_TODO: handle texture view baseMipLevel and mipLevelCount?
  const mipLevel = 0.5 * Math.log2(deltaMax);
  return softwareTextureReadLevel(t, call, texture, sampler, mipLevel);
}

export type TextureTestOptions = {
  ddx?: number; // the derivative we want at sample time
  ddy?: number;
  uvwStart?: readonly [number, number]; // the starting uv value (these are used make the coordinates negative as it uncovered issues on some hardware)
  offset?: readonly [number, number]; // a constant offset
};

/**
 * out of bounds is defined as any of the following being true
 *
 * * coords is outside the range [0, textureDimensions(t, level))
 * * array_index is outside the range [0, textureNumLayers(t))
 * * level is outside the range [0, textureNumLevels(t))
 * * sample_index is outside the range [0, textureNumSamples(s))
 */
function isOutOfBoundsCall<T extends Dimensionality>(texture: Texture, call: TextureCall<T>) {
  assert(call.coords !== undefined);

  const desc = reifyTextureDescriptor(texture.descriptor);
  const { coords, mipLevel, arrayIndex, sampleIndex } = call;

  if (mipLevel !== undefined && (mipLevel < 0 || mipLevel >= desc.mipLevelCount)) {
    return true;
  }

  const size = virtualMipSize(
    texture.descriptor.dimension || '2d',
    texture.descriptor.size,
    mipLevel ?? 0
  );

  for (let i = 0; i < coords.length; ++i) {
    const v = coords[i];
    if (v < 0 || v >= size[i]) {
      return true;
    }
  }

  if (arrayIndex !== undefined) {
    const size = reifyExtent3D(desc.size);
    if (arrayIndex < 0 || arrayIndex >= size.depthOrArrayLayers) {
      return true;
    }
  }

  if (sampleIndex !== undefined) {
    if (sampleIndex < 0 || sampleIndex >= desc.sampleCount) {
      return true;
    }
  }

  return false;
}

function isValidOutOfBoundsValue(
  texture: Texture,
  gotRGBA: PerTexelComponent<number>,
  maxFractionalDiff: number
) {
  // For a texture builtin with no sampler (eg textureLoad),
  // any out of bounds access is allowed to return one of:
  //
  // * the value of any texel in the texture
  // * 0,0,0,0 or 0,0,0,1 if not a depth texture
  // * 0 if a depth texture
  if (texture.descriptor.format.includes('depth')) {
    if (gotRGBA.R === 0) {
      return true;
    }
  } else {
    if (
      gotRGBA.R === 0 &&
      gotRGBA.B === 0 &&
      gotRGBA.G === 0 &&
      (gotRGBA.A === 0 || gotRGBA.A === 1)
    ) {
      return true;
    }
  }

  // Can be any texel value
  for (let mipLevel = 0; mipLevel < texture.texels.length; ++mipLevel) {
    const mipTexels = texture.texels[mipLevel];
    const size = virtualMipSize(
      texture.descriptor.dimension || '2d',
      texture.descriptor.size,
      mipLevel
    );
    const sampleCount = texture.descriptor.sampleCount ?? 1;
    for (let z = 0; z < size[2]; ++z) {
      for (let y = 0; y < size[1]; ++y) {
        for (let x = 0; x < size[0]; ++x) {
          for (let sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex) {
            const texel = mipTexels.color({ x, y, z, sampleIndex });
            const rgba = convertPerTexelComponentToResultFormat(texel, mipTexels.format);
            if (texelsApproximatelyEqual(gotRGBA, rgba, mipTexels.format, maxFractionalDiff)) {
              return true;
            }
          }
        }
      }
    }
  }

  return false;
}

/**
 * For a texture builtin with no sampler (eg textureLoad),
 * any out of bounds access is allowed to return one of:
 *
 * * the value of any texel in the texture
 * * 0,0,0,0 or 0,0,0,1 if not a depth texture
 * * 0 if a depth texture
 */
function okBecauseOutOfBounds<T extends Dimensionality>(
  texture: Texture,
  call: TextureCall<T>,
  gotRGBA: PerTexelComponent<number>,
  maxFractionalDiff: number
) {
  if (!isOutOfBoundsCall(texture, call)) {
    return false;
  }

  return isValidOutOfBoundsValue(texture, gotRGBA, maxFractionalDiff);
}

const kRGBAComponents = [
  TexelComponent.R,
  TexelComponent.G,
  TexelComponent.B,
  TexelComponent.A,
] as const;

const kRComponent = [TexelComponent.R] as const;

function texelsApproximatelyEqual(
  gotRGBA: PerTexelComponent<number>,
  expectRGBA: PerTexelComponent<number>,
  format: EncodableTextureFormat,
  maxFractionalDiff: number
) {
  const rep = kTexelRepresentationInfo[format];
  const got = convertResultFormatToTexelViewFormat(gotRGBA, format);
  const expect = convertResultFormatToTexelViewFormat(expectRGBA, format);
  const gULP = convertPerTexelComponentToResultFormat(
    rep.bitsToULPFromZero(rep.numberToBits(got)),
    format
  );
  const eULP = convertPerTexelComponentToResultFormat(
    rep.bitsToULPFromZero(rep.numberToBits(expect)),
    format
  );

  const rgbaComponentsToCheck = isDepthOrStencilTextureFormat(format)
    ? kRComponent
    : kRGBAComponents;

  for (const component of rgbaComponentsToCheck) {
    const g = gotRGBA[component]!;
    const e = expectRGBA[component]!;
    const absDiff = Math.abs(g - e);
    const ulpDiff = Math.abs(gULP[component]! - eULP[component]!);
    if (ulpDiff > 3 && absDiff > maxFractionalDiff) {
      return false;
    }
  }
  return true;
}

// If it's `textureGather` then we need to convert all values to one component.
// In other words, imagine the format is rg11b10ufloat. If it was
// `textureSample` we'd have `r11, g11, b10, a=1` but for `textureGather`
//
// component = 0 => `r11, r11, r11, r11`
// component = 1 => `g11, g11, g11, g11`
// component = 2 => `b10, b10, b10, b10`
//
// etc..., each from a different texel
//
// The Texel utils don't handle this. So if `component = 2` we take each value,
// copy it to the `B` component, run it through the texel utils so it returns
// the correct ULP for a 10bit float (not an 11 bit float). Then copy it back to
// the channel it came from.
function getULPFromZeroForComponents(
  rgba: PerTexelComponent<number>,
  format: EncodableTextureFormat,
  builtin: TextureBuiltin,
  componentNdx?: number
): PerTexelComponent<number> {
  const rep = kTexelRepresentationInfo[format];
  if (isBuiltinGather(builtin)) {
    const out: PerTexelComponent<number> = {};
    const component = kRGBAComponents[componentNdx ?? 0];
    const temp: PerTexelComponent<number> = { R: 0, G: 0, B: 0, A: 1 };
    for (const comp of kRGBAComponents) {
      temp[component] = rgba[comp];
      const texel = convertResultFormatToTexelViewFormat(temp, format);
      const ulp = convertPerTexelComponentToResultFormat(
        rep.bitsToULPFromZero(rep.numberToBits(texel)),
        format
      );
      out[comp] = ulp[component];
    }
    return out;
  } else {
    const texel = convertResultFormatToTexelViewFormat(rgba, format);
    return convertPerTexelComponentToResultFormat(
      rep.bitsToULPFromZero(rep.numberToBits(texel)),
      format
    );
  }
}

/**
 * Checks the result of each call matches the expected result.
 */
export async function checkCallResults<T extends Dimensionality>(
  t: GPUTest,
  texture: Texture,
  textureType: string,
  sampler: GPUSamplerDescriptor | undefined,
  calls: TextureCall<T>[],
  results: Awaited<ReturnType<typeof doTextureCalls<T>>>
) {
  const errs: string[] = [];
  const format = texture.texels[0].format;
  const size = reifyExtent3D(texture.descriptor.size);
  const maxFractionalDiff =
    sampler?.minFilter === 'linear' ||
    sampler?.magFilter === 'linear' ||
    sampler?.mipmapFilter === 'linear'
      ? getMaxFractionalDiffForTextureFormat(texture.descriptor.format)
      : 0;

  for (let callIdx = 0; callIdx < calls.length; callIdx++) {
    const call = calls[callIdx];
    const gotRGBA = results.results[callIdx];
    const expectRGBA = softwareTextureReadLevel(t, call, texture, sampler, call.mipLevel ?? 0);

    // The spec says depth and stencil have implementation defined values for G, B, and A
    // so if this is `textureGather` and component > 0 then there's nothing to check.
    if (
      isDepthOrStencilTextureFormat(format) &&
      isBuiltinGather(call.builtin) &&
      call.component! > 0
    ) {
      continue;
    }

    if (texelsApproximatelyEqual(gotRGBA, expectRGBA, format, maxFractionalDiff)) {
      continue;
    }

    if (!sampler && okBecauseOutOfBounds(texture, call, gotRGBA, maxFractionalDiff)) {
      continue;
    }

    const gULP = getULPFromZeroForComponents(gotRGBA, format, call.builtin, call.component);
    const eULP = getULPFromZeroForComponents(expectRGBA, format, call.builtin, call.component);

    // from the spec: https://gpuweb.github.io/gpuweb/#reading-depth-stencil
    // depth and stencil values are D, ?, ?, ?
    const rgbaComponentsToCheck =
      isBuiltinGather(call.builtin) || !isDepthOrStencilTextureFormat(format)
        ? kRGBAComponents
        : kRComponent;

    let bad = false;
    const diffs = rgbaComponentsToCheck.map(component => {
      const g = gotRGBA[component]!;
      const e = expectRGBA[component]!;
      const absDiff = Math.abs(g - e);
      const ulpDiff = Math.abs(gULP[component]! - eULP[component]!);
      assert(!Number.isNaN(ulpDiff));
      const maxAbs = Math.max(Math.abs(g), Math.abs(e));
      const relDiff = maxAbs > 0 ? absDiff / maxAbs : 0;
      if (ulpDiff > 3 && absDiff > maxFractionalDiff) {
        bad = true;
      }
      return { absDiff, relDiff, ulpDiff };
    });

    const isFloatType = (format: GPUTextureFormat) => {
      const info = kTextureFormatInfo[format];
      return info.color?.type === 'float' || info.depth?.type === 'depth';
    };
    const fix5 = (n: number) => (isFloatType(format) ? n.toFixed(5) : n.toString());
    const fix5v = (arr: number[]) => arr.map(v => fix5(v)).join(', ');
    const rgbaToArray = (p: PerTexelComponent<number>): number[] =>
      rgbaComponentsToCheck.map(component => p[component]!);

    if (bad) {
      const desc = describeTextureCall(call);
      errs.push(`result was not as expected:
      size: [${size.width}, ${size.height}, ${size.depthOrArrayLayers}]
  mipCount: ${texture.descriptor.mipLevelCount ?? 1}
      call: ${desc}  // #${callIdx}`);
      if (isCubeViewDimension(texture.viewDescriptor)) {
        const coord = convertCubeCoordToNormalized3DTextureCoord(call.coords as vec3);
        const faceNdx = Math.floor(coord[2] * 6);
        errs.push(`          : as 3D texture coord: (${coord[0]}, ${coord[1]}, ${coord[2]})`);
        for (let mipLevel = 0; mipLevel < (texture.descriptor.mipLevelCount ?? 1); ++mipLevel) {
          const mipSize = virtualMipSize(
            texture.descriptor.dimension ?? '2d',
            texture.descriptor.size,
            mipLevel
          );
          const t = coord.slice(0, 2).map((v, i) => (v * mipSize[i]).toFixed(3));
          errs.push(
            `          : as texel coord mip level[${mipLevel}]: (${t[0]}, ${t[1]}), face: ${faceNdx}(${kFaceNames[faceNdx]})`
          );
        }
      } else {
        for (let mipLevel = 0; mipLevel < (texture.descriptor.mipLevelCount ?? 1); ++mipLevel) {
          const mipSize = virtualMipSize(
            texture.descriptor.dimension ?? '2d',
            texture.descriptor.size,
            mipLevel
          );
          const t = call.coords!.map((v, i) => (v * mipSize[i]).toFixed(3));
          errs.push(`          : as texel coord @ mip level[${mipLevel}]: (${t.join(', ')})`);
        }
      }
      errs.push(`\
       got: ${fix5v(rgbaToArray(gotRGBA))}
  expected: ${fix5v(rgbaToArray(expectRGBA))}
  max diff: ${maxFractionalDiff}
 abs diffs: ${fix5v(diffs.map(({ absDiff }) => absDiff))}
 rel diffs: ${diffs.map(({ relDiff }) => `${(relDiff * 100).toFixed(2)}%`).join(', ')}
 ulp diffs: ${diffs.map(({ ulpDiff }) => ulpDiff).join(', ')}
`);

      if (sampler) {
        if (t.rec.debugging) {
          const expectedSamplePoints = [
            'expected:',
            ...(await identifySamplePoints(texture, call, (texels: TexelView[]) => {
              return Promise.resolve(
                softwareTextureReadLevel(
                  t,
                  call,
                  {
                    texels,
                    descriptor: texture.descriptor,
                    viewDescriptor: texture.viewDescriptor,
                  },
                  sampler,
                  call.mipLevel ?? 0
                )
              );
            })),
          ];
          const gotSamplePoints = [
            'got:',
            ...(await identifySamplePoints(texture, call, async (texels: TexelView[]) => {
              const gpuTexture = createTextureFromTexelViewsLocal(t, texels, texture.descriptor);
              const result = (await results.run(gpuTexture))[callIdx];
              gpuTexture.destroy();
              return result;
            })),
          ];
          errs.push('  sample points:');
          errs.push(layoutTwoColumns(expectedSamplePoints, gotSamplePoints).join('\n'));
          errs.push('', '');
        }
      } // if (sampler)

      // Don't report the other errors. There 50 sample points per subcase and
      // 50-100 subcases so the log would get enormous if all 50 fail. One
      // report per subcase is enough.
      break;
    } // if (bad)
  } // for cellNdx

  results.destroy();

  return errs.length > 0 ? new Error(errs.join('\n')) : undefined;
}

/**
 * "Renders a quad" to a TexelView with the given parameters,
 * sampling from the given Texture.
 */
export function softwareRasterize<T extends Dimensionality>(
  t: GPUTest,
  texture: Texture,
  sampler: GPUSamplerDescriptor,
  targetSize: [number, number],
  options: TextureTestOptions
) {
  const [width, height] = targetSize;
  const { ddx = 1, ddy = 1, uvwStart = [0, 0] } = options;
  const format = 'rgba32float';

  const textureSize = reifyExtent3D(texture.descriptor.size);

  // MAINTENANCE_TODO: Consider passing these in as a similar computation
  // happens in putDataInTextureThenDrawAndCheckResultsComparedToSoftwareRasterizer.
  // The issue is there, the calculation is "what do we need to multiply the unitQuad
  // by to get the derivatives we want". The calculation here is "what coordinate
  // will we get for a given frag coordinate". It turns out to be the same calculation
  // but needs rephrasing them so they are more obviously the same would help
  // consolidate them into one calculation.
  const screenSpaceUMult = (ddx * width) / textureSize.width;
  const screenSpaceVMult = (ddy * height) / textureSize.height;

  const rep = kTexelRepresentationInfo[format];

  const expData = new Float32Array(width * height * 4);
  for (let y = 0; y < height; ++y) {
    const fragY = height - y - 1 + 0.5;
    for (let x = 0; x < width; ++x) {
      const fragX = x + 0.5;
      // This code calculates the same value that will be passed to
      // `textureSample` in the fragment shader for a given frag coord (see the
      // WGSL code which uses the same formula, but using interpolation). That
      // shader renders a clip space quad and includes a inter-stage "uv"
      // coordinates that start with a unit quad (0,0) to (1,1) and is
      // multiplied by ddx,ddy and as added in uStart and vStart
      //
      // uv = unitQuad * vec2(ddx, ddy) + vec2(vStart, uStart);
      //
      // softwareTextureRead<T> simulates a single call to `textureSample` so
      // here we're computing the `uv` value that will be passed for a
      // particular fragment coordinate. fragX / width, fragY / height provides
      // the unitQuad value.
      //
      // ddx and ddy in this case are the derivative values we want to test. We
      // pass those into the softwareTextureRead<T> as they would normally be
      // derived from the change in coord.
      const coords = [
        (fragX / width) * screenSpaceUMult + uvwStart[0],
        (fragY / height) * screenSpaceVMult + uvwStart[1],
      ] as T;
      const call: TextureCall<T> = {
        builtin: 'textureSample',
        coordType: 'f',
        coords,
        ddx: [ddx / textureSize.width, 0] as T,
        ddy: [0, ddy / textureSize.height] as T,
        offset: options.offset as T,
      };
      const sample = softwareTextureRead<T>(t, call, texture, sampler);
      const rgba = { R: 0, G: 0, B: 0, A: 1, ...sample };
      const asRgba32Float = new Float32Array(rep.pack(rgba));
      expData.set(asRgba32Float, (y * width + x) * 4);
    }
  }

  return TexelView.fromTextureDataByReference(format, new Uint8Array(expData.buffer), {
    bytesPerRow: width * 4 * 4,
    rowsPerImage: height,
    subrectOrigin: [0, 0, 0],
    subrectSize: targetSize,
  });
}

/**
 * Render textured quad to an rgba32float texture.
 */
export function drawTexture(
  t: GPUTest & TextureTestMixinType,
  texture: GPUTexture,
  samplerDesc: GPUSamplerDescriptor,
  options: TextureTestOptions
) {
  const device = t.device;
  const { ddx = 1, ddy = 1, uvwStart = [0, 0, 0], offset } = options;

  const format = 'rgba32float';
  const renderTarget = t.createTextureTracked({
    format,
    size: [32, 32],
    usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.RENDER_ATTACHMENT,
  });

  // Compute the amount we need to multiply the unitQuad by get the
  // derivatives we want.
  const uMult = (ddx * renderTarget.width) / texture.width;
  const vMult = (ddy * renderTarget.height) / texture.height;

  const offsetWGSL = offset ? `, vec2i(${offset[0]},${offset[1]})` : '';

  const code = `
struct InOut {
  @builtin(position) pos: vec4f,
  @location(0) uv: vec2f,
};

@vertex fn vs(@builtin(vertex_index) vertex_index : u32) -> InOut {
  let positions = array(
    vec2f(-1,  1), vec2f( 1,  1),
    vec2f(-1, -1), vec2f( 1, -1),
  );
  let pos = positions[vertex_index];
  return InOut(
    vec4f(pos, 0, 1),
    (pos * 0.5 + 0.5) * vec2f(${uMult}, ${vMult}) + vec2f(${uvwStart[0]}, ${uvwStart[1]}),
  );
}

@group(0) @binding(0) var          T    : texture_2d<f32>;
@group(0) @binding(1) var          S    : sampler;

@fragment fn fs(v: InOut) -> @location(0) vec4f {
  return textureSample(T, S, v.uv${offsetWGSL});
}
`;

  const shaderModule = device.createShaderModule({ code });

  const pipeline = device.createRenderPipeline({
    layout: 'auto',
    vertex: { module: shaderModule },
    fragment: {
      module: shaderModule,
      targets: [{ format }],
    },
    primitive: { topology: 'triangle-strip' },
  });

  const sampler = device.createSampler(samplerDesc);

  const bindGroup = device.createBindGroup({
    layout: pipeline.getBindGroupLayout(0),
    entries: [
      { binding: 0, resource: texture.createView() },
      { binding: 1, resource: sampler },
    ],
  });

  const encoder = device.createCommandEncoder();

  const renderPass = encoder.beginRenderPass({
    colorAttachments: [{ view: renderTarget.createView(), loadOp: 'clear', storeOp: 'store' }],
  });

  renderPass.setPipeline(pipeline);
  renderPass.setBindGroup(0, bindGroup);
  renderPass.draw(4);
  renderPass.end();
  device.queue.submit([encoder.finish()]);

  return renderTarget;
}

function getMaxFractionalDiffForTextureFormat(format: GPUTextureFormat) {
  // Note: I'm not sure what we should do here. My assumption is, given texels
  // have random values, the difference between 2 texels can be very large. In
  // the current version, for a float texture they can be +/- 1000 difference.
  // Sampling is very GPU dependent. So if one pixel gets a random value of
  // -1000 and the neighboring pixel gets +1000 then any slight variation in how
  // sampling is applied will generate a large difference when interpolating
  // between -1000 and +1000.
  //
  // We could make some entry for every format but for now I just put the
  // tolerances here based on format texture suffix.
  //
  // It's possible the math in the software rasterizer is just bad but the
  // results certainly seem close.
  //
  // These tolerances started from the OpenGL ES dEQP tests.
  // Those tests always render to an rgba8unorm texture. The shaders do effectively
  //
  //   result = textureSample(...) * scale + bias
  //
  // to get the results in a 0.0 to 1.0 range. After reading the values back they
  // expand them to their original ranges with
  //
  //   value = (result - bias) / scale;
  //
  // Tolerances from dEQP
  // --------------------
  // 8unorm: 3.9 / 255
  // 8snorm: 7.9 / 128
  // 2unorm: 7.9 / 512
  // ufloat: 156.249
  //  float: 31.2498
  //
  // The numbers below have been set empirically to get the tests to pass on all
  // devices. The devices with the most divergence from the calculated expected
  // values are MacOS Intel and AMD.
  //
  // MAINTENANCE_TODO: Double check the software rendering math and lower these
  // tolerances if possible.

  if (format.includes('depth')) {
    return 3 / 65536;
  } else if (format.includes('8unorm')) {
    return 7 / 255;
  } else if (format.includes('2unorm')) {
    return 9 / 512;
  } else if (format.includes('unorm')) {
    return 7 / 255;
  } else if (format.includes('8snorm')) {
    return 7.9 / 128;
  } else if (format.includes('snorm')) {
    return 7.9 / 128;
  } else if (format.endsWith('ufloat')) {
    return 156.249;
  } else if (format.endsWith('float')) {
    return 44;
  } else {
    // It's likely an integer format. In any case, zero tolerance is passable.
    return 0;
  }
}

export function checkTextureMatchesExpectedTexelView(
  t: GPUTest & TextureTestMixinType,
  format: GPUTextureFormat,
  actualTexture: GPUTexture,
  expectedTexelView: TexelView
) {
  const maxFractionalDiff = getMaxFractionalDiffForTextureFormat(format);
  t.expectTexelViewComparisonIsOkInTexture(
    { texture: actualTexture },
    expectedTexelView,
    [actualTexture.width, actualTexture.height],
    { maxFractionalDiff }
  );
}

/**
 * Puts data in a texture. Renders a quad to a rgba32float. Then "software renders"
 * to a TexelView the expected result and compares the rendered texture to the
 * expected TexelView.
 */
export async function putDataInTextureThenDrawAndCheckResultsComparedToSoftwareRasterizer<
  T extends Dimensionality,
>(
  t: GPUTest & TextureTestMixinType,
  descriptor: GPUTextureDescriptor,
  viewDescriptor: GPUTextureViewDescriptor,
  samplerDesc: GPUSamplerDescriptor,
  options: TextureTestOptions
) {
  const { texture, texels } = await createTextureWithRandomDataAndGetTexels(t, descriptor);

  const actualTexture = drawTexture(t, texture, samplerDesc, options);
  const expectedTexelView = softwareRasterize<T>(
    t,
    { descriptor, texels, viewDescriptor },
    samplerDesc,
    [actualTexture.width, actualTexture.height],
    options
  );

  checkTextureMatchesExpectedTexelView(t, texture.format, actualTexture, expectedTexelView);
}

const sumOfCharCodesOfString = (s: unknown) =>
  String(s)
    .split('')
    .reduce((sum, c) => sum + c.charCodeAt(0), 0);

/**
 * Makes a function that fills a block portion of a Uint8Array with random valid data
 * for an astc block.
 *
 * The astc format is fairly complicated. For now we do the simplest thing.
 * which is to set the block as a "void-extent" block (a solid color).
 * This makes our test have far less precision.
 *
 * MAINTENANCE_TODO: generate other types of astc blocks. One option would
 * be to randomly select from set of pre-made blocks.
 *
 * See Spec:
 * https://registry.khronos.org/OpenGL/extensions/KHR/KHR_texture_compression_astc_hdr.txt
 */
function makeAstcBlockFiller(format: GPUTextureFormat) {
  const info = kTextureFormatInfo[format];
  const bytesPerBlock = info.color!.bytes;
  return (data: Uint8Array, offset: number, hashBase: number) => {
    // set the block to be a void-extent block
    data.set(
      [
        0b1111_1100, // 0
        0b1111_1101, // 1
        0b1111_1111, // 2
        0b1111_1111, // 3
        0b1111_1111, // 4
        0b1111_1111, // 5
        0b1111_1111, // 6
        0b1111_1111, // 7
      ],
      offset
    );
    // fill the rest of the block with random data
    const end = offset + bytesPerBlock;
    for (let i = offset + 8; i < end; ++i) {
      data[i] = hashU32(hashBase, i);
    }
  };
}

/**
 * Makes a function that fills a block portion of a Uint8Array with random bytes.
 */
function makeRandomBytesBlockFiller(format: GPUTextureFormat) {
  const info = kTextureFormatInfo[format];
  const bytesPerBlock = info.color!.bytes;
  return (data: Uint8Array, offset: number, hashBase: number) => {
    const end = offset + bytesPerBlock;
    for (let i = offset; i < end; ++i) {
      data[i] = hashU32(hashBase, i);
    }
  };
}

function getBlockFiller(format: GPUTextureFormat) {
  if (format.startsWith('astc')) {
    return makeAstcBlockFiller(format);
  } else {
    return makeRandomBytesBlockFiller(format);
  }
}

/**
 * Fills a texture with random data.
 */
export function fillTextureWithRandomData(device: GPUDevice, texture: GPUTexture) {
  assert(!isCompressedFloatTextureFormat(texture.format));
  const info = kTextureFormatInfo[texture.format];
  const hashBase =
    sumOfCharCodesOfString(texture.format) +
    sumOfCharCodesOfString(texture.dimension) +
    texture.width +
    texture.height +
    texture.depthOrArrayLayers +
    texture.mipLevelCount;
  const bytesPerBlock = info.color!.bytes;
  const fillBlock = getBlockFiller(texture.format);
  for (let mipLevel = 0; mipLevel < texture.mipLevelCount; ++mipLevel) {
    const size = physicalMipSizeFromTexture(texture, mipLevel);
    const blocksAcross = Math.ceil(size[0] / info.blockWidth);
    const blocksDown = Math.ceil(size[1] / info.blockHeight);
    const bytesPerRow = blocksAcross * bytesPerBlock;
    const bytesNeeded = bytesPerRow * blocksDown * size[2];
    const data = new Uint8Array(bytesNeeded);
    for (let offset = 0; offset < bytesNeeded; offset += bytesPerBlock) {
      fillBlock(data, offset, hashBase);
    }
    device.queue.writeTexture(
      { texture, mipLevel },
      data,
      { bytesPerRow, rowsPerImage: blocksDown },
      size
    );
  }
}

const s_readTextureToRGBA32DeviceToPipeline = new WeakMap<
  GPUDevice,
  Map<string, GPUComputePipeline>
>();

// MAINTENANCE_TODO: remove cast once textureBindingViewDimension is added to IDL
function getEffectiveViewDimension(
  t: GPUTest,
  descriptor: GPUTextureDescriptor
): GPUTextureViewDimension {
  const { textureBindingViewDimension } = descriptor as unknown as {
    textureBindingViewDimension?: GPUTextureViewDimension;
  };
  const size = reifyExtent3D(descriptor.size);
  return effectiveViewDimensionForDimension(
    textureBindingViewDimension,
    descriptor.dimension,
    size.depthOrArrayLayers
  );
}

export async function readTextureToTexelViews(
  t: GPUTest,
  texture: GPUTexture,
  descriptor: GPUTextureDescriptor,
  format: EncodableTextureFormat
) {
  const device = t.device;
  const viewDimensionToPipelineMap =
    s_readTextureToRGBA32DeviceToPipeline.get(device) ??
    new Map<GPUTextureViewDimension, GPUComputePipeline>();
  s_readTextureToRGBA32DeviceToPipeline.set(device, viewDimensionToPipelineMap);

  const viewDimension = getEffectiveViewDimension(t, descriptor);
  const id = `${viewDimension}:${texture.sampleCount}`;
  let pipeline = viewDimensionToPipelineMap.get(id);
  if (!pipeline) {
    let textureWGSL;
    let loadWGSL;
    let dimensionWGSL = 'textureDimensions(tex, uni.mipLevel)';
    switch (viewDimension) {
      case '2d':
        if (texture.sampleCount > 1) {
          textureWGSL = 'texture_multisampled_2d<f32>';
          loadWGSL = 'textureLoad(tex, coord.xy, sampleIndex)';
          dimensionWGSL = 'textureDimensions(tex)';
        } else {
          textureWGSL = 'texture_2d<f32>';
          loadWGSL = 'textureLoad(tex, coord.xy, mipLevel)';
        }
        break;
      case 'cube-array': // cube-array doesn't exist in compat so we can just use 2d_array for this
      case '2d-array':
        textureWGSL = 'texture_2d_array<f32>';
        loadWGSL = `
          textureLoad(
              tex,
              coord.xy,
              coord.z,
              mipLevel)`;
        break;
      case '3d':
        textureWGSL = 'texture_3d<f32>';
        loadWGSL = 'textureLoad(tex, coord.xyz, mipLevel)';
        break;
      case 'cube':
        textureWGSL = 'texture_cube<f32>';
        loadWGSL = `
          textureLoadCubeAs2DArray(tex, coord.xy, coord.z, mipLevel);
        `;
        break;
      default:
        unreachable(`unsupported view: ${viewDimension}`);
    }
    const module = device.createShaderModule({
      code: `
        const faceMat = array(
          mat3x3f( 0,  0,  -2,  0, -2,   0,  1,  1,   1),   // pos-x
          mat3x3f( 0,  0,   2,  0, -2,   0, -1,  1,  -1),   // neg-x
          mat3x3f( 2,  0,   0,  0,  0,   2, -1,  1,  -1),   // pos-y
          mat3x3f( 2,  0,   0,  0,  0,  -2, -1, -1,   1),   // neg-y
          mat3x3f( 2,  0,   0,  0, -2,   0, -1,  1,   1),   // pos-z
          mat3x3f(-2,  0,   0,  0, -2,   0,  1,  1,  -1));  // neg-z

        // needed for compat mode.
        fn textureLoadCubeAs2DArray(tex: texture_cube<f32>, coord: vec2u, layer: u32, mipLevel: u32) -> vec4f {
          // convert texel coord normalized coord
          let size = textureDimensions(tex, mipLevel);
          let uv = (vec2f(coord) + 0.5) / vec2f(size.xy);

          // convert uv + layer into cube coord
          let cubeCoord = faceMat[layer] * vec3f(uv, 1.0);

          return textureSampleLevel(tex, smp, cubeCoord, f32(mipLevel));
        }

        struct Uniforms {
          mipLevel: u32,
          sampleCount: u32,
        };

        @group(0) @binding(0) var<uniform> uni: Uniforms;
        @group(0) @binding(1) var tex: ${textureWGSL};
        @group(0) @binding(2) var smp: sampler;
        @group(0) @binding(3) var<storage, read_write> data: array<vec4f>;

        @compute @workgroup_size(1) fn cs(
          @builtin(global_invocation_id) global_invocation_id : vec3<u32>) {
          _ = smp;
          let size = ${dimensionWGSL};
          let ndx = global_invocation_id.z * size.x * size.y * uni.sampleCount +
                    global_invocation_id.y * size.x * uni.sampleCount +
                    global_invocation_id.x;
          let coord = vec3u(global_invocation_id.x / uni.sampleCount, global_invocation_id.yz);
          let sampleIndex = global_invocation_id.x % uni.sampleCount;
          let mipLevel = uni.mipLevel;
          data[ndx] = ${loadWGSL};
        }
      `,
    });
    pipeline = device.createComputePipeline({ layout: 'auto', compute: { module } });
    viewDimensionToPipelineMap.set(id, pipeline);
  }

  const encoder = device.createCommandEncoder();

  const readBuffers = [];
  for (let mipLevel = 0; mipLevel < texture.mipLevelCount; ++mipLevel) {
    const size = virtualMipSize(texture.dimension, texture, mipLevel);

    const uniformValues = new Uint32Array([mipLevel, texture.sampleCount, 0, 0]); // min size is 16 bytes
    const uniformBuffer = t.createBufferTracked({
      size: uniformValues.byteLength,
      usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
    });
    device.queue.writeBuffer(uniformBuffer, 0, uniformValues);

    const storageBuffer = t.createBufferTracked({
      size: size[0] * size[1] * size[2] * 4 * 4 * texture.sampleCount, // rgba32float
      usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_SRC,
    });

    const readBuffer = t.createBufferTracked({
      size: storageBuffer.size,
      usage: GPUBufferUsage.MAP_READ | GPUBufferUsage.COPY_DST,
    });
    readBuffers.push({ size, readBuffer });

    const sampler = device.createSampler();

    const bindGroup = device.createBindGroup({
      layout: pipeline.getBindGroupLayout(0),
      entries: [
        { binding: 0, resource: { buffer: uniformBuffer } },
        { binding: 1, resource: texture.createView({ dimension: viewDimension }) },
        { binding: 2, resource: sampler },
        { binding: 3, resource: { buffer: storageBuffer } },
      ],
    });

    const pass = encoder.beginComputePass();
    pass.setPipeline(pipeline);
    pass.setBindGroup(0, bindGroup);
    pass.dispatchWorkgroups(size[0] * texture.sampleCount, size[1], size[2]);
    pass.end();
    encoder.copyBufferToBuffer(storageBuffer, 0, readBuffer, 0, readBuffer.size);
  }

  device.queue.submit([encoder.finish()]);

  const texelViews: TexelView[] = [];

  for (const { readBuffer, size } of readBuffers) {
    await readBuffer.mapAsync(GPUMapMode.READ);

    // need a copy of the data since unmapping will nullify the typedarray view.
    const data = new Float32Array(readBuffer.getMappedRange()).slice();
    readBuffer.unmap();

    const { sampleCount } = texture;
    texelViews.push(
      TexelView.fromTexelsAsColors(format, coord => {
        const offset =
          ((coord.z * size[0] * size[1] + coord.y * size[0] + coord.x) * sampleCount +
            (coord.sampleIndex ?? 0)) *
          4;
        return {
          R: data[offset + 0],
          G: data[offset + 1],
          B: data[offset + 2],
          A: data[offset + 3],
        };
      })
    );
  }

  return texelViews;
}

function createTextureFromTexelViewsLocal(
  t: GPUTest,
  texelViews: TexelView[],
  desc: Omit<GPUTextureDescriptor, 'format'>
): GPUTexture {
  const modifiedDescriptor = { ...desc };
  // If it's a depth or stencil texture we need to render to it to fill it with data.
  if (isDepthOrStencilTextureFormat(texelViews[0].format)) {
    modifiedDescriptor.usage = desc.usage | GPUTextureUsage.RENDER_ATTACHMENT;
  }
  return createTextureFromTexelViews(t, texelViews, modifiedDescriptor);
}

/**
 * Fills a texture with random data and returns that data as
 * an array of TexelView.
 *
 * For compressed textures the texture is filled with random bytes
 * and then read back from the GPU by sampling so the GPU decompressed
 * the texture.
 *
 * For uncompressed textures the TexelViews are generated and then
 * copied to the texture.
 */
export async function createTextureWithRandomDataAndGetTexels(
  t: GPUTest,
  descriptor: GPUTextureDescriptor
) {
  if (isCompressedTextureFormat(descriptor.format)) {
    const texture = t.createTextureTracked(descriptor);

    fillTextureWithRandomData(t.device, texture);
    const texels = await readTextureToTexelViews(
      t,
      texture,
      descriptor,
      getTexelViewFormatForTextureFormat(texture.format)
    );
    return { texture, texels };
  } else {
    const texels = createRandomTexelViewMipmap(descriptor);
    const texture = createTextureFromTexelViewsLocal(t, texels, descriptor);
    return { texture, texels };
  }
}

function valueIfAllComponentsAreEqual(
  c: PerTexelComponent<number>,
  componentOrder: readonly TexelComponent[]
) {
  const s = new Set(componentOrder.map(component => c[component]!));
  return s.size === 1 ? s.values().next().value : undefined;
}

/**
 * Creates a VideoFrame with random data and a TexelView with the same data.
 */
export function createVideoFrameWithRandomDataAndGetTexels(textureSize: GPUExtent3D) {
  const size = reifyExtent3D(textureSize);
  assert(size.depthOrArrayLayers === 1);

  // Fill ImageData with random values.
  const imageData = new ImageData(size.width, size.height);
  const data = imageData.data;
  const asU32 = new Uint32Array(data.buffer);
  for (let i = 0; i < asU32.length; ++i) {
    asU32[i] = hashU32(i);
  }

  // Put the ImageData into a canvas and make a VideoFrame
  const canvas = new OffscreenCanvas(size.width, size.height);
  const ctx = canvas.getContext('2d')!;
  ctx.putImageData(imageData, 0, 0);
  const videoFrame = new VideoFrame(canvas, { timestamp: 0 });

  // Premultiply the ImageData
  for (let i = 0; i < data.length; i += 4) {
    const alpha = data[i + 3] / 255;
    data[i + 0] = data[i + 0] * alpha;
    data[i + 1] = data[i + 1] * alpha;
    data[i + 2] = data[i + 2] * alpha;
  }

  // Create a TexelView from the premultiplied ImageData
  const texels = [
    TexelView.fromTextureDataByReference('rgba8unorm', data, {
      bytesPerRow: size.width * 4,
      rowsPerImage: size.height,
      subrectOrigin: [0, 0, 0],
      subrectSize: size,
    }),
  ];

  return { videoFrame, texels };
}

const kFaceNames = ['+x', '-x', '+y', '-y', '+z', '-z'] as const;

/**
 * Generates a text art grid showing which texels were sampled
 * followed by a list of the samples and the weights used for each
 * component.
 *
 * It works by making a set of indices for every texel in the texture.
 * It splits the set into 2. It picks one set and generates texture data
 * using TexelView.fromTexelsAsColor with [1, 1, 1, 1] texels for members
 * of the current set.
 *
 * In then calls 'run' which renders a single `call`. `run` uses either
 * the software renderer or WebGPU. It then checks the results. If the
 * result is zero, all texels in the current had no influence when sampling
 * and can be discarded.
 *
 * If the result is > 0 then, if the set has more than one member, the
 * set is split and added to the list to sets to test. If the set only
 * had one member then the result is the weight used when sampling that texel.
 *
 * This lets you see if the weights from the software renderer match the
 * weights from WebGPU.
 *
 * Example:
 *
 *     0   1   2   3   4   5   6   7
 *   ┌───┬───┬───┬───┬───┬───┬───┬───┐
 * 0 │   │   │   │   │   │   │   │   │
 *   ├───┼───┼───┼───┼───┼───┼───┼───┤
 * 1 │   │   │   │   │   │   │   │ a │
 *   ├───┼───┼───┼───┼───┼───┼───┼───┤
 * 2 │   │   │   │   │   │   │   │ b │
 *   ├───┼───┼───┼───┼───┼───┼───┼───┤
 * 3 │   │   │   │   │   │   │   │   │
 *   ├───┼───┼───┼───┼───┼───┼───┼───┤
 * 4 │   │   │   │   │   │   │   │   │
 *   ├───┼───┼───┼───┼───┼───┼───┼───┤
 * 5 │   │   │   │   │   │   │   │   │
 *   ├───┼───┼───┼───┼───┼───┼───┼───┤
 * 6 │   │   │   │   │   │   │   │   │
 *   ├───┼───┼───┼───┼───┼───┼───┼───┤
 * 7 │   │   │   │   │   │   │   │   │
 *   └───┴───┴───┴───┴───┴───┴───┴───┘
 * a: at: [7, 1], weights: [R: 0.75000]
 * b: at: [7, 2], weights: [R: 0.25000]
 */
async function identifySamplePoints<T extends Dimensionality>(
  texture: Texture,
  call: TextureCall<T>,
  run: (texels: TexelView[]) => Promise<PerTexelComponent<number>>
) {
  const info = texture.descriptor;
  const isCube = isCubeViewDimension(texture.viewDescriptor);
  const mipLevelCount = texture.descriptor.mipLevelCount ?? 1;
  const mipLevelSize = range(mipLevelCount, mipLevel =>
    virtualMipSize(texture.descriptor.dimension ?? '2d', texture.descriptor.size, mipLevel)
  );
  const numTexelsPerLevel = mipLevelSize.map(size => size.reduce((s, v) => s * v));
  const numTexelsOfPrecedingLevels = (() => {
    let total = 0;
    return numTexelsPerLevel.map(v => {
      const num = total;
      total += v;
      return num;
    });
  })();
  const numTexels = numTexelsPerLevel.reduce((sum, v) => sum + v);

  // This isn't perfect. We already know there was an error. We're just
  // generating info so it seems okay it's not perfect. This format will
  // be used to generate weights by drawing with a texture of this format
  // with a specific pixel set to [1, 1, 1, 1]. As such, if the result
  // is > 0 then that pixel was sampled and the results are the weights.
  //
  // Ideally, this texture with a single pixel set to [1, 1, 1, 1] would
  // be the same format we were originally testing, the one we already
  // detected an error for. This way, whatever subtle issues there are
  // from that format will affect the weight values we're computing. But,
  // if that format is not encodable, for example if it's a compressed
  // texture format, then we have no way to build a texture so we use
  // rgba8unorm instead.
  const format = (
    kEncodableTextureFormats.includes(info.format as EncodableTextureFormat)
      ? info.format
      : 'rgba8unorm'
  ) as EncodableTextureFormat;
  const rep = kTexelRepresentationInfo[format];

  const components = isBuiltinGather(call.builtin) ? kRGBAComponents : rep.componentOrder;
  const convertResultAsAppropriate = isBuiltinGather(call.builtin)
    ? <T>(v: T) => v
    : convertResultFormatToTexelViewFormat;

  // Identify all the texels that are sampled, and their weights.
  const sampledTexelWeights = new Map<number, PerTexelComponent<number>>();
  const unclassifiedStack = [new Set<number>(range(numTexels, v => v))];
  while (unclassifiedStack.length > 0) {
    // Pop the an unclassified texels stack
    const unclassified = unclassifiedStack.pop()!;

    // Split unclassified texels evenly into two new sets
    const setA = new Set<number>();
    const setB = new Set<number>();
    [...unclassified.keys()].forEach((t, i) => ((i & 1) === 0 ? setA : setB).add(t));

    // Push setB to the unclassified texels stack
    if (setB.size > 0) {
      unclassifiedStack.push(setB);
    }

    // See if any of the texels in setA were sampled.0
    const results = convertResultAsAppropriate(
      await run(
        range(mipLevelCount, mipLevel =>
          TexelView.fromTexelsAsColors(
            format,
            (coords: Required<GPUOrigin3DDict>): Readonly<PerTexelComponent<number>> => {
              const size = mipLevelSize[mipLevel];
              const texelsPerSlice = size[0] * size[1];
              const texelsPerRow = size[0];
              const texelId =
                numTexelsOfPrecedingLevels[mipLevel] +
                coords.x +
                coords.y * texelsPerRow +
                coords.z * texelsPerSlice;
              const isCandidate = setA.has(texelId);
              const texel: PerTexelComponent<number> = {};
              for (const component of rep.componentOrder) {
                texel[component] = isCandidate ? 1 : 0;
              }
              return texel;
            }
          )
        )
      ),
      format
    );
    if (components.some(c => results[c] !== 0)) {
      // One or more texels of setA were sampled.
      if (setA.size === 1) {
        // We identified a specific texel was sampled.
        // As there was only one texel in the set, results holds the sampling weights.
        setA.forEach(texel => sampledTexelWeights.set(texel, results));
      } else {
        // More than one texel in the set. Needs splitting.
        unclassifiedStack.push(setA);
      }
    }
  }

  const getMipLevelFromTexelId = (texelId: number) => {
    for (let mipLevel = mipLevelCount - 1; mipLevel > 0; --mipLevel) {
      if (texelId - numTexelsOfPrecedingLevels[mipLevel] >= 0) {
        return mipLevel;
      }
    }
    return 0;
  };

  // separate the sampledTexelWeights by mipLevel, then by layer, within a layer the texelId only includes x and y
  const levels: Map<number, PerTexelComponent<number>>[][] = [];
  for (const [texelId, weight] of sampledTexelWeights.entries()) {
    const mipLevel = getMipLevelFromTexelId(texelId);
    const level = levels[mipLevel] ?? [];
    levels[mipLevel] = level;
    const size = mipLevelSize[mipLevel];
    const texelsPerSlice = size[0] * size[1];
    const id = texelId - numTexelsOfPrecedingLevels[mipLevel];
    const layer = Math.floor(id / texelsPerSlice);
    const layerEntries = level[layer] ?? new Map();
    level[layer] = layerEntries;
    const xyId = id - layer * texelsPerSlice;
    layerEntries.set(xyId, weight);
  }

  // ┌───┬───┬───┬───┐
  // │ a │   │   │   │
  // ├───┼───┼───┼───┤
  // │   │   │   │   │
  // ├───┼───┼───┼───┤
  // │   │   │   │   │
  // ├───┼───┼───┼───┤
  // │   │   │   │ b │
  // └───┴───┴───┴───┘
  const lines: string[] = [];
  const letter = (idx: number) => String.fromCodePoint(idx < 30 ? 97 + idx : idx + 9600 - 30); // 97: 'a'
  let idCount = 0;

  for (let mipLevel = 0; mipLevel < mipLevelCount; ++mipLevel) {
    const level = levels[mipLevel];
    if (!level) {
      continue;
    }

    const [width, height, depthOrArrayLayers] = mipLevelSize[mipLevel];
    const texelsPerRow = width;

    for (let layer = 0; layer < depthOrArrayLayers; ++layer) {
      const layerEntries = level[layer];

      const orderedTexelIndices: number[] = [];
      lines.push('');
      const unSampled = layerEntries ? '' : 'un-sampled';
      if (isCube) {
        const face = kFaceNames[layer % 6];
        lines.push(`layer: ${layer}, cube-layer: ${(layer / 6) | 0} (${face}) ${unSampled}`);
      } else {
        lines.push(`layer: ${unSampled}`);
      }

      if (!layerEntries) {
        continue;
      }

      {
        let line = '  ';
        for (let x = 0; x < width; x++) {
          line += `  ${x.toString().padEnd(2)}`;
        }
        lines.push(line);
      }
      {
        let line = '  ┌';
        for (let x = 0; x < width; x++) {
          line += x === width - 1 ? '───┐' : '───┬';
        }
        lines.push(line);
      }
      for (let y = 0; y < height; y++) {
        {
          let line = `${y.toString().padEnd(2)}│`;
          for (let x = 0; x < width; x++) {
            const texelIdx = x + y * texelsPerRow;
            const weight = layerEntries.get(texelIdx);
            if (weight !== undefined) {
              line += ` ${letter(idCount + orderedTexelIndices.length)} │`;
              orderedTexelIndices.push(texelIdx);
            } else {
              line += '   │';
            }
          }
          lines.push(line);
        }
        if (y < height - 1) {
          let line = '  ├';
          for (let x = 0; x < width; x++) {
            line += x === width - 1 ? '───┤' : '───┼';
          }
          lines.push(line);
        }
      }
      {
        let line = '  └';
        for (let x = 0; x < width; x++) {
          line += x === width - 1 ? '───┘' : '───┴';
        }
        lines.push(line);
      }

      const pad2 = (n: number) => n.toString().padStart(2);
      const fix5 = (n: number) => n.toFixed(5);
      orderedTexelIndices.forEach((texelIdx, i) => {
        const weights = layerEntries.get(texelIdx)!;
        const y = Math.floor(texelIdx / texelsPerRow);
        const x = texelIdx % texelsPerRow;
        const singleWeight = valueIfAllComponentsAreEqual(weights, components);
        const w =
          singleWeight !== undefined
            ? `weight: ${fix5(singleWeight)}`
            : `weights: [${components.map(c => `${c}: ${fix5(weights[c]!)}`).join(', ')}]`;
        const coord = `${pad2(x)}, ${pad2(y)}, ${pad2(layer)}`;
        lines.push(`${letter(idCount + i)}: mip(${mipLevel}) at: [${coord}], ${w}`);
      });
      idCount += orderedTexelIndices.length;
    }
  }

  return lines;
}

function layoutTwoColumns(columnA: string[], columnB: string[]) {
  const widthA = Math.max(...columnA.map(l => l.length));
  const lines = Math.max(columnA.length, columnB.length);
  const out: string[] = new Array<string>(lines);
  for (let line = 0; line < lines; line++) {
    const a = columnA[line] ?? '';
    const b = columnB[line] ?? '';
    out[line] = `${a}${' '.repeat(widthA - a.length)} | ${b}`;
  }
  return out;
}

/**
 * Returns the number of layers ot test for a given view dimension
 */
export function getDepthOrArrayLayersForViewDimension(viewDimension?: GPUTextureViewDimension) {
  switch (viewDimension) {
    case undefined:
    case '2d':
      return 1;
    case '3d':
      return 8;
    case 'cube':
      return 6;
    default:
      unreachable();
  }
}

/**
 * Choose a texture size based on the given parameters.
 * The size will be in a multiple of blocks. If it's a cube
 * map the size will so be square.
 */
export function chooseTextureSize({
  minSize,
  minBlocks,
  format,
  viewDimension,
}: {
  minSize: number;
  minBlocks: number;
  format: GPUTextureFormat;
  viewDimension?: GPUTextureViewDimension;
}) {
  const { blockWidth, blockHeight } = kTextureFormatInfo[format];
  const width = align(Math.max(minSize, blockWidth * minBlocks), blockWidth);
  const height = align(Math.max(minSize, blockHeight * minBlocks), blockHeight);
  if (viewDimension === 'cube' || viewDimension === 'cube-array') {
    const blockLCM = lcm(blockWidth, blockHeight);
    const largest = Math.max(width, height);
    const size = align(largest, blockLCM);
    return [size, size, viewDimension === 'cube-array' ? 24 : 6];
  }
  const depthOrArrayLayers = getDepthOrArrayLayersForViewDimension(viewDimension);
  return [width, height, depthOrArrayLayers];
}

export const kSamplePointMethods = ['texel-centre', 'spiral'] as const;
export type SamplePointMethods = (typeof kSamplePointMethods)[number];

export const kCubeSamplePointMethods = ['cube-edges', 'texel-centre', 'spiral'] as const;
export type CubeSamplePointMethods = (typeof kSamplePointMethods)[number];

type TextureBuiltinInputArgs = {
  textureBuiltin?: TextureBuiltin;
  descriptor: GPUTextureDescriptor;
  sampler?: GPUSamplerDescriptor;
  mipLevel?: RangeDef;
  sampleIndex?: RangeDef;
  arrayIndex?: RangeDef;
  component?: boolean;
  depthRef?: boolean;
  offset?: boolean;
  hashInputs: (number | string | boolean)[];
};

/**
 * Generates an array of coordinates at which to sample a texture.
 */
function generateTextureBuiltinInputsImpl<T extends Dimensionality>(
  makeValue: (x: number, y: number, z: number) => T,
  n: number,
  args:
    | (TextureBuiltinInputArgs & {
        method: 'texel-centre';
      })
    | (TextureBuiltinInputArgs & {
        method: 'spiral';
        radius?: number;
        loops?: number;
      })
): {
  coords: T;
  mipLevel: number;
  sampleIndex?: number;
  arrayIndex?: number;
  offset?: T;
  component?: number;
  depthRef?: number;
}[] {
  const { method, descriptor } = args;
  const dimension = descriptor.dimension ?? '2d';
  const mipLevelCount = descriptor.mipLevelCount ?? 1;
  const size = virtualMipSize(dimension, descriptor.size, 0);
  const coords: T[] = [];
  switch (method) {
    case 'texel-centre': {
      for (let i = 0; i < n; i++) {
        const r = hashU32(i);
        const x = Math.floor(lerp(0, size[0] - 1, (r & 0xff) / 0xff)) + 0.5;
        const y = Math.floor(lerp(0, size[1] - 1, ((r >> 8) & 0xff) / 0xff)) + 0.5;
        const z = Math.floor(lerp(0, size[2] - 1, ((r >> 16) & 0xff) / 0xff)) + 0.5;
        coords.push(makeValue(x / size[0], y / size[1], z / size[2]));
      }
      break;
    }
    case 'spiral': {
      const { radius = 1.5, loops = 2 } = args;
      for (let i = 0; i < n; i++) {
        const f = i / (Math.max(n, 2) - 1);
        const r = radius * f;
        const a = loops * 2 * Math.PI * f;
        coords.push(makeValue(0.5 + r * Math.cos(a), 0.5 + r * Math.sin(a), 0));
      }
      break;
    }
  }

  const _hashInputs = args.hashInputs.map(v =>
    typeof v === 'string' ? sumOfCharCodesOfString(v) : typeof v === 'boolean' ? (v ? 1 : 0) : v
  );
  const makeRangeValue = ({ num, type }: RangeDef, ...hashInputs: number[]) => {
    const range = num + (type === 'u32' ? 1 : 2);
    const number =
      (hashU32(..._hashInputs, ...hashInputs) / 0x1_0000_0000) * range - (type === 'u32' ? 0 : 1);
    return type === 'f32' ? number : Math.floor(number);
  };
  // Generates the same values per coord instead of using all the extra `_hashInputs`.
  const makeIntHashValueRepeatable = (min: number, max: number, ...hashInputs: number[]) => {
    const range = max - min;
    return min + Math.floor((hashU32(...hashInputs) / 0x1_0000_0000) * range);
  };

  // Samplers across devices use different methods to interpolate.
  // Quantizing the texture coordinates seems to hit coords that produce
  // comparable results to our computed results.
  // Note: This value works with 8x8 textures. Other sizes have not been tested.
  // Values that worked for reference:
  // Win 11, NVidia 2070 Super: 16
  // Linux, AMD Radeon Pro WX 3200: 256
  // MacOS, M1 Mac: 256
  const kSubdivisionsPerTexel = 4;

  // When filtering is nearest then we want to avoid edges of texels
  //
  //             U
  //             |
  //     +---+---+---+---+---+---+---+---+
  //     |   | A | B |   |   |   |   |   |
  //     +---+---+---+---+---+---+---+---+
  //
  // Above, coordinate U could sample either A or B
  //
  //               U
  //               |
  //     +---+---+---+---+---+---+---+---+
  //     |   | A | B | C |   |   |   |   |
  //     +---+---+---+---+---+---+---+---+
  //
  // For textureGather we want to avoid texel centers
  // as for coordinate U could either gather A,B or B,C.

  const avoidEdgeCase =
    !args.sampler || args.sampler.minFilter === 'nearest' || isBuiltinGather(args.textureBuiltin);
  const edgeRemainder = isBuiltinGather(args.textureBuiltin) ? kSubdivisionsPerTexel / 2 : 0;

  // textureGather issues for 2d/3d textures
  //
  // If addressModeU is repeat, then on an 8x1 texture, u = 0.01 or u = 0.99
  // would gather these texels
  //
  //     +---+---+---+---+---+---+---+---+
  //     | * |   |   |   |   |   |   | * |
  //     +---+---+---+---+---+---+---+---+
  //
  // If addressModeU is clamp-to-edge or mirror-repeat,
  // then on an 8x1 texture, u = 0.01 would gather this texel
  //
  //     +---+---+---+---+---+---+---+---+
  //     | * |   |   |   |   |   |   |   |
  //     +---+---+---+---+---+---+---+---+
  //
  // and 0.99 would gather this texel
  //
  //     +---+---+---+---+---+---+---+---+
  //     |   |   |   |   |   |   |   | * |
  //     +---+---+---+---+---+---+---+---+
  //
  // This means we have to if addressMode is not `repeat`, we
  // need to avoid the edge of the texture.
  //
  // Note: we don't have these specific issues with cube maps
  // as they ignore addressMode
  const euclideanModulo = (n: number, m: number) => ((n % m) + m) % m;
  const addressMode: GPUAddressMode[] =
    args.textureBuiltin === 'textureSampleBaseClampToEdge'
      ? ['clamp-to-edge', 'clamp-to-edge', 'clamp-to-edge']
      : [
          args.sampler?.addressModeU ?? 'clamp-to-edge',
          args.sampler?.addressModeV ?? 'clamp-to-edge',
          args.sampler?.addressModeW ?? 'clamp-to-edge',
        ];
  const avoidTextureEdge = (axis: number, textureDimensionUnits: number, v: number) => {
    assert(isBuiltinGather(args.textureBuiltin));
    if (addressMode[axis] === 'repeat') {
      return v;
    }
    const inside = euclideanModulo(v, textureDimensionUnits);
    const outside = v - inside;
    return outside + clamp(inside, { min: 1, max: textureDimensionUnits - 1 });
  };

  const numComponents = isDepthOrStencilTextureFormat(descriptor.format) ? 1 : 4;
  return coords.map((c, i) => {
    const mipLevel = args.mipLevel
      ? quantizeMipLevel(makeRangeValue(args.mipLevel, i), args.sampler?.mipmapFilter ?? 'nearest')
      : 0;
    const clampedMipLevel = clamp(mipLevel, { min: 0, max: mipLevelCount - 1 });
    const mipSize = virtualMipSize(dimension, size, clampedMipLevel);
    const q = mipSize.map(v => v * kSubdivisionsPerTexel);

    const coords = c.map((v, i) => {
      // Quantize to kSubdivisionsPerPixel
      const v1 = Math.floor(v * q[i]);
      // If it's nearest or textureGather and we're on the edge of a texel then move us off the edge
      // since the edge could choose one texel or another.
      const isTexelEdgeCase = Math.abs(v1 % kSubdivisionsPerTexel) === edgeRemainder;
      const v2 = isTexelEdgeCase && avoidEdgeCase ? v1 + 1 : v1;
      const v3 = isBuiltinGather(args.textureBuiltin) ? avoidTextureEdge(i, q[i], v2) : v2;
      // Convert back to texture coords
      return v3 / q[i];
    }) as T;

    return {
      coords,
      mipLevel,
      sampleIndex: args.sampleIndex ? makeRangeValue(args.sampleIndex, i, 1) : undefined,
      arrayIndex: args.arrayIndex ? makeRangeValue(args.arrayIndex, i, 2) : undefined,
      depthRef: args.depthRef ? makeRangeValue({ num: 1, type: 'f32' }, i, 5) : undefined,
      offset: args.offset
        ? (coords.map((_, j) => makeIntHashValueRepeatable(-8, 8, i, 3 + j)) as T)
        : undefined,
      component: args.component ? makeIntHashValueRepeatable(0, numComponents, i, 4) : undefined,
    };
  });
}

/**
 * When mipmapFilter === 'nearest' we need to stay away from 0.5
 * because the GPU could decide to choose one mip or the other.
 *
 * Some example transition values, the value at which the GPU chooses
 * mip level 1 over mip level 0:
 *
 * M1 Mac: 0.515381
 * Intel Mac: 0.49999
 * AMD Mac: 0.5
 */
const kMipEpsilon = 0.02;
function quantizeMipLevel(mipLevel: number, mipmapFilter: GPUMipmapFilterMode) {
  if (mipmapFilter === 'linear') {
    return mipLevel;
  }
  const intMip = Math.floor(mipLevel);
  const fractionalMip = mipLevel - intMip;
  if (fractionalMip < 0.5 - kMipEpsilon || fractionalMip > 0.5 + kMipEpsilon) {
    return mipLevel;
  } else {
    return intMip + 0.5 + (fractionalMip < 0.5 ? -kMipEpsilon : +kMipEpsilon);
  }
}

// Removes the first element from an array of types
type FilterFirstElement<T extends unknown[]> = T extends [unknown, ...infer R] ? R : [];

type GenerateTextureBuiltinInputsImplArgs = FilterFirstElement<
  Parameters<typeof generateTextureBuiltinInputsImpl>
>;

export function generateTextureBuiltinInputs1D(...args: GenerateTextureBuiltinInputsImplArgs) {
  return generateTextureBuiltinInputsImpl<vec1>((x: number) => [x], ...args);
}

export function generateTextureBuiltinInputs2D(...args: GenerateTextureBuiltinInputsImplArgs) {
  return generateTextureBuiltinInputsImpl<vec2>((x: number, y: number) => [x, y], ...args);
}

export function generateTextureBuiltinInputs3D(...args: GenerateTextureBuiltinInputsImplArgs) {
  return generateTextureBuiltinInputsImpl<vec3>(
    (x: number, y: number, z: number) => [x, y, z],
    ...args
  );
}

type mat3 =
  /* prettier-ignore */ [
  number, number, number,
  number, number, number,
  number, number, number,
];

const kFaceUVMatrices: mat3[] =
  /* prettier-ignore */ [
  [ 0,  0,  -2,  0, -2,   0,  1,  1,   1],   // pos-x
  [ 0,  0,   2,  0, -2,   0, -1,  1,  -1],   // neg-x
  [ 2,  0,   0,  0,  0,   2, -1,  1,  -1],   // pos-y
  [ 2,  0,   0,  0,  0,  -2, -1, -1,   1],   // neg-y
  [ 2,  0,   0,  0, -2,   0, -1,  1,   1],   // pos-z
  [-2,  0,   0,  0, -2,   0,  1,  1,  -1],   // neg-z
];

/** multiply a vec3 by mat3 */
function transformMat3(v: vec3, m: mat3): vec3 {
  const x = v[0];
  const y = v[1];
  const z = v[2];

  return [
    x * m[0] + y * m[3] + z * m[6],
    x * m[1] + y * m[4] + z * m[7],
    x * m[2] + y * m[5] + z * m[8],
  ];
}

/** normalize a vec3 */
function normalize(v: vec3): vec3 {
  const length = Math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  assert(length > 0);
  return v.map(v => v / length) as vec3;
}

/**
 * Converts a cube map coordinate to a uv coordinate (0 to 1) and layer (0.5/6.0 to 5.5/6.0).
 */
export function convertCubeCoordToNormalized3DTextureCoord(v: vec3): vec3 {
  let uvw;
  let layer;
  // normalize the coord.
  // MAINTENANCE_TODO: handle(0, 0, 0)
  const r = normalize(v);
  const absR = r.map(v => Math.abs(v));
  if (absR[0] > absR[1] && absR[0] > absR[2]) {
    // x major
    const negX = r[0] < 0.0 ? 1 : 0;
    uvw = [negX ? r[2] : -r[2], -r[1], absR[0]];
    layer = negX;
  } else if (absR[1] > absR[2]) {
    // y major
    const negY = r[1] < 0.0 ? 1 : 0;
    uvw = [r[0], negY ? -r[2] : r[2], absR[1]];
    layer = 2 + negY;
  } else {
    // z major
    const negZ = r[2] < 0.0 ? 1 : 0;
    uvw = [negZ ? -r[0] : r[0], -r[1], absR[2]];
    layer = 4 + negZ;
  }
  return [(uvw[0] / uvw[2] + 1) * 0.5, (uvw[1] / uvw[2] + 1) * 0.5, (layer + 0.5) / 6];
}

/**
 * Convert a 3d texcoord into a cube map coordinate.
 */
export function convertNormalized3DTexCoordToCubeCoord(uvLayer: vec3) {
  const [u, v, faceLayer] = uvLayer;
  return normalize(transformMat3([u, v, 1], kFaceUVMatrices[Math.min(5, faceLayer * 6) | 0]));
}

/**
 * Wrap a texel based face coord across cube faces
 *
 * We have a face texture in texels coord where U/V choose a texel and W chooses the face.
 * If U/V are outside the size of the texture then, when normalized and converted
 * to a cube map coordinate, they'll end up pointing to a different face.
 *
 * addressMode is effectively ignored for cube
 *
 * By converting from a texel based coord to a normalized coord and then to a cube map coord,
 * if the texel was outside of the face, the cube map coord will end up pointing to a different
 * face. We then convert back cube coord -> normalized face coord -> texel based coord
 */
function wrapFaceCoordToCubeFaceAtEdgeBoundaries(textureSize: number, faceCoord: vec3) {
  // convert texel based face coord to normalized 2d-array coord
  const nc0: vec3 = [
    (faceCoord[0] + 0.5) / textureSize,
    (faceCoord[1] + 0.5) / textureSize,
    (faceCoord[2] + 0.5) / 6,
  ];
  const cc = convertNormalized3DTexCoordToCubeCoord(nc0);
  const nc1 = convertCubeCoordToNormalized3DTextureCoord(cc);
  // convert normalized 2d-array coord back texel based face coord
  const fc = [
    Math.floor(nc1[0] * textureSize),
    Math.floor(nc1[1] * textureSize),
    Math.floor(nc1[2] * 6),
  ];

  return fc;
}

function applyAddressModesToCoords(
  addressMode: GPUAddressMode[],
  textureSize: number[],
  coord: number[]
) {
  return coord.map((v, i) => {
    switch (addressMode[i]) {
      case 'clamp-to-edge':
        return clamp(v, { min: 0, max: textureSize[i] - 1 });
      case 'mirror-repeat': {
        const n = Math.floor(v / textureSize[i]);
        v = v - n * textureSize[i];
        return (n & 1) !== 0 ? textureSize[i] - v - 1 : v;
      }
      case 'repeat':
        return v - Math.floor(v / textureSize[i]) * textureSize[i];
      default:
        unreachable();
    }
  });
}

/**
 * Generates an array of coordinates at which to sample a texture for a cubemap
 */
export function generateSamplePointsCube(
  n: number,
  args:
    | (TextureBuiltinInputArgs & {
        method: 'texel-centre';
      })
    | (TextureBuiltinInputArgs & {
        method: 'spiral';
        radius?: number;
        loops?: number;
      })
    | (TextureBuiltinInputArgs & {
        method: 'cube-edges';
      })
): {
  coords: vec3;
  mipLevel: number;
  arrayIndex?: number;
  offset?: undefined;
  component?: number;
  depthRef?: number;
}[] {
  const { method, descriptor } = args;
  const mipLevelCount = descriptor.mipLevelCount ?? 1;
  const size = virtualMipSize('2d', descriptor.size, 0);
  const textureWidth = size[0];
  const coords: vec3[] = [];
  switch (method) {
    case 'texel-centre': {
      for (let i = 0; i < n; i++) {
        const r = hashU32(i);
        const u = (Math.floor(lerp(0, textureWidth - 1, (r & 0xff) / 0xff)) + 0.5) / textureWidth;
        const v =
          (Math.floor(lerp(0, textureWidth - 1, ((r >> 8) & 0xff) / 0xff)) + 0.5) / textureWidth;
        const face = Math.floor(lerp(0, 6, ((r >> 16) & 0xff) / 0x100));
        coords.push(convertNormalized3DTexCoordToCubeCoord([u, v, face]));
      }
      break;
    }
    case 'spiral': {
      const { radius = 1.5, loops = 2 } = args;
      for (let i = 0; i < n; i++) {
        const f = (i + 1) / (Math.max(n, 2) - 1);
        const r = radius * f;
        const theta = loops * 2 * Math.PI * f;
        const phi = loops * 1.3 * Math.PI * f;
        const sinTheta = Math.sin(theta);
        const cosTheta = Math.cos(theta);
        const sinPhi = Math.sin(phi);
        const cosPhi = Math.cos(phi);
        const ux = cosTheta * sinPhi;
        const uy = cosPhi;
        const uz = sinTheta * sinPhi;
        coords.push([ux * r, uy * r, uz * r]);
      }
      break;
    }
    case 'cube-edges': {
      /* prettier-ignore */
      coords.push(
        // between edges
        // +x
        [  1   , -1.01,  0    ],  // wrap -y
        [  1   , +1.01,  0    ],  // wrap +y
        [  1   ,  0   , -1.01 ],  // wrap -z
        [  1   ,  0   , +1.01 ],  // wrap +z
        // -x
        [ -1   , -1.01,  0    ],  // wrap -y
        [ -1   , +1.01,  0    ],  // wrap +y
        [ -1   ,  0   , -1.01 ],  // wrap -z
        [ -1   ,  0   , +1.01 ],  // wrap +z

        // +y
        [ -1.01,  1   ,  0    ],  // wrap -x
        [ +1.01,  1   ,  0    ],  // wrap +x
        [  0   ,  1   , -1.01 ],  // wrap -z
        [  0   ,  1   , +1.01 ],  // wrap +z
        // -y
        [ -1.01, -1   ,  0    ],  // wrap -x
        [ +1.01, -1   ,  0    ],  // wrap +x
        [  0   , -1   , -1.01 ],  // wrap -z
        [  0   , -1   , +1.01 ],  // wrap +z

        // +z
        [ -1.01,  0   ,  1    ],  // wrap -x
        [ +1.01,  0   ,  1    ],  // wrap +x
        [  0   , -1.01,  1    ],  // wrap -y
        [  0   , +1.01,  1    ],  // wrap +y
        // -z
        [ -1.01,  0   , -1    ],  // wrap -x
        [ +1.01,  0   , -1    ],  // wrap +x
        [  0   , -1.01, -1    ],  // wrap -y
        [  0   , +1.01, -1    ],  // wrap +y

        // corners (see comment "Issues with corners of cubemaps")
        // for why these are commented out.
        // [-1.01, -1.02, -1.03],
        // [ 1.01, -1.02, -1.03],
        // [-1.01,  1.02, -1.03],
        // [ 1.01,  1.02, -1.03],
        // [-1.01, -1.02,  1.03],
        // [ 1.01, -1.02,  1.03],
        // [-1.01,  1.02,  1.03],
        // [ 1.01,  1.02,  1.03],
      );
      break;
    }
  }

  const _hashInputs = args.hashInputs.map(v =>
    typeof v === 'string' ? sumOfCharCodesOfString(v) : typeof v === 'boolean' ? (v ? 1 : 0) : v
  );
  const makeRangeValue = ({ num, type }: RangeDef, ...hashInputs: number[]) => {
    const range = num + (type === 'u32' ? 1 : 2);
    const number =
      (hashU32(..._hashInputs, ...hashInputs) / 0x1_0000_0000) * range - (type === 'u32' ? 0 : 1);
    return type === 'f32' ? number : Math.floor(number);
  };
  const makeIntHashValue = (min: number, max: number, ...hashInputs: number[]) => {
    const range = max - min;
    return min + Math.floor((hashU32(..._hashInputs, ...hashInputs) / 0x1_0000_0000) * range);
  };

  // Samplers across devices use different methods to interpolate.
  // Quantizing the texture coordinates seems to hit coords that produce
  // comparable results to our computed results.
  // Note: This value works with 8x8 textures. Other sizes have not been tested.
  // Values that worked for reference:
  // Win 11, NVidia 2070 Super: 16
  // Linux, AMD Radeon Pro WX 3200: 256
  // MacOS, M1 Mac: 256
  //
  // Note: When doing `textureGather...` we can't use texel centers
  // because which 4 pixels will be gathered jumps if we're slightly under
  // or slightly over the center
  //
  // Similarly, if we're using 'nearest' filtering then we don't want texel
  // edges for the same reason.
  //
  // Also note that for textureGather. The way it works for cube maps is to
  // first convert from cube map coordinate to a 2D texture coordinate and
  // a face. Then, choose 4 texels just like normal 2D texture coordinates.
  // If one of the 4 texels is outside the current face, wrap it to the correct
  // face.
  //
  // An issue this brings up though. Imagine a 2D texture with addressMode = 'repeat'
  //
  //       2d texture   (same texture repeated to show 'repeat')
  //     ┌───┬───┬───┐     ┌───┬───┬───┐
  //     │   │   │   │     │   │   │   │
  //     ├───┼───┼───┤     ├───┼───┼───┤
  //     │   │   │  a│     │c  │   │   │
  //     ├───┼───┼───┤     ├───┼───┼───┤
  //     │   │   │  b│     │d  │   │   │
  //     └───┴───┴───┘     └───┴───┴───┘
  //
  // Assume the texture coordinate is at the bottom right corner of a.
  // Then textureGather will grab c, d, b, a (no idea why that order).
  // but think of it as top-right, bottom-right, bottom-left, top-left.
  // Similarly, if the texture coordinate is at the top left of d it
  // will select the same 4 texels.
  //
  // But, in the case of a cubemap, each face is in different direction
  // relative to the face next to it.
  //
  //             +-----------+
  //             |0->u       |
  //             |↓          |
  //             |v   +y     |
  //             |    (2)    |
  //             |           |
  // +-----------+-----------+-----------+-----------+
  // |0->u       |0->u       |0->u       |0->u       |
  // |↓          |↓          |↓          |↓          |
  // |v   -x     |v   +z     |v   +x     |v   -z     |
  // |    (1)    |    (4)    |    (0)    |    (5)    |
  // |           |           |           |           |
  // +-----------+-----------+-----------+-----------+
  //             |0->u       |
  //             |↓          |
  //             |v   -y     |
  //             |    (3)    |
  //             |           |
  //             +-----------+
  //
  // As an example, imagine going from the +y to the +x face.
  // See diagram above, the right edge of the +y face wraps
  // to the top edge of the +x face.
  //
  //                             +---+---+
  //                             |  a|c  |
  //     ┌───┬───┬───┐           ┌───┬───┬───┐
  //     │   │   │   │           │  b│d  │   │
  //     ├───┼───┼───┤---+       ├───┼───┼───┤
  //     │   │   │  a│ c |       │   │   │   │
  //     ├───┼───┼───┤---+       ├───┼───┼───┤
  //     │   │   │  b│ d |       │   │   │   │
  //     └───┴───┴───┘---+       └───┴───┴───┘
  //        +y face                 +x face
  //
  // If the texture coordinate is in the bottom right corner of a,
  // the rectangle of texels we read are a,b,c,d and, if we the
  // texture coordinate is in the top left corner of d we also
  // read a,b,c,d according to the 2 diagrams above.
  //
  // But, notice that when reading from the POV of +y vs +x,
  // which actual a,b,c,d texels are different.
  //
  // From the POV of face +x: a,b are in face +x and c,d are in face +y
  // From the POV of face +y: a,c are in face +x and b,d are in face +y
  //
  // This is all the long way of saying that if we're on the edge of a cube
  // face we could get drastically different results because the orientation
  // of the rectangle of the 4 texels we use, rotates. So, we need to avoid
  // any values too close to the edge just in case our math is different than
  // the GPU's.
  //
  const kSubdivisionsPerTexel = 4;
  const avoidEdgeCase =
    !args.sampler || args.sampler.minFilter === 'nearest' || isBuiltinGather(args.textureBuiltin);
  const edgeRemainder = isBuiltinGather(args.textureBuiltin) ? kSubdivisionsPerTexel / 2 : 0;
  return coords.map((c, i) => {
    const mipLevel = args.mipLevel
      ? quantizeMipLevel(makeRangeValue(args.mipLevel, i), args.sampler?.mipmapFilter ?? 'nearest')
      : 0;
    const clampedMipLevel = clamp(mipLevel, { min: 0, max: mipLevelCount - 1 });
    const mipSize = virtualMipSize('2d', size, Math.ceil(clampedMipLevel));
    const q = [
      mipSize[0] * kSubdivisionsPerTexel,
      mipSize[0] * kSubdivisionsPerTexel,
      6 * kSubdivisionsPerTexel,
    ];

    const uvw = convertCubeCoordToNormalized3DTextureCoord(c);

    // If this is a corner, move to in so it's not
    // (see comment "Issues with corners of cubemaps")
    const ndx = getUnusedCubeCornerSampleIndex(mipSize[0], uvw);
    if (ndx >= 0) {
      const halfTexel = 0.5 / mipSize[0];
      uvw[0] = clamp(uvw[0], { min: halfTexel, max: 1 - halfTexel });
    }

    const quantizedUVW = uvw.map((v, i) => {
      // Quantize to kSubdivisionsPerPixel
      const v1 = Math.floor(v * q[i]);
      // If it's nearest or textureGather and we're on the edge of a texel then move us off the edge
      // since the edge could choose one texel or another.
      const isEdgeCase = Math.abs(v1 % kSubdivisionsPerTexel) === edgeRemainder;
      const v2 = isEdgeCase && avoidEdgeCase ? v1 + 1 : v1;
      // Convert back to texture coords slightly off
      return (v2 + 1 / 16) / q[i];
    }) as vec3;

    const coords = convertNormalized3DTexCoordToCubeCoord(quantizedUVW);
    return {
      coords,
      mipLevel,
      arrayIndex: args.arrayIndex ? makeRangeValue(args.arrayIndex, i, 2) : undefined,
      depthRef: args.depthRef ? makeRangeValue({ num: 1, type: 'f32' }, i, 5) : undefined,
      component: args.component ? makeIntHashValue(0, 4, i, 4) : undefined,
    };
  });
}

function wgslTypeFor(data: number | Dimensionality, type: 'f' | 'i' | 'u'): string {
  if (Array.isArray(data)) {
    switch (data.length) {
      case 1:
        return `${type}32`;
      case 2:
        return `vec2${type}`;
      case 3:
        return `vec3${type}`;
      default:
        unreachable();
    }
  }
  return `${type}32`;
}

function wgslExpr(data: number | vec1 | vec2 | vec3 | vec4): string {
  if (Array.isArray(data)) {
    switch (data.length) {
      case 1:
        return data[0].toString();
      case 2:
        return `vec2(${data.map(v => v.toString()).join(', ')})`;
      case 3:
        return `vec3(${data.map(v => v.toString()).join(', ')})`;
      default:
        unreachable();
    }
  }
  return data.toString();
}

function wgslExprFor(data: number | vec1 | vec2 | vec3 | vec4, type: 'f' | 'i' | 'u'): string {
  if (Array.isArray(data)) {
    switch (data.length) {
      case 1:
        return `${type}(${data[0].toString()})`;
      case 2:
        return `vec2${type}(${data.map(v => v.toString()).join(', ')})`;
      case 3:
        return `vec3${type}(${data.map(v => v.toString()).join(', ')})`;
      default:
        unreachable();
    }
  }
  return `${type}32(${data.toString()})`;
}

function binKey<T extends Dimensionality>(call: TextureCall<T>): string {
  const keys: string[] = [];
  for (const name of kTextureCallArgNames) {
    const value = call[name];
    if (value !== undefined) {
      if (name === 'offset' || name === 'component') {
        // offset and component must be constant expressions
        keys.push(`${name}: ${wgslExpr(value)}`);
      } else {
        keys.push(`${name}: ${wgslTypeFor(value, call.coordType)}`);
      }
    }
  }
  return `${call.builtin}(${keys.join(', ')})`;
}

function buildBinnedCalls<T extends Dimensionality>(calls: TextureCall<T>[]) {
  const args: string[] = [];
  const fields: string[] = [];
  const data: number[] = [];
  const prototype = calls[0];

  if (isBuiltinGather(prototype.builtin) && prototype['componentType']) {
    args.push(`/* component */ ${wgslExpr(prototype['component']!)}`);
  }

  // All texture builtins take a Texture
  args.push('T');

  if (builtinNeedsSampler(prototype.builtin)) {
    // textureSample*() builtins take a sampler as the second argument
    args.push('S');
  }

  for (const name of kTextureCallArgNames) {
    const value = prototype[name];
    if (value !== undefined) {
      if (name === 'offset') {
        args.push(`/* offset */ ${wgslExpr(value)}`);
      } else if (name === 'component') {
        // was handled above
      } else {
        const type =
          name === 'mipLevel'
            ? prototype.levelType!
            : name === 'arrayIndex'
            ? prototype.arrayIndexType!
            : name === 'sampleIndex'
            ? prototype.sampleIndexType!
            : name === 'depthRef'
            ? 'f'
            : prototype.coordType;
        args.push(`args.${name}`);
        fields.push(`@align(16) ${name} : ${wgslTypeFor(value, type)}`);
      }
    }
  }

  for (const call of calls) {
    for (const name of kTextureCallArgNames) {
      const value = call[name];
      assert(
        (prototype[name] === undefined) === (value === undefined),
        'texture calls are not binned correctly'
      );
      if (value !== undefined && name !== 'offset' && name !== 'component') {
        const type = getCallArgType<T>(call, name);
        const bitcastToU32 = kBitCastFunctions[type];
        if (value instanceof Array) {
          for (const c of value) {
            data.push(bitcastToU32(c));
          }
        } else {
          data.push(bitcastToU32(value));
        }
        // All fields are aligned to 16 bytes.
        while ((data.length & 3) !== 0) {
          data.push(0);
        }
      }
    }
  }

  const expr = `${prototype.builtin}(${args.join(', ')})`;

  return { expr, fields, data };
}

function binCalls<T extends Dimensionality>(calls: TextureCall<T>[]): number[][] {
  const map = new Map<string, number>(); // key to bin index
  const bins: number[][] = [];
  calls.forEach((call, callIdx) => {
    const key = binKey(call);
    const binIdx = map.get(key);
    if (binIdx === undefined) {
      map.set(key, bins.length);
      bins.push([callIdx]);
    } else {
      bins[binIdx].push(callIdx);
    }
  });
  return bins;
}

export function describeTextureCall<T extends Dimensionality>(call: TextureCall<T>): string {
  const args: string[] = [];
  if (isBuiltinGather(call.builtin) && call.componentType) {
    args.push(`component: ${wgslExprFor(call.component!, call.componentType)}`);
  }
  args.push('texture: T');
  if (builtinNeedsSampler(call.builtin)) {
    args.push('sampler: S');
  }
  for (const name of kTextureCallArgNames) {
    const value = call[name];
    if (value !== undefined && name !== 'component') {
      if (name === 'coords') {
        args.push(`${name}: ${wgslExprFor(value, call.coordType)}`);
      } else if (name === 'mipLevel') {
        args.push(`${name}: ${wgslExprFor(value, call.levelType!)}`);
      } else if (name === 'arrayIndex') {
        args.push(`${name}: ${wgslExprFor(value, call.arrayIndexType!)}`);
      } else if (name === 'sampleIndex') {
        args.push(`${name}: ${wgslExprFor(value, call.sampleIndexType!)}`);
      } else if (name === 'depthRef') {
        args.push(`${name}: ${wgslExprFor(value, 'f')}`);
      } else {
        args.push(`${name}: ${wgslExpr(value)}`);
      }
    }
  }
  return `${call.builtin}(${args.join(', ')})`;
}

const s_deviceToPipelines = new WeakMap<GPUDevice, Map<string, GPURenderPipeline>>();

/**
 * Given a list of "calls", each one of which has a texture coordinate,
 * generates a fragment shader that uses the fragment position as an index
 * (position.y * 256 + position.x) That index is then used to look up a
 * coordinate from a storage buffer which is used to call the WGSL texture
 * function to read/sample the texture, and then write to an rgba32float
 * texture.  We then read the rgba32float texture for the per "call" results.
 *
 * Calls are "binned" by call parameters. Each bin has its own structure and
 * field in the storage buffer. This allows the calls to be non-homogenous and
 * each have their own data type for coordinates.
 *
 * Note: this function returns:
 *
 * 'results': an array of results, one for each call.
 *
 * 'run': a function that accepts a texture and runs the same class pipeline with
 *        that texture as input, returning an array of results. This can be used by
 *        identifySamplePoints to query the mix-weights used. We do this so we're
 *        using the same shader that generated the original results when querying
 *        the weights.
 *
 * 'destroy': a function that cleans up the buffers used by `run`.
 */
export async function doTextureCalls<T extends Dimensionality>(
  t: GPUTest,
  gpuTexture: GPUTexture | GPUExternalTexture,
  viewDescriptor: GPUTextureViewDescriptor,
  textureType: string,
  sampler: GPUSamplerDescriptor | undefined,
  calls: TextureCall<T>[]
) {
  const {
    format,
    dimension,
    depthOrArrayLayers,
    sampleCount,
  }: {
    format: GPUTextureFormat;
    dimension: GPUTextureDimension;
    depthOrArrayLayers: number;
    sampleCount: number;
  } =
    gpuTexture instanceof GPUExternalTexture
      ? { format: 'rgba8unorm', dimension: '2d', depthOrArrayLayers: 1, sampleCount: 1 }
      : gpuTexture;

  let structs = '';
  let body = '';
  let dataFields = '';
  const data: number[] = [];
  let callCount = 0;
  const binned = binCalls(calls);
  binned.forEach((binCalls, binIdx) => {
    const b = buildBinnedCalls(binCalls.map(callIdx => calls[callIdx]));
    structs += `struct Args${binIdx} {
  ${b.fields.join(',  \n')}
}
`;
    dataFields += `  args${binIdx} : array<Args${binIdx}, ${binCalls.length}>,
`;
    body += `
  {
    let is_active = (frag_idx >= ${callCount}) & (frag_idx < ${callCount + binCalls.length});
    let args = data.args${binIdx}[frag_idx - ${callCount}];
    let call = ${b.expr};
    result = select(result, call, is_active);
  }
`;
    callCount += binCalls.length;
    data.push(...b.data);
  });

  const dataBuffer = t.createBufferTracked({
    size: data.length * 4,
    usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.STORAGE,
  });
  t.device.queue.writeBuffer(dataBuffer, 0, new Uint32Array(data));

  const builtin = calls[0].builtin;
  const isCompare = isBuiltinComparison(builtin);

  const { resultType, resultFormat, componentType } = isBuiltinGather(builtin)
    ? getTextureFormatTypeInfo(format)
    : gpuTexture instanceof GPUExternalTexture
    ? ({ resultType: 'vec4f', resultFormat: 'rgba32float', componentType: 'f32' } as const)
    : textureType.includes('depth')
    ? ({ resultType: 'f32', resultFormat: 'rgba32float', componentType: 'f32' } as const)
    : getTextureFormatTypeInfo(format);
  const returnType = `vec4<${componentType}>`;

  const samplerType = isCompare ? 'sampler_comparison' : 'sampler';

  const rtWidth = 256;
  const renderTarget = t.createTextureTracked({
    format: resultFormat,
    size: { width: rtWidth, height: Math.ceil(calls.length / rtWidth) },
    usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.RENDER_ATTACHMENT,
  });

  const code = `
${structs}

struct Data {
${dataFields}
}

@vertex
fn vs_main(@builtin(vertex_index) vertex_index : u32) -> @builtin(position) vec4f {
  let positions = array(
    vec4f(-1,  1, 0, 1), vec4f( 1,  1, 0, 1),
    vec4f(-1, -1, 0, 1), vec4f( 1, -1, 0, 1),
  );
  return positions[vertex_index];
}

@group(0) @binding(0) var          T    : ${textureType};
${sampler ? `@group(0) @binding(1) var          S    : ${samplerType}` : ''};
@group(0) @binding(2) var<storage> data : Data;

@fragment
fn fs_main(@builtin(position) frag_pos : vec4f) -> @location(0) ${returnType} {
  let frag_idx = u32(frag_pos.x) + u32(frag_pos.y) * ${renderTarget.width};
  var result : ${resultType};
${body}
  return ${returnType}(result);
}
`;

  const pipelines = s_deviceToPipelines.get(t.device) ?? new Map<string, GPURenderPipeline>();
  s_deviceToPipelines.set(t.device, pipelines);

  // unfilterable-float textures can only be used with manually created bindGroupLayouts
  // since the default 'auto' layout requires filterable textures/samplers.
  // So, if we don't need filtering, don't request a filtering sampler. If we require
  // filtering then check if the format is 32float format and if float32-filterable
  // is enabled.
  const info = kTextureFormatInfo[format ?? 'rgba8unorm'];
  const isFiltering =
    !!sampler &&
    (sampler.minFilter === 'linear' ||
      sampler.magFilter === 'linear' ||
      sampler.mipmapFilter === 'linear');
  let sampleType: GPUTextureSampleType = textureType.startsWith('texture_depth')
    ? 'depth'
    : isDepthTextureFormat(format)
    ? 'unfilterable-float'
    : isStencilTextureFormat(format)
    ? 'uint'
    : info.color?.type ?? 'float';
  if (isFiltering && sampleType === 'unfilterable-float') {
    assert(is32Float(format));
    assert(t.device.features.has('float32-filterable'));
    sampleType = 'float';
  }
  if (sampleCount > 1 && sampleType === 'float') {
    sampleType = 'unfilterable-float';
  }

  const entries: GPUBindGroupLayoutEntry[] = [
    {
      binding: 2,
      visibility: GPUShaderStage.FRAGMENT,
      buffer: {
        type: 'read-only-storage',
      },
    },
  ];

  const viewDimension = effectiveViewDimensionForDimension(
    viewDescriptor.dimension,
    dimension,
    depthOrArrayLayers
  );

  if (textureType.includes('storage')) {
    entries.push({
      binding: 0,
      visibility: GPUShaderStage.FRAGMENT,
      storageTexture: {
        access: 'read-only',
        viewDimension,
        format,
      },
    });
  } else if (gpuTexture instanceof GPUExternalTexture) {
    entries.push({
      binding: 0,
      visibility: GPUShaderStage.FRAGMENT,
      externalTexture: {},
    });
  } else {
    entries.push({
      binding: 0,
      visibility: GPUShaderStage.FRAGMENT,
      texture: {
        sampleType,
        viewDimension,
        multisampled: sampleCount > 1,
      },
    });
  }

  if (sampler) {
    entries.push({
      binding: 1,
      visibility: GPUShaderStage.FRAGMENT,
      sampler: {
        type: isCompare ? 'comparison' : isFiltering ? 'filtering' : 'non-filtering',
      },
    });
  }

  const id = `${renderTarget.format}:${JSON.stringify(entries)}:${code}`;
  let pipeline = pipelines.get(id);
  if (!pipeline) {
    const shaderModule = t.device.createShaderModule({ code });
    const bindGroupLayout = t.device.createBindGroupLayout({ entries });
    const layout = t.device.createPipelineLayout({
      bindGroupLayouts: [bindGroupLayout],
    });

    pipeline = t.device.createRenderPipeline({
      layout,
      vertex: { module: shaderModule },
      fragment: {
        module: shaderModule,
        targets: [{ format: renderTarget.format }],
      },
      primitive: { topology: 'triangle-strip' },
    });

    pipelines.set(id, pipeline);
  }

  const gpuSampler = sampler ? t.device.createSampler(sampler) : undefined;

  const run = async (gpuTexture: GPUTexture | GPUExternalTexture) => {
    const bindGroup = t.device.createBindGroup({
      layout: pipeline!.getBindGroupLayout(0),
      entries: [
        {
          binding: 0,
          resource:
            gpuTexture instanceof GPUExternalTexture
              ? gpuTexture
              : gpuTexture.createView(viewDescriptor),
        },
        ...(sampler ? [{ binding: 1, resource: gpuSampler! }] : []),
        { binding: 2, resource: { buffer: dataBuffer } },
      ],
    });

    const bytesPerRow = align(16 * renderTarget.width, 256);
    const resultBuffer = t.createBufferTracked({
      size: renderTarget.height * bytesPerRow,
      usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ,
    });

    const encoder = t.device.createCommandEncoder();

    const renderPass = encoder.beginRenderPass({
      colorAttachments: [
        {
          view: renderTarget.createView(),
          loadOp: 'clear',
          storeOp: 'store',
        },
      ],
    });

    renderPass.setPipeline(pipeline!);
    renderPass.setBindGroup(0, bindGroup);
    renderPass.draw(4);
    renderPass.end();
    encoder.copyTextureToBuffer(
      { texture: renderTarget },
      { buffer: resultBuffer, bytesPerRow },
      { width: renderTarget.width, height: renderTarget.height }
    );
    t.device.queue.submit([encoder.finish()]);

    await resultBuffer.mapAsync(GPUMapMode.READ);

    const view = TexelView.fromTextureDataByReference(
      renderTarget.format as EncodableTextureFormat,
      new Uint8Array(resultBuffer.getMappedRange()),
      {
        bytesPerRow,
        rowsPerImage: renderTarget.height,
        subrectOrigin: [0, 0, 0],
        subrectSize: [renderTarget.width, renderTarget.height],
      }
    );

    let outIdx = 0;
    const out = new Array<PerTexelComponent<number>>(calls.length);
    for (const bin of binned) {
      for (const callIdx of bin) {
        const x = outIdx % rtWidth;
        const y = Math.floor(outIdx / rtWidth);
        out[callIdx] = view.color({ x, y, z: 0 });
        outIdx++;
      }
    }

    resultBuffer.destroy();

    return out;
  };

  const results = await run(gpuTexture);

  return {
    run,
    results,
    destroy() {
      dataBuffer.destroy();
      renderTarget.destroy();
    },
  };
}
