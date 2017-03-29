onload = function () {

  function genericOnClick() {
    console.log("hey, we clicked the menu!");
  }
  document.querySelector('#showmenu').onclick = function (e) {
    console.log("showmenu");
    var createprops = {left: e.clientX,
                       top:  e.clientY,
                       items: [
                         {id:0, name:"item1" },
                         {id:1, name:"item2" }
                       ]
                     };

    chrome.showMenu.create(createprops, function(id) { console.log("item clicked "+ id); });
  };
  document.querySelector('#submenus').onclick = function (e) {
    var createprops = {
      left:  e.clientX,
      top:   e.clientY,
      items: [
        { id: 10, name: "File", items: [
          {id: 11, name: "New tab"},
          {id: 12, name: "New window"},
          {id: 13, name: "New private tab"},
          {id: -1, name: "---" },
          {id: 14, name: "Show menu bar", type: "checkbox", checked: true},
          {id: -1, name: "---" },
          {id: 15, name: "Exit"}
        ]},
        { id:20 , name:"Edit", items: [
          {id: 21, name: "Undo" },
          {id: 22, name: "Redo" },
          {id: 23, name: "---" },
          {id: 24, name: "Cut" },
          {id: 25, name: "Copy"},
          {id: 26, name: "Paste"},
          {id: -1, name: "---" },
          {id: 27, name: "Select all" },
        ]},
        { id:30 , name:"View", items: [
          {id:31, name: "Full screen", type: "checkbox"},
          {id:32, name:"Mail"},
          {id:33, name:"Toolbars", items: [
            {id:34, name:"Panels"},
            {id:35, name:"Status bar", type: "checkbox", checked: true}
          ]}
        ]},
        { id:40 , name:"Tools", items: [
          {id:41, name:"Settings"}
        ]},
        { id:50 , name:"Help" }
      ]
    };
    chrome.showMenu.create(createprops, function(id) {
      console.log("item clicked "+ id);

      if (chrome.runtime.lastError) {
        console.log("upsi %o ", chrome.runtime.lastError);
        console.log(chrome.runtime.lastError.message);
      }
      console.log(id);
    });
  };
}
