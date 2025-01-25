// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ClientDelegateFactory} from 'chrome-untrusted://boca-app/app/client_delegate.js';
import {Config, Course, Identity, PageHandlerRemote, Window} from 'chrome-untrusted://boca-app/mojom/boca.mojom-webui.js';
import {Url} from 'chrome-untrusted://resources/mojo/url/mojom/url.mojom-webui.js';
import {assertDeepEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

class MockRemoteHandler extends PageHandlerRemote {
  override getWindowsTabsList(): Promise<{windowList: Window[]}> {
    const url1 = new Url();
    url1.url = 'http://foo1';
    const url2 = new Url();
    url2.url = 'http://foo2';
    const url3 = new Url();
    url3.url = 'http://foo3';
    return Promise.resolve({
      windowList: [
        {
          name: 'window1',
          tabList: [
            {title: 'title1', url: url1, favicon: 'dataurl1'},
            {title: 'title2', url: url2, favicon: 'dataurl2'},
          ],
        },
        {tabList: [{title: 'title3', url: url3, favicon: 'dataurl3'}]},
      ] as Window[],
    });
  }
  override listCourses(): Promise<{courses: Course[]}> {
    return Promise.resolve(
        {courses: [{id: '1', name: 'course1'}, {id: '2', name: 'course2'}]});
  }
  override listStudents(id: string): Promise<{students: Identity[]}> {
    // Dummy action get around with unused variable check.
    id;
    return Promise.resolve({
      students: [
        {id: '1', name: 'cat', email: 'email1'},
        {id: '2', name: 'dog', email: 'email2'},
      ],
    });
  }

  override createSession(config: Config): Promise<{success: boolean}> {
    assertDeepEquals(
        {
          sessionDuration: {
            // BigInt serialized as string.
            microseconds: 7200000000n,
          },
          students: [
            {id: '1', name: 'cat', email: 'cat@gmail.com'},
            {id: '2', name: 'dog', email: 'dog@gmail.com'},
          ],
          onTaskConfig: {
            isLocked: true,
            tabs: [
              {
                tab: {
                  url: {url: 'http://google.com/'},
                  title: 'google',
                  favicon: 'data/image',
                },
                navigationType: 0,
              },
              {
                tab: {
                  url: {url: 'http://youtube.com/'},
                  title: 'youtube',
                  favicon: 'data/image',
                },
                navigationType: 1,
              },
            ],
          },
          captionConfig: {
            captionEnabled: true,
            transcriptionEnabled: true,
            localOnly: true,
          },
        },
        config);
    return Promise.resolve({success: true});
  }
}

suite('ClientDelegateTest', function() {
  let clientDelegateImpl: ClientDelegateFactory;

  setup(function() {
    clientDelegateImpl = new ClientDelegateFactory(new MockRemoteHandler());
  });

  test(
      'client delegate should properly translate mojom layer data for windows' +
          'list',
      async () => {
        const result =
            await clientDelegateImpl.getInstance().getWindowsTabsList();

        assertDeepEquals(
            [
              {
                windowName: 'window1',
                tabList: [
                  {title: 'title1', url: 'http://foo1', favicon: 'dataurl1'},
                  {title: 'title2', url: 'http://foo2', favicon: 'dataurl2'},
                ],
              },
              {
                // Default window name should be empty
                windowName: '',
                tabList: [
                  {title: 'title3', url: 'http://foo3', favicon: 'dataurl3'},
                ],
              },
            ],
            result);
      });

  test(
      'client delegate should properly translate mojom layer data for course' +
          'list',
      async () => {
        const result = await clientDelegateImpl.getInstance().getCourseList();
        assertDeepEquals(
            [
              {id: '1', name: 'course1', section: 'default'},
              {id: '2', name: 'course2', section: 'default'},
            ],
            result);
      });

  test(
      'client delegate should properly translate mojom layer data for student' +
          'list',
      async () => {
        const result =
            await clientDelegateImpl.getInstance().getStudentList('1');

        assertDeepEquals(
            [
              {id: '1', name: 'cat', email: 'email1'},
              {id: '2', name: 'dog', email: 'email2'},
            ],
            result);
      });

  test(
      'client delegate should translate data for creating session',
      async () => {
        const result = await clientDelegateImpl.getInstance().createSession({
          sessionDurationInMinutes: 120,
          students: [
            {id: '1', name: 'cat', email: 'cat@gmail.com'},
            {id: '2', name: 'dog', email: 'dog@gmail.com'},
          ],
          onTaskConfig: {
            isLocked: true,
            tabs: [
              {
                tab: {
                  title: 'google',
                  url: 'http://google.com/',
                  favicon: 'data/image',
                },
                navigationType: 0,
              },
              {
                tab: {
                  title: 'youtube',
                  url: 'http://youtube.com/',
                  favicon: 'data/image',
                },
                navigationType: 1,
              },
            ],
          },
          captionConfig: {
            captionEnabled: true,
            localOnly: true,
            transcriptionEnabled: true,
          },
        });
        assertTrue(result);
      });
});
