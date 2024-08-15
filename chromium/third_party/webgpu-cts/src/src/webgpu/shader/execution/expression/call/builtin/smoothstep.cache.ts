import { FP } from '../../../../../util/floating_point.js';
import { makeCaseCache } from '../../case_cache.js';

// Cases: [f32|f16]_[non_]const
const cases = (['f32', 'f16'] as const)
  .flatMap(trait =>
    ([true, false] as const).map(nonConst => ({
      [`${trait}_${nonConst ? 'non_const' : 'const'}`]: () => {
        return FP[trait].generateScalarTripleToIntervalCases(
          FP[trait].sparseScalarRange(),
          FP[trait].sparseScalarRange(),
          FP[trait].sparseScalarRange(),
          nonConst ? 'unfiltered' : 'finite',
          FP[trait].smoothStepInterval
        );
      },
    }))
  )
  .reduce((a, b) => ({ ...a, ...b }), {});

export const d = makeCaseCache('smoothstep', cases);
