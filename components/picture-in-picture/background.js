chrome.runtime.onMessage.addListener((message, sender, callback) => {
  if (message.method === "getCurrentId") {
    // MessageSender contains a Tab object.
    callback({tabId: sender.tab.id});
  }
});
