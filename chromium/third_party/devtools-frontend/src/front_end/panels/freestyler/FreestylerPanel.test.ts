// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Common from '../../core/common/common.js';
import * as Host from '../../core/host/host.js';
import {describeWithEnvironment, registerNoopActions} from '../../testing/EnvironmentHelpers.js';

import * as Freestyler from './freestyler.js';

function getTestAidaClient() {
  return {
    async *
        fetch() {
          yield {explanation: 'test', metadata: {}, completed: true};
        },
    registerClientEvent: sinon.spy(),
  };
}

function getTestSyncInfo(): Host.InspectorFrontendHostAPI.SyncInformation {
  return {isSyncActive: true};
}

describeWithEnvironment('FreestylerPanel', () => {
  const mockView = sinon.stub();

  beforeEach(() => {
    registerNoopActions(['elements.toggle-element-search']);
    mockView.reset();
  });

  describe('consent view', () => {
    it('should render consent view when the consent is not given before', async () => {
      Common.Settings.settingForTest('freestyler-dogfood-consent-onboarding-finished').set(false);

      new Freestyler.FreestylerPanel(mockView, {
        aidaClient: getTestAidaClient(),
        aidaAvailability: Host.AidaClient.AidaAccessPreconditions.AVAILABLE,
        syncInfo: getTestSyncInfo(),
      });

      sinon.assert.calledWith(mockView, sinon.match({state: Freestyler.State.CONSENT_VIEW}));
    });

    it('should set the setting to true and render chat view on accept click', async () => {
      const setting = Common.Settings.settingForTest('freestyler-dogfood-consent-onboarding-finished');
      setting.set(false);

      new Freestyler.FreestylerPanel(mockView, {
        aidaClient: getTestAidaClient(),
        aidaAvailability: Host.AidaClient.AidaAccessPreconditions.AVAILABLE,

        syncInfo: getTestSyncInfo(),
      });

      const callArgs = mockView.getCall(0).args[0];
      mockView.reset();
      callArgs.onAcceptConsentClick();

      assert.isTrue(setting.get());
      sinon.assert.calledWith(mockView, sinon.match({state: Freestyler.State.CHAT_VIEW}));
    });

    it('should render chat view when the consent is given before', async () => {
      Common.Settings.settingForTest('freestyler-dogfood-consent-onboarding-finished').set(true);

      new Freestyler.FreestylerPanel(mockView, {
        aidaClient: getTestAidaClient(),
        aidaAvailability: Host.AidaClient.AidaAccessPreconditions.AVAILABLE,
        syncInfo: getTestSyncInfo(),
      });

      sinon.assert.calledWith(mockView, sinon.match({state: Freestyler.State.CHAT_VIEW}));
    });
  });

  describe('on rate click', () => {
    beforeEach(() => {
      Common.Settings.settingForTest('freestyler-dogfood-consent-onboarding-finished').set(true);
    });

    afterEach(() => {
      // @ts-expect-error global test variable
      setFreestylerServerSideLoggingEnabled(false);
    });

    it('should allow logging if configured', () => {
      // @ts-expect-error global test variable
      setFreestylerServerSideLoggingEnabled(true);

      const aidaClient = getTestAidaClient();
      new Freestyler.FreestylerPanel(mockView, {
        aidaClient,
        aidaAvailability: Host.AidaClient.AidaAccessPreconditions.AVAILABLE,
        syncInfo: getTestSyncInfo(),
      });
      const callArgs = mockView.getCall(0).args[0];
      mockView.reset();
      callArgs.onFeedbackSubmit(0, Host.AidaClient.Rating.POSITIVE);

      sinon.assert.match(aidaClient.registerClientEvent.firstCall.firstArg, sinon.match({
        disable_user_content_logging: false,
      }));
    });

    it('should send POSITIVE rating to aida client when the user clicks on positive rating', () => {
      const RPC_ID = 0;

      const aidaClient = getTestAidaClient();
      new Freestyler.FreestylerPanel(mockView, {
        aidaClient,
        aidaAvailability: Host.AidaClient.AidaAccessPreconditions.AVAILABLE,
        syncInfo: getTestSyncInfo(),
      });
      const callArgs = mockView.getCall(0).args[0];
      mockView.reset();
      callArgs.onFeedbackSubmit(RPC_ID, Host.AidaClient.Rating.POSITIVE);

      sinon.assert.match(aidaClient.registerClientEvent.firstCall.firstArg, sinon.match({
        corresponding_aida_rpc_global_id: RPC_ID,
        do_conversation_client_event: {
          user_feedback: {
            sentiment: 'POSITIVE',
          },
        },
        disable_user_content_logging: true,
      }));
    });

    it('should send NEGATIVE rating to aida client when the user clicks on positive rating', () => {
      const RPC_ID = 0;
      const aidaClient = getTestAidaClient();
      new Freestyler.FreestylerPanel(mockView, {
        aidaClient,
        aidaAvailability: Host.AidaClient.AidaAccessPreconditions.AVAILABLE,
        syncInfo: getTestSyncInfo(),
      });
      const callArgs = mockView.getCall(0).args[0];
      mockView.reset();
      callArgs.onFeedbackSubmit(RPC_ID, Host.AidaClient.Rating.NEGATIVE);

      sinon.assert.match(aidaClient.registerClientEvent.firstCall.firstArg, sinon.match({
        corresponding_aida_rpc_global_id: RPC_ID,
        do_conversation_client_event: {
          user_feedback: {
            sentiment: 'NEGATIVE',
          },
        },
        disable_user_content_logging: true,
      }));
    });

    it('should send feedback text with data', () => {
      const RPC_ID = 0;
      const feedback = 'This helped me a ton.';
      const aidaClient = getTestAidaClient();
      new Freestyler.FreestylerPanel(mockView, {
        aidaClient,
        aidaAvailability: Host.AidaClient.AidaAccessPreconditions.AVAILABLE,
        syncInfo: getTestSyncInfo(),
      });
      const callArgs = mockView.getCall(0).args[0];
      mockView.reset();
      callArgs.onFeedbackSubmit(RPC_ID, Host.AidaClient.Rating.POSITIVE, feedback);

      sinon.assert.match(aidaClient.registerClientEvent.firstCall.firstArg, sinon.match({
        corresponding_aida_rpc_global_id: RPC_ID,
        do_conversation_client_event: {
          user_feedback: {
            sentiment: 'POSITIVE',
            user_input: {
              comment: feedback,
            },
          },
        },
        disable_user_content_logging: true,
      }));
    });
  });
});
