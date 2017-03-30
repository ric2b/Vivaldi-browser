onload = function () {

    function genericOnClick() {
        console.log("genericOnClick");
    }
      document.querySelector('#showProfileData').onclick = function () {
      console.log("show profiles");
      chrome.importData.getProfiles(function(profiles) {
        console.log(profiles);
      });
    };

    document.querySelector('#importProfileData').onclick = function () {
      console.log("import profiles");

        var idx = 1;
        var history =   false;
        var favorities= true;
        var passwords= false;
        var search = false;
        var notes = false;
        var operaDefault = true

        var importArray=new Array(idx.toString() ,history.toString(),favorities.toString(),passwords.toString(),search.toString(),notes.toString(),operaDefault.toString());

        chrome.importData.startImport(importArray, function(results) {
            console.log(results)
        });

      chrome.importData.getProfiles(function(profiles) {
        console.log(profiles);
      });
    };
}
