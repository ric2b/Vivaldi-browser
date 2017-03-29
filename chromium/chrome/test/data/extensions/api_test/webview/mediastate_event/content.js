onload = function () {

    var webView = document.getElementById('webview');

    webView.onmediastatechanged = function (s) {
        chrome.test.succeed();
    };
}
