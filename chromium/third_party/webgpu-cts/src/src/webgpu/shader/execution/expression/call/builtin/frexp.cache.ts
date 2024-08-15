import { skipUndefined } from '../../../../../util/compare.js';
import { Scalar, Vector, i32, toVector } from '../../../../../util/conversion.js';
import { FP } from '../../../../../util/floating_point.js';
import { frexp } from '../../../../../util/math.js';
import { Case } from '../../case.js';
import { makeCaseCache } from '../../case_cache.js';

/* @returns a fract Case for a given scalar or vector input */
function makeCaseFract(v: number | readonly number[], trait: 'f32' | 'f16'): Case {
  const fp = FP[trait];
  let toInput: (n: readonly number[]) => Scalar | Vector;
  let toOutput: (n: readonly number[]) => Scalar | Vector;
  if (v instanceof Array) {
    // Input is vector
    toInput = (n: readonly number[]) => toVector(n, fp.scalarBuilder);
    toOutput = (n: readonly number[]) => toVector(n, fp.scalarBuilder);
  } else {
    // Input is scalar, also wrap it in an array.
    v = [v];
    toInput = (n: readonly number[]) => fp.scalarBuilder(n[0]);
    toOutput = (n: readonly number[]) => fp.scalarBuilder(n[0]);
  }

  v = v.map(fp.quantize);
  if (v.some(e => e !== 0 && fp.isSubnormal(e))) {
    return { input: toInput(v), expected: skipUndefined(undefined) };
  }

  const fs = v.map(e => {
    return frexp(e, trait).fract;
  });

  return { input: toInput(v), expected: toOutput(fs) };
}

/* @returns an exp Case for a given scalar or vector input */
function makeCaseExp(v: number | readonly number[], trait: 'f32' | 'f16'): Case {
  const fp = FP[trait];
  let toInput: (n: readonly number[]) => Scalar | Vector;
  let toOutput: (n: readonly number[]) => Scalar | Vector;
  if (v instanceof Array) {
    // Input is vector
    toInput = (n: readonly number[]) => toVector(n, fp.scalarBuilder);
    toOutput = (n: readonly number[]) => toVector(n, i32);
  } else {
    // Input is scalar, also wrap it in an array.
    v = [v];
    toInput = (n: readonly number[]) => fp.scalarBuilder(n[0]);
    toOutput = (n: readonly number[]) => i32(n[0]);
  }

  v = v.map(fp.quantize);
  if (v.some(e => e !== 0 && fp.isSubnormal(e))) {
    return { input: toInput(v), expected: skipUndefined(undefined) };
  }

  const fs = v.map(e => {
    return frexp(e, trait).exp;
  });

  return { input: toInput(v), expected: toOutput(fs) };
}

// Cases: [f32|f16]_vecN_[exp|whole]
const vec_cases = (['f32', 'f16'] as const)
  .flatMap(trait =>
    ([2, 3, 4] as const).flatMap(dim =>
      (['exp', 'fract'] as const).map(portion => ({
        [`${trait}_vec${dim}_${portion}`]: () => {
          return FP[trait]
            .vectorRange(dim)
            .map(v => (portion === 'exp' ? makeCaseExp(v, trait) : makeCaseFract(v, trait)));
        },
      }))
    )
  )
  .reduce((a, b) => ({ ...a, ...b }), {});

// Cases: [f32|f16]_[exp|whole]
const scalar_cases = (['f32', 'f16'] as const)
  .flatMap(trait =>
    (['exp', 'fract'] as const).map(portion => ({
      [`${trait}_${portion}`]: () => {
        return FP[trait]
          .scalarRange()
          .map(v => (portion === 'exp' ? makeCaseExp(v, trait) : makeCaseFract(v, trait)));
      },
    }))
  )
  .reduce((a, b) => ({ ...a, ...b }), {});

export const d = makeCaseCache('frexp', {
  ...scalar_cases,
  ...vec_cases,
});
