"use strict";

const {EventEmitter} = require("../../lib/events");

let listeners = [];
let eventEmitter = new EventEmitter();

let Prefs = exports.Prefs = {
  enabled: true,
  savestats: true,
  subscriptions_autoupdate: true,
  subscriptions_fallbackerrors: 5,
  subscriptions_fallbackurl: "",
  notificationurl: "https://example.com/notification.json",
  notificationdata: {},
  notifications_ignoredcategories: [],
  blocked_total: 10,
  show_statsinpopup: true
};

for (let key of Object.keys(Prefs))
{
  let value = Prefs[key];
  Object.defineProperty(Prefs, key, {
    get()
    {
      return value;
    },
    set(newValue)
    {
      if (newValue == value)
        return;

      value = newValue;
      for (let listener of listeners)
        listener(key);

      eventEmitter.emit(key);
    }
  });
}

Prefs.addListener = function(listener)
{
  if (listeners.indexOf(listener) < 0)
    listeners.push(listener);
};

Prefs.removeListener = function(listener)
{
  let index = listeners.indexOf(listener);
  if (index >= 0)
    listeners.splice(index, 1);
};

Prefs.on = eventEmitter.on.bind(eventEmitter);
Prefs.off = eventEmitter.off.bind(eventEmitter);
