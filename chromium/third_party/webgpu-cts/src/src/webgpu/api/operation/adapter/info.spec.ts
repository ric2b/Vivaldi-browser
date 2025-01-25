export const description = `
Tests for GPUAdapterInfo.
`;

import { Fixture } from '../../../../common/framework/fixture.js';
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { keysOf } from '../../../../common/util/data_tables.js';
import { getGPU } from '../../../../common/util/navigator_gpu.js';
import { assert, objectEquals } from '../../../../common/util/util.js';

export const g = makeTestGroup(Fixture);

const normalizedIdentifierRegex = /^$|^[a-z0-9]+(-[a-z0-9]+)*$/;

g.test('adapter_info')
  .desc(
    `
  Test that every member in the GPUAdapter.info except description is properly formatted`
  )
  .fn(async t => {
    const gpu = getGPU(t.rec);
    const adapter = await gpu.requestAdapter();
    assert(adapter !== null);

    const adapterInfo = adapter.info;
    assert(adapterInfo instanceof GPUAdapterInfo);

    t.expect(
      normalizedIdentifierRegex.test(adapterInfo.vendor),
      `adapterInfo.vendor should be a normalized identifier. But it's '${adapterInfo.vendor}'`
    );

    t.expect(
      normalizedIdentifierRegex.test(adapterInfo.architecture),
      `adapterInfo.architecture should be a normalized identifier. But it's '${adapterInfo.architecture}'`
    );

    t.expect(
      normalizedIdentifierRegex.test(adapterInfo.device),
      `adapterInfo.device should be a normalized identifier. But it's '${adapterInfo.device}'`
    );
  });

g.test('same_object')
  .desc(
    `
GPUAdapter.info and GPUDevice.adapterInfo provide the same object each time they're accessed,
but different objects from one another.`
  )
  .fn(async t => {
    const gpu = getGPU(t.rec);
    const adapter = await gpu.requestAdapter();
    assert(adapter !== null);
    assert(adapter.info instanceof GPUAdapterInfo);

    const adapterInfo1 = adapter.info;
    const adapterInfo2 = adapter.info;
    t.expect(adapterInfo1 === adapterInfo2, 'adapter.info should obey [SameObject]');

    const device = await t.requestDeviceTracked(adapter);
    assert(device !== null);
    assert(device.adapterInfo instanceof GPUAdapterInfo);

    const deviceAdapterInfo1 = device.adapterInfo;
    const deviceAdapterInfo2 = device.adapterInfo;
    t.expect(
      deviceAdapterInfo1 === deviceAdapterInfo2,
      'device.adapterInfo should obey [SameObject]'
    );

    t.expect(
      adapter.info !== device.adapterInfo,
      'adapter.info and device.adapterInfo should NOT return the same object'
    );
  });

g.test('device_matches_adapter')
  .desc(
    `
Test that GPUDevice.adapterInfo matches GPUAdapter.info. Cases access the members in
different orders to make sure that they are consistent regardless of the access order.`
  )
  .paramsSubcasesOnly(u =>
    u.combine('testDeviceFirst', [true, false]).combine('testMembersFirst', [true, false])
  )
  .fn(async t => {
    const { testDeviceFirst, testMembersFirst } = t.params;

    const gpu = getGPU(t.rec);
    const adapter = await gpu.requestAdapter();
    assert(adapter !== null);

    const device = await t.requestDeviceTracked(adapter);
    assert(device !== null);

    const deviceInfo: unknown[] = [];
    const adapterInfo: unknown[] = [];

    const kGPUAdapterInfoKeys = keysOf(GPUAdapterInfo.prototype);
    if (testMembersFirst) {
      if (testDeviceFirst) {
        assert(device.adapterInfo instanceof GPUAdapterInfo);
        for (const k of kGPUAdapterInfoKeys) {
          deviceInfo.push(device.adapterInfo[k]);
        }
        assert(adapter.info instanceof GPUAdapterInfo);
        for (const k of kGPUAdapterInfoKeys) {
          adapterInfo.push(adapter.info[k]);
        }
      } else {
        assert(adapter.info instanceof GPUAdapterInfo);
        for (const k of kGPUAdapterInfoKeys) {
          adapterInfo.push(adapter.info[k]);
        }
        assert(device.adapterInfo instanceof GPUAdapterInfo);
        for (const k of kGPUAdapterInfoKeys) {
          deviceInfo.push(device.adapterInfo[k]);
        }
      }
    } else {
      if (testDeviceFirst) {
        assert(device.adapterInfo instanceof GPUAdapterInfo);
        assert(adapter.info instanceof GPUAdapterInfo);
        for (const k of kGPUAdapterInfoKeys) {
          deviceInfo.push(device.adapterInfo[k]);
          adapterInfo.push(adapter.info[k]);
        }
      } else {
        assert(adapter.info instanceof GPUAdapterInfo);
        assert(device.adapterInfo instanceof GPUAdapterInfo);
        for (const k of kGPUAdapterInfoKeys) {
          adapterInfo.push(adapter.info[k]);
          deviceInfo.push(device.adapterInfo[k]);
        }
      }
      t.expect(objectEquals(deviceInfo, adapterInfo));
    }
  });
