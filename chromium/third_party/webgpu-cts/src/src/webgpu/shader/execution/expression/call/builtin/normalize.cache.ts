import { FP } from '../../../../../util/floating_point.js';
import { makeCaseCache } from '../../case_cache.js';

// Cases: [f32|f16]_vecN_[non_]const
const cases = (['f32', 'f16'] as const)
  .flatMap(trait =>
    ([2, 3, 4] as const).flatMap(dim =>
      ([true, false] as const).map(nonConst => ({
        [`${trait}_vec${dim}_${nonConst ? 'non_const' : 'const'}`]: () => {
          return FP[trait].generateVectorToVectorCases(
            FP[trait].vectorRange(dim),
            nonConst ? 'unfiltered' : 'finite',
            FP[trait].normalizeInterval
          );
        },
      }))
    )
  )
  .reduce((a, b) => ({ ...a, ...b }), {});

export const d = makeCaseCache('normalize', cases);
