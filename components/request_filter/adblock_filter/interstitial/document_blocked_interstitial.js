// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

function setupEvents() {
  $('icon').classList.add('icon');

  $('primary-button').addEventListener('click', function() {
        sendCommand(SecurityInterstitialCommandId.CMD_DONT_PROCEED);
  });

  const proceedButton = 'proceed-button';
  $(proceedButton).classList.remove(HIDDEN_CLASS);

  $(proceedButton).textContent = loadTimeData.getString('proceedButtonText');

  $(proceedButton).addEventListener('click', function(event) {
    sendCommand(SecurityInterstitialCommandId.CMD_PROCEED);
  });

  preventDefaultOnPoundLinkClicks();
}

document.addEventListener('DOMContentLoaded', setupEvents);
