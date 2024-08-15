export const description = `Validation tests for 'switch' statements'`;

import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { keysOf } from '../../../../common/util/data_tables.js';
import { Type } from '../../../util/conversion.js';
import { ShaderValidationTest } from '../shader_validation_test.js';

import { kTestTypes } from './test_types.js';

export const g = makeTestGroup(ShaderValidationTest);

g.test('condition_type')
  .desc(`Tests that a 'switch' condition must be of an integer type`)
  .params(u => u.combine('type', keysOf(kTestTypes)))
  .beforeAllSubcases(t => {
    if (kTestTypes[t.params.type].requires === 'f16') {
      t.selectDeviceOrSkipTestCase('shader-f16');
    }
  })
  .fn(t => {
    const type = kTestTypes[t.params.type];
    const code = `
${type.requires ? `enable ${type.requires};` : ''}

${type.header ?? ''}

fn f() -> bool {
  switch ${type.value} {
    case 1: {
      return true;
    }
    default: {
      return false;
    }
  }
}
`;

    const pass =
      t.params.type === 'i32' || t.params.type === 'u32' || t.params.type === 'abstract-int';
    t.expectCompileResult(pass, code);
  });

g.test('condition_type_match_case_type')
  .desc(`Tests that a 'switch' condition must have a common type with its case values`)
  .params(u =>
    u
      .combine('cond_type', ['i32', 'u32', 'abstract-int'] as const)
      .combine('case_type', ['i32', 'u32', 'abstract-int'] as const)
  )
  .fn(t => {
    const code = `
fn f() -> bool {
switch ${Type[t.params.cond_type].create(1).wgsl()} {
  case ${Type[t.params.case_type].create(2).wgsl()}: {
    return true;
  }
  default: {
    return false;
  }
}
}
`;

    const pass =
      t.params.cond_type === t.params.case_type ||
      t.params.cond_type === 'abstract-int' ||
      t.params.case_type === 'abstract-int';
    t.expectCompileResult(pass, code);
  });

g.test('case_types_match')
  .desc(`Tests that switch case types must have a common type`)
  .params(u =>
    u
      .combine('case_a_type', ['i32', 'u32', 'abstract-int'] as const)
      .combine('case_b_type', ['i32', 'u32', 'abstract-int'] as const)
  )
  .fn(t => {
    const code = `
fn f() -> bool {
switch 1 {
  case ${Type[t.params.case_a_type].create(1).wgsl()}: {
    return true;
  }
  case ${Type[t.params.case_b_type].create(2).wgsl()}: {
    return true;
  }
  default: {
    return false;
  }
}
}
`;

    const pass =
      t.params.case_a_type === t.params.case_b_type ||
      t.params.case_a_type === 'abstract-int' ||
      t.params.case_b_type === 'abstract-int';
    t.expectCompileResult(pass, code);
  });
