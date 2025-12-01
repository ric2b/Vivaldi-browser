// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved
import {preventDefaultOnPoundLinkClicks, SecurityInterstitialCommandId, sendCommand} from 'chrome://interstitials/common/resources/interstitial_common.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

function setupEvents() {
  loadTimeData.data = window.loadTimeDataRaw;

  const primaryButton = document.querySelector('#primary-button');
  primaryButton.addEventListener('click', function() {
        sendCommand(SecurityInterstitialCommandId.CMD_DONT_PROCEED);
  });

  const proceedButton = document.querySelector('#proceed-button');
  proceedButton.addEventListener('click', function(event) {
    sendCommand(SecurityInterstitialCommandId.CMD_PROCEED);
  });

  preventDefaultOnPoundLinkClicks();

 (( ) => {
  const reURL = /^https?:\/\//;

  const createListItem = function(name, value) {
      if ( value === '' ) {
          value = name;
          name = '';
      }
      const item = document.createElement('li');
      let name_span = document.createElement('span');
      if ( name !== '' && value !== '' ) {
        name += ': ';
      }
      name_span.innerText = name;
      item.appendChild(name_span);
      if ( reURL.test(value) ) {
          const related_url = document.createElement('a');
          related_url.href=value;
          related_url.innerText = value;
          item.appendChild(related_url);
      } else {
        item.appendChild(document.createTextNode(value))
      }
      return item;
  };

  // https://github.com/uBlockOrigin/uBlock-issues/issues/1649
  //   Limit recursion.
  const renderParams = function(parentNode, rawURL, depth = 0) {
      let url;
      try {
          url = new URL(rawURL);
      } catch {
          return false;
      }

      const search = url.search.slice(1);
      if ( search === '' ) { return false; }

      url.search = '';
      const list_item = createListItem(loadTimeData.getString('requestUrlWithoutParameters'), url.href);
      parentNode.appendChild(list_item);

      const params = new self.URLSearchParams(search);
      for ( const [ name, value ] of params ) {
          const list_item = createListItem(name, value);
          if ( depth < 2 && reURL.test(value) ) {
              const sub_list = document.createElement('ul');
              renderParams(sub_list, value, depth + 1);
              list_item.appendChild(sub_list);
          }
          parentNode.appendChild(list_item);
      }

      return true;
  };

  const request_url = loadTimeData.getString('requestUrl');

  if ( renderParams(document.querySelector('#url-details'), request_url) === false ) {
      return;
  }

  document.querySelector('#show-url-details').classList.remove('hidden');

  document.querySelector('#show-url-details').addEventListener("click", () => {
    document.querySelector('#url-details').classList.toggle('hidden')
  });

})();

}

document.addEventListener('DOMContentLoaded', setupEvents);
