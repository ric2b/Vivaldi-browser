// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved
import {addWebUiListener, sendWithPromise} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

// Communication between JS and c++ layer
export function isAdBlockEnabled() {
  return sendWithPromise('isAdBlockEnabled');
}
export function isTrackerBlockEnabled() {
  return sendWithPromise('isTrackerBlockEnabled');
}
export function getBlockingData(interval) {
  return sendWithPromise('getBlockingData', interval);
}
export function openLinkInNewTab(link) {
     sendWithPromise('openLinkInNewTab', link);
}
export function closeActivityFromJS() {
     sendWithPromise('closeActivityFromJS');
}

window.closeActivityFromJS=closeActivityFromJS
window.openLinkInNewTab=openLinkInNewTab;
// End communication functions

// Generate the list of trackers/ads blocked per site into the HTML
export function createSiteDivs(websitesArray) {
  websitesArray.sort((function(index) {
    return function(a, b) {
      return (a[index] === b[index] ? 0 : (a[index] > b[index] ? -1 : 1));
    };
  })(0));
  const tableWebsites = document.querySelector('.table.websites');
  const divs = tableWebsites.querySelectorAll('div');
  divs.forEach((div) => {
    if (!div.classList.contains('th')) {
      div.remove();
    }
  });
  for (let entry in websitesArray) {
    var sitenameDiv = document.createElement('div');
    var trackersDiv = document.createElement('div');
    var adsDiv = document.createElement('div');

    sitenameDiv.classList.add('sitename');
    sitenameDiv.innerHTML = `${websitesArray[entry][0]}`;

    trackersDiv.classList.add('siteAds');
    trackersDiv.innerHTML = `${websitesArray[entry][1]}`;

    adsDiv.classList.add('siteTrackers');
    adsDiv.innerHTML = `${websitesArray[entry][2]}`;

    document.getElementsByClassName('table websites')[0].appendChild(
        sitenameDiv);
    document.getElementsByClassName('table websites')[0].appendChild(
        trackersDiv);
    document.getElementsByClassName('table websites')[0].appendChild(adsDiv);
  }
}

// Generate the list of trackers blocked into the HTML
export function createTrackerDivs(websitesArray) {
  websitesArray.sort((function(index) {
    return function(a, b) {
      return (a[index] === b[index] ? 0 : (a[index] > b[index] ? -1 : 1));
    };
  })(2));
  const tableWebsites = document.querySelector('.table.trackers');
  const divs = tableWebsites.querySelectorAll('div');
  divs.forEach((div) => {
    if (!div.classList.contains('th')) {
      div.remove();
    }
  });
  for (let entry in websitesArray) {
    var sitenameDiv = document.createElement('div');
    var trackersDiv = document.createElement('div');
    var adsDiv = document.createElement('div');

    sitenameDiv.classList.add('sitename');
    sitenameDiv.innerHTML = `${websitesArray[entry][0]}`;

    trackersDiv.classList.add('siteAds');
    trackersDiv.innerHTML = `${websitesArray[entry][1]}`;

    adsDiv.classList.add('siteTrackers');
    adsDiv.innerHTML = `${websitesArray[entry][2]}`;

    document.getElementsByClassName('table trackers')[0].appendChild(
        sitenameDiv);
    document.getElementsByClassName('table trackers')[0].appendChild(
        trackersDiv);
    document.getElementsByClassName('table trackers')[0].appendChild(adsDiv);
  }
}

export function formatTime(seconds) {
  if (seconds <= 60) {
    return seconds.toFixed(2) + ' s';
  }
  const hours = Math.floor(seconds / 3600);
  const minutes = Math.floor((seconds % 3600) / 60);

  // Pad with leading zeros if necessary
  const paddedHours = String(hours).padStart(2, '0');
  const paddedMinutes = String(minutes).padStart(2, '0');
  return `${paddedHours} h ${paddedMinutes} m`;
}

export function formatData(megabytes) {
  if (megabytes <= 1000) {
    return `${megabytes.toFixed(2)} MB`;
  }
  // Convert megabytes to gigabytes
  const gigabytes = megabytes / 1024;

  const formattedGigabytes =
      gigabytes.toFixed(2);  // Format to 2 decimal places

  return `${formattedGigabytes} GB`;
}

// The main function for updating the site data
export async function updateState() {
  var interval = document.getElementById('timeperiod').value;
  var adBlockEnabled = await isAdBlockEnabled();
  var trackerBlockEnabled = await isTrackerBlockEnabled();
  var blockingData = await getBlockingData(interval);
  var adsBlocked = parseInt(blockingData[1]);
  var trackersBlocked = parseInt(blockingData[2]);
  var timeSaved = Math.round((trackersBlocked * 0.02  + adsBlocked * 0.02)* 100) / 100;
  var bandwidthSaved = Math.round((trackersBlocked * 15  + adsBlocked * 15) *100) / 100000.0;

  // Create the list for the ads blocked / tracker sites blocked
  createTrackerDivs(blockingData[3]);
  createSiteDivs(blockingData[4]);


  document.getElementById('timeSavedNumber').innerHTML = formatTime(timeSaved);
  document.getElementById('bandwidthNumber').innerHTML = formatData(bandwidthSaved);
  updateReport(
      adBlockEnabled, trackerBlockEnabled, adsBlocked, trackersBlocked)
}

// Update all visual elements based on the enabled Adblocking level
export function updateReport(
    adBlockEnabled, trackerBlockEnabled, sumAds, sumTrackers) {
  const mainShield = document.getElementById('mainShield');
  const notification = document.getElementById('notification');
  const pageHeader = document.getElementById('pageHeader');
  const adBox = document.getElementById('adBox');
  const trackerBox = document.getElementById('trackerBox');
  const bandwidthBox = document.getElementById('bandwidthBox');
  const timeBox = document.getElementById('timeBox');
  const adsNumber = document.getElementById('adsNumber');
  const trackerNumber = document.getElementById('trackerNumber');

  // Update common elements
  adsNumber.innerHTML = sumAds;
  trackerNumber.innerHTML = sumTrackers;
  notification.className = 'hidden notification';
  pageHeader.className = '';

  // Update shield icon based on enabled state
  if (adBlockEnabled && trackerBlockEnabled) {
    mainShield.src = 'img/report_icon_full.svg';
  } else if (adBlockEnabled || trackerBlockEnabled) {
    mainShield.src = 'img/report_icon_half.svg';
  } else {
    mainShield.src = 'img/report_icon_disabled.svg';
    notification.className = 'notification';
    pageHeader.className = 'disabled';
  }

  // Update box designs
  const adBoxClass = adBlockEnabled ? 'box blue' : 'box blue disabled';
  const trackerBoxClass =
      trackerBlockEnabled ? 'box blue' : 'box blue disabled';
  const bandwidthBoxClass = (adBlockEnabled || trackerBlockEnabled) ?
      'box green' :
      'box green disabled';
  const timeBoxClass = (adBlockEnabled || trackerBlockEnabled) ?
      'box green' :
      'box green disabled';

  adBox.className = adBoxClass;
  trackerBox.className = trackerBoxClass;
  bandwidthBox.className = bandwidthBoxClass;
  timeBox.className = timeBoxClass;
}

document.getElementById('timeperiod').addEventListener('change', updateState);
window.addEventListener('load', updateState);
document.body.style.display = 'block';