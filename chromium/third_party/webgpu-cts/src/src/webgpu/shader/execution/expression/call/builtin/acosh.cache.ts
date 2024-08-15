import { FP } from '../../../../../util/floating_point.js';
import { biasedRange } from '../../../../../util/math.js';
import { makeCaseCache } from '../../case_cache.js';

// Cases: [f32|f16]_[non_]const
const cases = (['f32', 'f16'] as const)
  .flatMap(trait =>
    ([true, false] as const).map(nonConst => ({
      [`${trait}_${nonConst ? 'non_const' : 'const'}`]: () => {
        return FP[trait].generateScalarToIntervalCases(
          [...biasedRange(1, 2, 100), ...FP[trait].scalarRange()], // x near 1 can be problematic to implement
          nonConst ? 'unfiltered' : 'finite',
          ...FP[trait].acoshIntervals
        );
      },
    }))
  )
  .reduce((a, b) => ({ ...a, ...b }), {});

export const d = makeCaseCache('acosh', cases);
