export const description = `Validation tests for 'return' statements'`;

import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { isConvertible, scalarTypeOf, Type } from '../../../util/conversion.js';
import { ShaderValidationTest } from '../shader_validation_test.js';

export const g = makeTestGroup(ShaderValidationTest);

const kTestTypesNoAbstract = [
  'bool',
  'i32',
  'u32',
  'f32',
  'f16',
  'vec2f',
  'vec3h',
  'vec4u',
  'vec3b',
  'mat2x3f',
  'mat4x2h',
] as const;

const kTestTypes = [
  ...kTestTypesNoAbstract,
  'abstract-int',
  'abstract-float',
  'vec2af',
  'vec3af',
  'vec4af',
  'vec2ai',
  'vec3ai',
  'vec4ai',
] as const;

g.test('return_missing_value')
  .desc(`Tests that a 'return' must have a value if the function has a return type`)
  .params(u => u.combine('type', [...kTestTypesNoAbstract, undefined]))
  .beforeAllSubcases(t => {
    if (t.params.type !== undefined && scalarTypeOf(Type[t.params.type]).kind) {
      t.selectDeviceOrSkipTestCase('shader-f16');
    }
  })
  .fn(t => {
    const type = t.params.type ? Type[t.params.type] : undefined;
    const enable = type && scalarTypeOf(type).kind === 'f16' ? 'enable f16;' : '';
    const code = `
${enable}

fn f()${type ? `-> ${type}` : ''} {
  return;
}
`;

    const pass = type === undefined;
    t.expectCompileResult(pass, code);
  });

g.test('return_unexpected_value')
  .desc(`Tests that a 'return' must not have a value if the function has no return type`)
  .params(u => u.combine('type', [...kTestTypes, undefined]))
  .beforeAllSubcases(t => {
    if (t.params.type !== undefined && scalarTypeOf(Type[t.params.type]).kind) {
      t.selectDeviceOrSkipTestCase('shader-f16');
    }
  })
  .fn(t => {
    const type = t.params.type ? Type[t.params.type] : undefined;
    const enable = type && scalarTypeOf(type).kind === 'f16' ? 'enable f16;' : '';
    const code = `
${enable}

fn f() {
  return ${type ? `${type.create(1).wgsl()}` : ''};
}
`;

    const pass = type === undefined;
    t.expectCompileResult(pass, code);
  });

g.test('return_type_match')
  .desc(`Tests that a 'return' value type must match the function return type`)
  .params(u =>
    u.combine('return_value_type', kTestTypes).combine('fn_return_type', kTestTypesNoAbstract)
  )
  .beforeAllSubcases(t => {
    if (
      scalarTypeOf(Type[t.params.return_value_type]).kind === 'f16' ||
      scalarTypeOf(Type[t.params.fn_return_type]).kind === 'f16'
    ) {
      t.selectDeviceOrSkipTestCase('shader-f16');
    }
  })
  .fn(t => {
    const returnValueType = Type[t.params.return_value_type];
    const fnReturnType = Type[t.params.fn_return_type];
    const enable =
      scalarTypeOf(returnValueType).kind === 'f16' || scalarTypeOf(fnReturnType).kind === 'f16'
        ? 'enable f16;'
        : '';
    const code = `
${enable}

fn f() -> ${fnReturnType} {
  return ${returnValueType.create(1).wgsl()};
}
`;

    const pass = isConvertible(returnValueType, fnReturnType);
    t.expectCompileResult(pass, code);
  });
