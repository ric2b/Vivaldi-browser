export const description = `Validation parser tests for 'return' statements`;

import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { keysOf } from '../../../../common/util/data_tables.js';
import { ShaderValidationTest } from '../shader_validation_test.js';

export const g = makeTestGroup(ShaderValidationTest);

const kTests = {
  no_expr: { wgsl: `return;`, pass_value: false, pass_no_value: true },
  v: { wgsl: `return v;`, pass_value: true, pass_no_value: false },
  literal: { wgsl: `return 10;`, pass_value: true, pass_no_value: false },
  expr: { wgsl: `return 1 + 2;`, pass_value: true, pass_no_value: false },
  paren_expr: { wgsl: `return (1 + 2);`, pass_value: true, pass_no_value: false },
  call: { wgsl: `return x();`, pass_value: true, pass_no_value: false },

  v_no_semicolon: { wgsl: `return v`, pass_value: false, pass_no_value: false },
  expr_no_semicolon: { wgsl: `return 1 + 2`, pass_value: false, pass_no_value: false },
  phony_assign: { wgsl: `return _ = 1;`, pass_value: false, pass_no_value: false },
  increment: { wgsl: `return v++;`, pass_value: false, pass_no_value: false },
  compound_assign: { wgsl: `return v += 4;`, pass_value: false, pass_no_value: false },
  lparen_literal: { wgsl: `return (4;`, pass_value: false, pass_no_value: false },
  literal_lparen: { wgsl: `return 4(;`, pass_value: false, pass_no_value: false },
  rparen_literal: { wgsl: `return )4;`, pass_value: false, pass_no_value: false },
  literal_rparen: { wgsl: `return 4);`, pass_value: false, pass_no_value: false },
  lparen_literal_lparen: { wgsl: `return (4(;`, pass_value: false, pass_no_value: false },
  rparen_literal_rparen: { wgsl: `return )4);`, pass_value: false, pass_no_value: false },
};

g.test('parse')
  .desc(`Test that 'return' statements are parsed correctly.`)
  .params(u =>
    u.combine('test', keysOf(kTests)).combine('fn_returns_value', [false, true] as const)
  )
  .fn(t => {
    const code = `
fn f() ${t.params.fn_returns_value ? '-> i32' : ''} {
  let v = 42;
  ${kTests[t.params.test].wgsl}
}
fn x() -> i32 {
  return 1;
}
`;
    t.expectCompileResult(
      t.params.fn_returns_value
        ? kTests[t.params.test].pass_value
        : kTests[t.params.test].pass_no_value,
      code
    );
  });
