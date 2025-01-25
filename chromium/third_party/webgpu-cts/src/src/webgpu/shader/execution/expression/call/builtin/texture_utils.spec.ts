export const description = `
Tests for texture_utils.ts
`;

import { makeTestGroup } from '../../../../../../common/framework/test_group.js';
import { assert } from '../../../../../../common/util/util.js';
import { GPUTest } from '../../../../../gpu_test.js';
import { getTextureDimensionFromView, virtualMipSize } from '../../../../../util/texture/base.js';
import {
  kTexelRepresentationInfo,
  PerTexelComponent,
  TexelRepresentationInfo,
} from '../../../../../util/texture/texel_data.js';

import {
  chooseTextureSize,
  createTextureWithRandomDataAndGetTexels,
  isSupportedViewFormatCombo,
  readTextureToTexelViews,
  texelsApproximatelyEqual,
} from './texture_utils.js';

export const g = makeTestGroup(GPUTest);

function texelFormat(texel: Readonly<PerTexelComponent<number>>, rep: TexelRepresentationInfo) {
  return rep.componentOrder.map(component => `${component}: ${texel[component]}`).join(', ');
}

g.test('readTextureToTexelViews')
  .desc('test readTextureToTexelViews for various formats and dimensions')
  .params(u =>
    u
      .combineWithParams([
        { srcFormat: 'r8unorm', texelViewFormat: 'rgba32float' },
        { srcFormat: 'r8sint', texelViewFormat: 'rgba32sint' },
        { srcFormat: 'r8uint', texelViewFormat: 'rgba32uint' },
        { srcFormat: 'rgba32float', texelViewFormat: 'rgba32float' },
        { srcFormat: 'rgba32uint', texelViewFormat: 'rgba32uint' },
        { srcFormat: 'rgba32sint', texelViewFormat: 'rgba32sint' },
        { srcFormat: 'depth24plus', texelViewFormat: 'rgba32float' },
        { srcFormat: 'depth24plus', texelViewFormat: 'r32float' },
        { srcFormat: 'depth24plus-stencil8', texelViewFormat: 'r32float' },
        { srcFormat: 'stencil8', texelViewFormat: 'rgba32sint' },
      ] as const)
      .combine('viewDimension', ['1d', '2d', '2d-array', '3d', 'cube', 'cube-array'] as const)
      .filter(t => isSupportedViewFormatCombo(t.srcFormat, t.viewDimension))
  )
  .beforeAllSubcases(t => {
    t.skipIfTextureViewDimensionNotSupported(t.params.viewDimension);
  })
  .fn(async t => {
    const { srcFormat, texelViewFormat, viewDimension } = t.params;
    const size = chooseTextureSize({ minSize: 8, minBlocks: 4, format: srcFormat, viewDimension });
    const descriptor: GPUTextureDescriptor = {
      format: srcFormat,
      dimension: getTextureDimensionFromView(viewDimension),
      size,
      mipLevelCount: viewDimension === '1d' ? 1 : 3,
      usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.TEXTURE_BINDING,
      ...(t.isCompatibility && { textureBindingViewDimension: viewDimension }),
    };
    const { texels: expectedTexelViews, texture } = await createTextureWithRandomDataAndGetTexels(
      t,
      descriptor
    );
    const actualTexelViews = await readTextureToTexelViews(t, texture, descriptor, texelViewFormat);

    assert(actualTexelViews.length === expectedTexelViews.length, 'num mip levels match');

    const errors = [];
    for (let mipLevel = 0; mipLevel < actualTexelViews.length; ++mipLevel) {
      const actualMipLevelTexelView = actualTexelViews[mipLevel];
      const expectedMipLevelTexelView = expectedTexelViews[mipLevel];
      const mipLevelSize = virtualMipSize(texture.dimension, size, mipLevel);

      const actualRep = kTexelRepresentationInfo[actualMipLevelTexelView.format];
      const expectedRep = kTexelRepresentationInfo[expectedMipLevelTexelView.format];

      for (let z = 0; z < mipLevelSize[2]; ++z) {
        for (let y = 0; y < mipLevelSize[1]; ++y) {
          for (let x = 0; x < mipLevelSize[0]; ++x) {
            const actual = actualMipLevelTexelView.color({ x, y, z });
            const expected = expectedMipLevelTexelView.color({ x, y, z });
            // This currently expects the exact same values in actual vs expected.
            // It's possible this needs to be relaxed slightly but only for non-integer formats.
            // For now, if the tests pass everywhere, we'll keep it at 0 tolerance.
            const maxFractionalDiff = 0;
            if (
              !texelsApproximatelyEqual(
                actual,
                actualMipLevelTexelView.format,
                expected,
                expectedMipLevelTexelView.format,
                maxFractionalDiff
              )
            ) {
              const actualStr = texelFormat(actual, actualRep);
              const expectedStr = texelFormat(expected, expectedRep);
              errors.push(
                `texel at ${x}, ${y}, ${z}, expected: ${expectedStr}, actual: ${actualStr}`
              );
            }
          }
        }
      }

      assert(errors.length === 0, errors.join('\n'));
    }
  });
