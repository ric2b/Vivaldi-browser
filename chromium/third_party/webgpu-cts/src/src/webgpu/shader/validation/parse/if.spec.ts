export const description = `Validation parser tests for 'if' statements`;

import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { keysOf } from '../../../../common/util/data_tables.js';
import { ShaderValidationTest } from '../shader_validation_test.js';

export const g = makeTestGroup(ShaderValidationTest);

const kTests = {
  true: { wgsl: `if true {}`, pass: true },
  paren_true: { wgsl: `if (true) {}`, pass: true },
  expr: { wgsl: `if expr {}`, pass: true },
  paren_expr: { wgsl: `if (expr) {}`, pass: true },

  true_else: { wgsl: `if true {} else {}`, pass: true },
  paren_true_else: { wgsl: `if (true) {} else {}`, pass: true },
  expr_else: { wgsl: `if expr {} else {}`, pass: true },
  paren_expr_else: { wgsl: `if (expr) {} else {}`, pass: true },

  true_else_if_true: { wgsl: `if true {} else if true {}`, pass: true },
  paren_true_else_if_paren_true: { wgsl: `if (true) {} else if (true) {}`, pass: true },
  true_else_if_paren_true: { wgsl: `if true {} else if (true) {}`, pass: true },
  paren_true_else_if_true: { wgsl: `if (true) {} else if true {}`, pass: true },

  expr_else_if_expr: { wgsl: `if expr {} else if expr {}`, pass: true },
  paren_expr_else_if_paren_expr: { wgsl: `if (expr) {} else if (expr) {}`, pass: true },
  expr_else_if_paren_expr: { wgsl: `if expr {} else if (expr) {}`, pass: true },
  paren_expr_else_if_expr: { wgsl: `if (expr) {} else if expr {}`, pass: true },

  if: { wgsl: `if`, pass: false },
  block: { wgsl: `if{}`, pass: false },
  semicolon: { wgsl: `if;`, pass: false },
  true_lbrace: { wgsl: `if true {`, pass: false },
  true_rbrace: { wgsl: `if true }`, pass: false },

  lparen_true: { wgsl: `if (true {}`, pass: false },
  rparen_true: { wgsl: `if )true {}`, pass: false },
  true_lparen: { wgsl: `if true( {}`, pass: false },
  true_rparen: { wgsl: `if true) {}`, pass: false },

  true_else_if_no_block: { wgsl: `if true {} else if `, pass: false },
  true_else_if: { wgsl: `if true {} else if {}`, pass: false },
  true_else_if_semicolon: { wgsl: `if true {} else if ;`, pass: false },
  true_else_if_true_lbrace: { wgsl: `if true {} else if true {`, pass: false },
  true_else_if_true_rbrace: { wgsl: `if true {} else if true }`, pass: false },

  true_else_if_lparen_true: { wgsl: `if true {} else if (true {}`, pass: false },
  true_else_if_rparen_true: { wgsl: `if true {} else if )true {}`, pass: false },
  true_else_if_true_lparen: { wgsl: `if true {} else if true( {}`, pass: false },
  true_else_if_true_rparen: { wgsl: `if true {} else if true) {}`, pass: false },

  else: { wgsl: `else { }`, pass: false },
  else_if: { wgsl: `else if true { }`, pass: false },
  true_elif: { wgsl: `if (true) { } elif (true) {}`, pass: false },
  true_elsif: { wgsl: `if (true) { } elsif (true) {}`, pass: false },
  elif: { wgsl: `elif (true) {}`, pass: false },
  elsif: { wgsl: `elsif (true) {}`, pass: false },
};

g.test('parse')
  .desc(`Test that 'if' statements are parsed correctly.`)
  .params(u => u.combine('test', keysOf(kTests)))
  .fn(t => {
    const code = `
fn f() {
  let expr = true;
  ${kTests[t.params.test].wgsl}
}`;
    t.expectCompileResult(kTests[t.params.test].pass, code);
  });
