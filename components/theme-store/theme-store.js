// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved
//
// chrome-extension://lglfeioladcfajpjdnghbfgohdihdnfl/

const debug = true;
const vivaldi = window.vivaldi;

function LOG(str) {
  if (debug) {
    console.log(str);
  }
}

class ThemeStore {
  constructor() {
    this.downloadUrl = "";
    this.themeId = "";
    this.themeSize = 1024 * 1024;  // 1MB default size
    this.themeVersion = "";
    this.installing = false;
    this.installButton = undefined;
    this.updateButton = undefined;
    this.isInstalledButton = undefined;
    this.themeData = null;
    this.ratingForm = undefined;
  }
  inject() {
    this.installHooks();
    this.checkThemeStatus();
  }
  constructDownloadUrl(buttonElm) {
    const url = buttonElm.href;

    return url;
  }
  checkThemeStatus() {
    if (!this.themeId) {
      return;
    }
    vivaldi.themePrivate.getThemeData(this.themeId, (themeData) => {
      if (this.themeData != themeData) {
        this.updateButtons(themeData);
        this.themeData = themeData;
      }
    });
  }
  updateButtons(themeData) {
    if (themeData.isInstalled) {
      this.installButton.classList.remove("hidden");
      if (themeData.version != this.themeVersion) {
        if (this.updateButton) {
          this.updateButton.classList.remove("hidden");
          this.installButton.classList.add("hidden");
          this.isInstalledButton?.classList.add("hidden");
        }
      } else if (this.isInstalledButton) {
        this.isInstalledButton.classList.remove("hidden");
        this.updateButton.classList.add("hidden");
        this.installButton.classList.add("hidden");
      }
      this.ratingForm?.classList.remove("hidden");
    } else {
      this.installButton.classList.remove("hidden");
      this.updateButton?.classList.add("hidden");
      this.isInstalledButton?.classList.add("hidden");
      this.ratingForm?.classList.add("hidden");
    }
  }
  installHooks() {
    // <meta id="themeUuid" content="a8790245-cf0b-46cc-8526-706b001a92c6">
    let metaElm = document.getElementById("themeUuid");
    if (metaElm && metaElm.content) {
      this.themeId = metaElm.content;
      LOG("Theme uuid detected: " + this.themeId);
      // <a id="installButton" itemprop="contentUrl" href="/download/2ZQDJn9vLBz/3/Vox%20Machina.zip" class="btn install">Install theme</a>
      this.installButton =  document.getElementById("installButton");
      if (this.installButton) {
        this.downloadUrl = this.constructDownloadUrl(this.installButton);
        this.installButton.addEventListener('click', this.installButtonClicked);
        this.addEventHandlers();

        // <a id="updateButton" href="https://themes.vivaldi.net/install/40y7QGwvXxM" class="btn install hidden">Update theme</a>
        this.updateButton = document.getElementById("updateButton");
        if (this.updateButton) {
          this.updateButton.addEventListener('click', this.installButtonClicked);
        }
        // <a id="installedButton" class="btn btn-outline disabled hidden">Installed</a>
        this.isInstalledButton = document.getElementById("installedButton");

        // <meta id="themeFilesize" content="1231231">
        metaElm = document.getElementById("themeFilesize");
        if (metaElm && metaElm.content) {
          this.themeSize = parseInt(metaElm.content, 10);
          LOG("Theme size: " + this.themeSize);
        }
        // <meta id="themeVersion" content="2">
        metaElm = document.getElementById("themeVersion");
        if (metaElm && metaElm.content) {
          this.themeVersion = metaElm.content;
          LOG("Theme version: " + this.themeVersion);
        }
      }
      this.ratingForm  = document.getElementById("ratingform");
    }
  }
  addEventHandlers() {
    const tp = vivaldi.themePrivate;
    tp.onThemeDownloadStarted.addListener(this.onDownloadStarted);
    tp.onThemeDownloadProgress.addListener(this.onDownloadProgress);
    tp.onThemeDownloadCompleted.addListener(this.onDownloadCompleted);
    tp.onThemesUpdated.addListener(this.onThemesUpdated);
  }
  installButtonClicked = (event) => {
    if (!this.installing) {
      LOG("Download url: " + this.downloadUrl);
      if (this.downloadUrl) {
        this.installing = true;
        vivaldi.themePrivate.download(this.themeId, this.downloadUrl,
          () => {

        });
      }
    }
    event.stopPropagation();
    event.preventDefault();
  }
  onThemesUpdated = () => {
    this.checkThemeStatus();
  }
  onDownloadStarted = (themeId) => {
    if (this.themeId !== themeId)
      return;

    if (this.installButton) {
      // Save the text as we remove it to fit the progress bar.
      this.installButtonText = this.installButton.innerText;
      this.installButton.innerText = "";
      if (!this.progressElm) {
        this.progressElm = document.createElement("progress");
        this.installButton.appendChild(this.progressElm);
        this.progressElm.max = this.themeSize;
        this.progressElm.value = 0;
        this.progressElm.style = "max-width: 80px"
      }
    }
  }
  onDownloadProgress = (themeId, progress) => {
    if (this.themeId !== themeId)
      return;

    if (this.progressElm) {
      this.progressElm.value = progress;
    }
  }
  onDownloadCompleted = (themeId, success, error) => {
    if (this.themeId !== themeId)
      return;

    this.installing = false;
    if (this.installButton) {
      this.installButton.innerText = this.installButtonText;
      if (this.progressElm.parentNode) {
        this.progressElm.parentNode.removeChild(this.progressElm);
      }
      this.progressElm = undefined;
    }
    if (!success) {
      console.warn("Error downloading theme: " + themeId + ", error: " + error);
    }
  }
};

new ThemeStore().inject();
