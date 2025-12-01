# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import textwrap

from PB.go.chromium.org.luci.common.proto.findings import findings as findings_pb
from recipe_engine import post_process


PYTHON_VERSION_COMPATIBILITY = 'PY3'

DEPS = [
    'gclient',
    'presubmit',
    'recipe_engine/buildbucket',
    'recipe_engine/context',
    'recipe_engine/cq',
    'recipe_engine/json',
    'recipe_engine/path',
    'recipe_engine/properties',
    'recipe_engine/proto',
    'recipe_engine/runtime',
]


def RunSteps(api):
  api.gclient.set_config('infra')
  with api.context(cwd=api.path.cache_dir / 'builder'):
    bot_update_step = api.presubmit.prepare()
    skip_owners = api.properties.get('skip_owners', False)
    run_all = api.properties.get('run_all', False)
    return api.presubmit.execute(bot_update_step, skip_owners, run_all)


def GenTests(api):
  yield api.test(
      'success_ci',
      api.buildbucket.ci_build(
          git_repo='https://chromium.googlesource.com/infra/infra'),
      api.properties(run_all=True),
      api.step_data('presubmit', api.json.output({})),
      api.post_process(post_process.StatusSuccess),
      api.post_process(post_process.DropExpectation),
  )

  yield (api.test('success') + api.runtime(is_experimental=False) +
         api.buildbucket.try_build(project='infra') + api.step_data(
             'presubmit',
             api.json.output({
                 'errors': [],
                 'notifications': [],
                 'warnings': []
             }),
         ) + api.post_process(post_process.StatusSuccess) +
         api.post_process(post_process.DropExpectation))

  yield (api.test('cq_dry_run') + api.runtime(is_experimental=False) +
         api.buildbucket.try_build(project='infra') +
         api.cq(run_mode=api.cq.DRY_RUN) +
         api.post_process(post_process.StatusSuccess) + api.post_process(
             post_process.StepCommandContains, 'presubmit', ['--dry_run']) +
         api.post_process(post_process.DropExpectation))

  yield (api.test('skip_owners') + api.runtime(is_experimental=False) +
         api.buildbucket.try_build(project='infra') +
         api.properties(skip_owners=True) +
         api.post_process(post_process.StatusSuccess) +
         api.post_process(post_process.StepCommandContains, 'presubmit',
                          ['--skip_canned', 'CheckOwners']) +
         api.post_process(post_process.DropExpectation))

  yield api.test(
      'timeout',
      api.runtime(is_experimental=False),
      api.buildbucket.try_build(project='infra'),
      api.presubmit(timeout_s=600),
      api.step_data(
          'presubmit',
          api.json.output({
              'errors': [],
              'notifications': [],
              'warnings': []
          }),
          times_out_after=1200,
      ),
      api.post_process(post_process.StatusFailure),
      api.post_process(
          post_process.SummaryMarkdown,
          (u'#### There are 0 error(s), 0 warning(s), and 0 notifications(s).'
           '\n\nTimeout occurred during presubmit step.')),
      api.post_process(post_process.DropExpectation),
      status="FAILURE")

  yield api.test(
      'failure',
      api.runtime(is_experimental=False),
      api.buildbucket.try_build(project='infra'),
      api.step_data(
          'presubmit',
          api.json.output(
              {
                  'errors': [{
                      'message': 'Missing LGTM',
                      'long_text': 'Here are some suggested OWNERS: fake@',
                      'items': [],
                      'fatal': True
                  }, {
                      'message': 'Syntax error in fake.py',
                      'long_text': 'Expected "," after item in list',
                      'items': [],
                      'fatal': True
                  }],
                  'notifications': [{
                      'message': 'If there is a bug associated please add it.',
                      'long_text': '',
                      'items': [],
                      'fatal': False
                  }],
                  'warnings': [{
                      'message': 'Line 100 has more than 80 characters',
                      'long_text': '',
                      'items': [],
                      'fatal': False
                  }]
              },
              retcode=1)),
      api.post_process(post_process.StatusFailure),
      api.post_process(
          post_process.SummaryMarkdown,
          textwrap.dedent(u'''
          #### There are 2 error(s), 1 warning(s), and 1 notifications(s).

          **ERROR**
          ```
          Missing LGTM
          Here are some suggested OWNERS: fake@
          ```

          **ERROR**
          ```
          Syntax error in fake.py
          Expected "," after item in list
          ```

          #### To see notifications and warnings, look at the stdout of the presubmit step.
          ''').strip()),
      api.post_process(post_process.DropExpectation),
      status="FAILURE")

  long_message = (u'Here are some suggested OWNERS:' +
    u'\nreallyLongFakeAccountNameEmail@chromium.org' * 10)
  yield api.test(
      'failure-long-message',
      api.runtime(is_experimental=False),
      api.buildbucket.try_build(project='infra'),
      api.step_data(
          'presubmit',
          api.json.output(
              {
                  'errors': [{
                      'message': 'Missing LGTM',
                      'long_text': long_message,
                      'items': [],
                      'fatal': True
                  }],
                  'notifications': [],
                  'warnings': []
              },
              retcode=1)),
      api.post_process(post_process.StatusFailure),
      api.post_process(
          post_process.SummaryMarkdown,
          textwrap.dedent('''
          #### There are 1 error(s), 0 warning(s), and 0 notifications(s).

          **ERROR**
          ```
          Missing LGTM
          Here are some suggested OWNERS:
          reallyLongFakeAccountNameEmail@chromium.org
          reallyLongFakeAccountNameEmail@chromium.org
          reallyLongFakeAccountNameEmail@chromium.org
          reallyLongFakeAccountNameEmail@chromium.org
          reallyLongFakeAccountNameEmail@chromium.org
          reallyLongFakeAccountNameEmail@chromium.org
          reallyLongFakeAccountNameEmail@chromium.org
          reallyLongFakeAccountNameEmail@chromium.org
          reallyLongFakeAccountNameEmail@chromium.org
          ```

          **Error size > 450 chars, there are 2 more error(s) (15 total)**
          **The complete output can be found at the bottom of the presubmit stdout.**
          ''').strip()),
      api.post_process(post_process.DropExpectation),
      status="FAILURE")

  yield api.test(
      'infra-failure',
      api.runtime(is_experimental=False),
      api.buildbucket.try_build(project='infra'),
      api.step_data(
          'presubmit',
          api.json.output(
              {
                  'errors': [{
                      'message': 'Infra Failure',
                      'long_text': '',
                      'items': [],
                      'fatal': True
                  }],
                  'notifications': [],
                  'warnings': []
              },
              retcode=2)),
      api.post_process(post_process.StatusException),
      api.post_process(
          post_process.SummaryMarkdown,
          textwrap.dedent(u'''
          #### There are 1 error(s), 0 warning(s), and 0 notifications(s).

          **ERROR**
          ```
          Infra Failure

          ```
          ''').strip()),
      api.post_process(post_process.DropExpectation),
      status="INFRA_FAILURE")

  bug_msg = ('Something unexpected occurred'
             ' while running presubmit checks.'
             ' Please [file a bug](https://issues.chromium.org'
             '/issues/new?component=1456211)')
  yield api.test(
      'failure-no-json',
      api.runtime(is_experimental=False),
      api.buildbucket.try_build(project='infra'),
      api.step_data('presubmit', api.json.output(None, retcode=1)),
      api.post_process(post_process.StatusException),
      api.post_process(post_process.SummaryMarkdown, bug_msg),
      api.post_process(post_process.DropExpectation),
      status="INFRA_FAILURE")

  yield api.test(
      'infra-failure-no-json',
      api.runtime(is_experimental=False),
      api.buildbucket.try_build(project='infra'),
      api.step_data('presubmit', api.json.output(None, retcode=2)),
      api.post_process(post_process.StatusException),
      api.post_process(post_process.SummaryMarkdown, bug_msg),
      api.post_process(post_process.DropExpectation),
      status="INFRA_FAILURE")

  def _has_uploaded_findings(check, steps, expected_findings):
    step_name = 'upload presubmit results as findings'
    check(step_name in steps)
    check('findings.json' in steps[step_name].logs)
    findings = api.proto.decode(steps[step_name].logs['findings.json'],
                                findings_pb.Findings, 'JSONPB').findings
    check(len(findings) == len(expected_findings))
    for (actual, expected) in zip(findings, expected_findings):
      check(actual == expected)

  gerrit_change_ref = findings_pb.Location.GerritChangeReference(
      host='chromium-review.googlesource.com',
      project='infra',
      change=123456,
      patchset=7)

  yield api.test(
      'upload_findings',
      api.runtime(is_experimental=False),
      api.buildbucket.try_build(project='infra'),
      api.step_data(
          'presubmit',
          api.json.output(
              {
                  'errors': [{
                      'message':
                      'bug bug bug',
                      'long_text':
                      '',
                      'items': [],
                      'locations': [{
                          'file_path': 'path/to/file1',
                      }, {
                          'file_path': 'path/to/file2',
                      }],
                      'fatal':
                      True
                  }],
                  'notifications':
                  [{
                      'message': 'cc abc@google.com',
                      'long_text': '',
                      'items': [],
                      'locations': [{
                          'file_path': '/COMMIT_MSG',
                      }],
                      'fatal': False
                  }],
                  'warnings': [{
                      'message':
                      'Change takes 20 min to take effect after landing',
                      'long_text': '',
                      'items': [],
                      'locations': [],
                      'fatal': False
                  }, {
                      'message':
                      'typo!!',
                      'long_text':
                      'replace foo with bar',
                      'items': [],
                      'locations': [{
                          'file_path': 'path/to/file',
                          'start_line': 1,
                          'start_col': 2,
                          'end_line': 1,
                          'end_col': 5,
                      }],
                      'fatal':
                      False
                  }]
              },
              retcode=1)),
      api.post_process(
          _has_uploaded_findings,
          [
              findings_pb.Finding(
                  category='chromium_presubmit',
                  location=findings_pb.Location(
                      gerrit_change_ref=gerrit_change_ref,
                      file_path='path/to/file1'),
                  message='bug bug bug',
                  severity_level=findings_pb.Finding.SEVERITY_LEVEL_ERROR),
              findings_pb.Finding(
                  category='chromium_presubmit',
                  location=findings_pb.Location(
                      gerrit_change_ref=gerrit_change_ref,
                      file_path='path/to/file2'),
                  message='bug bug bug',
                  severity_level=findings_pb.Finding.SEVERITY_LEVEL_ERROR),
              findings_pb.Finding(
                  category='chromium_presubmit',
                  location=findings_pb.Location(
                      gerrit_change_ref=gerrit_change_ref,
                      file_path='path/to/file',
                      range=findings_pb.Location.Range(
                          start_line=1,
                          start_column=2,
                          end_line=1,
                          end_column=5,
                      )),
                  message='typo!!',
                  severity_level=findings_pb.Finding.SEVERITY_LEVEL_WARNING),
              findings_pb.Finding(
                  category='chromium_presubmit',
                  location=findings_pb.Location(
                      gerrit_change_ref=gerrit_change_ref,
                      file_path='/COMMIT_MSG'),
                  message='cc abc@google.com',
                  severity_level=findings_pb.Finding.SEVERITY_LEVEL_INFO),
          ],
      ),
      api.post_process(post_process.StatusFailure),
      api.post_process(post_process.DropExpectation),
      status="FAILURE")

  yield api.test(
      'upload_findings_when_presubmit_succeeds',
      api.runtime(is_experimental=False),
      api.buildbucket.try_build(project='infra'),
      api.step_data(
          'presubmit',
          api.json.output({
              'errors': [],
              'notifications': [],
              'warnings': [{
                  'message': 'this is a message',
                  'long_text': '',
                  'items': [],
                  'locations': [{
                      'file_path': 'path/to/file',
                  }],
                  'fatal': False
              }]
          })),
      api.post_process(
          _has_uploaded_findings,
          [
              findings_pb.Finding(
                  category='chromium_presubmit',
                  location=findings_pb.Location(
                      gerrit_change_ref=gerrit_change_ref,
                      file_path='path/to/file'),
                  message='this is a message',
                  severity_level=findings_pb.Finding.SEVERITY_LEVEL_WARNING),
          ],
      ), api.post_process(post_process.StatusSuccess),
      api.post_process(post_process.DropExpectation))
