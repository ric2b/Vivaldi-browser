// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

function setupEvents() {
  document.querySelector('#icon').classList.add('icon');

  const primaryButton = document.querySelector('#primary-button');
  primaryButton.addEventListener('click', function() {
        sendCommand(SecurityInterstitialCommandId.CMD_DONT_PROCEED);
  });

  const proceedButton = document.querySelector('#proceed-button');
  proceedButton.addEventListener('click', function(event) {
    sendCommand(SecurityInterstitialCommandId.CMD_PROCEED);
  });

  preventDefaultOnPoundLinkClicks();
}

document.addEventListener('DOMContentLoaded', setupEvents);
