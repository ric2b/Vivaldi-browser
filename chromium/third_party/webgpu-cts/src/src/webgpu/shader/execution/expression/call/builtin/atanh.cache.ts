import { FP } from '../../../../../util/floating_point.js';
import { biasedRange } from '../../../../../util/math.js';
import { makeCaseCache } from '../../case_cache.js';

// Cases: [f32|f16]_[non_]const
const cases = (['f32', 'f16'] as const)
  .flatMap(trait =>
    ([true, false] as const).map(nonConst => ({
      [`${trait}_${nonConst ? 'non_const' : 'const'}`]: () => {
        return FP[trait].generateScalarToIntervalCases(
          [
            // discontinuity at x = -1
            ...biasedRange(FP[trait].constants().negative.less_than_one, -0.9, 20),
            -1,
            // discontinuity at x = 1
            ...biasedRange(FP[trait].constants().positive.less_than_one, 0.9, 20),
            1,
            ...FP[trait].scalarRange(),
          ],
          nonConst ? 'unfiltered' : 'finite',
          FP[trait].atanhInterval
        );
      },
    }))
  )
  .reduce((a, b) => ({ ...a, ...b }), {});

export const d = makeCaseCache('atanh', cases);
