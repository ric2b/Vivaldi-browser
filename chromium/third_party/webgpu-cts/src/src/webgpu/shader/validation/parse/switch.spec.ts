export const description = `Validation parser tests for 'switch' statements`;

import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { keysOf } from '../../../../common/util/data_tables.js';
import { ShaderValidationTest } from '../shader_validation_test.js';

export const g = makeTestGroup(ShaderValidationTest);

const kTests = {
  L_default: { wgsl: `switch L { default {} }`, pass: true },
  L_paren_default: { wgsl: `switch (L) { default {} }`, pass: true },
  L_case_1_2_default: { wgsl: `switch L { case 1, 2 {} default {} }`, pass: true },
  L_case_1_case_2_default: { wgsl: `switch L { case 1 {} case 2 {} default {} }`, pass: true },
  L_case_1_colon_case_2_colon_default_colon: {
    wgsl: `switch L { case 1: {} case 2: {} default: {} }`,
    pass: true,
  },
  L_case_1_colon_default_colon: { wgsl: `switch L { case 1: {} default: {} }`, pass: true },
  L_case_1_colon_default: { wgsl: `switch L { case 1: {} default {} }`, pass: true },
  L_case_1_default_2: { wgsl: `switch L { case 1, default, 2 {} }`, pass: true },
  L_case_1_default_case_2: { wgsl: `switch L { case 1 {} default {} case 2 {} }`, pass: true },
  L_case_1_default_colon: { wgsl: `switch L { case 1 {} default: {} }`, pass: true },
  L_case_1_default: { wgsl: `switch L { case 1 {} default {} }`, pass: true },
  L_case_2_1_default: { wgsl: `switch L { case 2, 1 {} default {} }`, pass: true },
  L_case_2_case_1_default: { wgsl: `switch L { case 2 {} case 1 {} default {} }`, pass: true },
  L_case_2_default_case_1: { wgsl: `switch L { case 2 {} default {} case 1 {} }`, pass: true },
  L_case_builtin_default: { wgsl: `switch L { case max(1,2) {} default {} }`, pass: true },
  L_case_C1_case_C2_default: { wgsl: `switch L { case C1 {} case C2 {} default {} }`, pass: true },
  L_case_C1_default: { wgsl: `switch L { case C1 {} default {} }`, pass: true },
  L_case_default_1: { wgsl: `switch L { case default, 1 {} }`, pass: true },
  L_case_default_2_1: { wgsl: `switch L { case default, 2, 1 {} }`, pass: true },
  L_case_default_2_case_1: { wgsl: `switch L { case default, 2 {} case 1 {} }`, pass: true },
  L_case_default: { wgsl: `switch L { case default {} }`, pass: true },
  L_case_expr_default: { wgsl: `switch L { case 1+1 {} default {} }`, pass: true },
  L_default_break: { wgsl: `switch L { default { break; } }`, pass: true },
  L_default_case_1_2: { wgsl: `switch L { default {} case 1, 2 {} }`, pass: true },
  L_default_case_1_break: { wgsl: `switch L { default {} case 1 { break; } }`, pass: true },
  L_default_case_1_case_2: { wgsl: `switch L { default {} case 1 {} case 2 {} }`, pass: true },
  L_default_case_1_colon_break: { wgsl: `switch L { default {} case 1: { break; } }`, pass: true },
  L_default_case_2_case_1: { wgsl: `switch L { default {} case 2 {} case 1 {} }`, pass: true },
  L_default_colon_break: { wgsl: `switch L { default: { break; } }`, pass: true },
  L_default_colon: { wgsl: `switch L { default: {} }`, pass: true },

  L_no_block: { wgsl: `switch L`, pass: false },
  L_empty_block: { wgsl: `switch L {}`, pass: false },
  L_no_default: { wgsl: `switch L { case 1 {} }`, pass: false },
  L_default_default: { wgsl: `switch L { default, default {} }`, pass: false },
  L_default_block_default_block: { wgsl: `switch L { default {} default {} }`, pass: false },
  L_case_1_case_1_default: { wgsl: `switch L { case 1 {} case 1 {} default {} }`, pass: false },
  L_case_C1_case_C1_default: { wgsl: `switch L { case C1 {} case C1 {} default {} }`, pass: false },
  L_case_C2_case_expr_default: {
    wgsl: `switch L { case C2 {} case 1+1 {} default {} }`,
    pass: false,
  },
  L_default_1: { wgsl: `switch L { default, 1 {} }`, pass: false },
  L_default_2_case_1: { wgsl: `switch L { default, 2 {} case 1 {} }`, pass: false },

  no_cond: { wgsl: `switch { default{} }`, pass: false },
  no_cond_no_block: { wgsl: `switch;`, pass: false },
  lparen_L: { wgsl: `switch (L { default {}}`, pass: false },
  L_lparen: { wgsl: `switch L) { default {}}`, pass: false },
  lparen_L_lparen: { wgsl: `switch )L) { default {}}`, pass: false },
  rparen_L_rparen: { wgsl: `switch (L( { default {}}`, pass: false },
};

g.test('parse')
  .desc(`Test that 'switch' statements are parsed correctly.`)
  .params(u => u.combine('test', keysOf(kTests)))
  .fn(t => {
    const code = `
fn f() {
  let L = 1;
  const C1 = 1;
  const C2 = 2;
  ${kTests[t.params.test].wgsl}
}`;
    t.expectCompileResult(kTests[t.params.test].pass, code);
  });
