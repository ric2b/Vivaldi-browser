onload = function () {

    function genericOnClick() {
        console.log("hei! menyen ble klikket på.");
    }

    document.querySelector('#showmenu').onclick = function (e) {

        console.log("getSavedPasswords");

        chrome.savedpasswords.getList( function(data) {
          console.log("item clicked %o", data);

          if (chrome.runtime.lastError) {
            console.log("upsi %o ", chrome.runtime.lastError);
            console.log(chrome.runtime.lastError.message);
          }
          console.log(id);
        });
    };

  document.querySelector('#delete').onclick = function (e) {
        console.log("delete");

        chrome.savedpasswords.remove('1', function(data) {
          console.log("item deleted %o", data);

          if (chrome.runtime.lastError) {
            console.log("upsi %o ", chrome.runtime.lastError);
            console.log(chrome.runtime.lastError.message);
          }
          console.log(id);
        });
    };
}
