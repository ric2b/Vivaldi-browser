export const description = `Validation parser tests for 'while' statements`;

import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { keysOf } from '../../../../common/util/data_tables.js';
import { ShaderValidationTest } from '../shader_validation_test.js';

export const g = makeTestGroup(ShaderValidationTest);

const kTests = {
  true: { wgsl: `while true {}`, pass: true },
  paren_true: { wgsl: `while (true) {}`, pass: true },
  true_break: { wgsl: `while true { break; }`, pass: true },
  true_discard: { wgsl: `while true { discard; }`, pass: true },
  true_return: { wgsl: `while true { return; }`, pass: true },
  expr: { wgsl: `while expr {}`, pass: true },
  paren_expr: { wgsl: `while (expr) {}`, pass: true },

  while: { wgsl: `while`, pass: false },
  block: { wgsl: `while{}`, pass: false },
  semicolon: { wgsl: `while;`, pass: false },
  true_lbrace: { wgsl: `while true {`, pass: false },
  true_rbrace: { wgsl: `while true }`, pass: false },

  lparen_true: { wgsl: `while (true {}`, pass: false },
  rparen_true: { wgsl: `while )true {}`, pass: false },
  true_lparen: { wgsl: `while true( {}`, pass: false },
  true_rparen: { wgsl: `while true) {}`, pass: false },
  lparen_true_lparen: { wgsl: `while (true( {}`, pass: false },
  rparen_true_rparen: { wgsl: `while )true) {}`, pass: false },
};

g.test('parse')
  .desc(`Test that 'while' statements are parsed correctly.`)
  .params(u => u.combine('test', keysOf(kTests)))
  .fn(t => {
    const code = `
fn f() {
  let expr = true;
  ${kTests[t.params.test].wgsl}
}`;
    t.expectCompileResult(kTests[t.params.test].pass, code);
  });
