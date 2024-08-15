export const description = `Validation tests for 'while' statements'`;

import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { keysOf } from '../../../../common/util/data_tables.js';
import { ShaderValidationTest } from '../shader_validation_test.js';

import { kTestTypes } from './test_types.js';

export const g = makeTestGroup(ShaderValidationTest);

g.test('condition_type')
  .desc(`Tests that a 'while' condition must be a bool type`)
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
  while (${type.value}) {
    return true;
  }
  return false;
}
`;

    const pass = t.params.type === 'bool';
    t.expectCompileResult(pass, code);
  });
