
onload = function () {
    var sessionuniquecounter = 0;
    var sessioncontent;
    var onNewNodeAdded = function (newnode) {
        var statuslist = document.querySelector('#infolist');

        li = statuslist.appendChild(document.createElement('li'));
        li.textContent = "done adding " + newnode.title + " " + newnode.id + " added:" + Date(newnode.dateAdded);
    };

    var onNodeRemoved = function () {
        var statuslist = document.querySelector('#infolist');
        li = statuslist.appendChild(document.createElement('li'));
        li.textContent = "node removed";
    };

    // event
    chrome.notes.onCreated = onNewNodeAdded;

    document.querySelector('#getnote').onclick = function (e) {

        var onNoteFetched = function (resultArray) {
            var statuslist = document.querySelector('#infolist');
            li = statuslist.appendChild(document.createElement('li'));
            var notenode = resultArray[0];
            var wasAdded = new Date(notenode.dateAdded);
            li.textContent = "found note with id=" + notenode.id + " " + notenode.title + " was added:" + wasAdded;
        };

        chrome.notes.get("6", onNoteFetched);
    };
    document.querySelector('#addnote').onclick = function (e) {

        ++sessionuniquecounter;
        sessioncontent = "this is some sessionunique content + " + sessionuniquecounter;

        var createprops = { title: "test",
                            content:  sessioncontent,
                            attachments: [
                              { id: "1", content: "attachments inline content item1", content_type: "text/plain", filename: "/var/usr/big_file.mpg" },
                              { id: "2", content: "attachments inline content item2", content_type: "application/octet-stream", filename: "/var/usr/big_file.mpg" }
                           ]
                         };

        chrome.notes.create(createprops);

    };

    document.querySelector('#removenote').onclick = function (e) {
        chrome.notes.remove("6", onNodeRemoved);
    };

    document.querySelector('#changenote').onclick = function (e) {
        var changes = {
            title: "yo",
            content: "yo",
            url: "yo",
            dateAdded: 0,
            dateGroupModified: 0
        };
        chrome.notes.update("6", changes, function(changenote) { console.log("note changed"); });
    };

    var addnotetotreelist = function(pNote) {
        var notestree = document.querySelector('#notestree');
        var li = notestree.appendChild(document.createElement('li'));
        var wasAdded = new Date(pNote.dateAdded);
        li.textContent = pNote.id + " " + pNote.title + " was added:" + wasAdded;
        // TODO: Show the attachments as well.
    }

    document.querySelector('#showtree').onclick = function (e) {
        // clear the list
        document.querySelector('#notestree').textContent = '';

        // Note: This will only work on an unaltered tree where
        //       the |id|s is sequential.
        var idx = 1;
        var gotNote = function (notes) {
            if (chrome.runtime.lastError) {
                console.log("no note found");
            }

            if (notes) {
                addnotetotreelist(notes[0]);
                idx++;
                getNotes();
            }
        };

        var getNotes = function () {
            chrome.notes.get(idx.toString(), gotNote);
        };

        getNotes(); // get all notes and show them
    };

    var showNotes = function(notes) {
            for(var i=0; i<notes.length; i++) {
                var note = notes[i];
                if ( note.children)
                   showNotes(note.children);
                var notestree = document.querySelector('#notestree');
                var li = notestree.appendChild(document.createElement('li'));
                var wasAdded = new Date(note.dateAdded);
                li.textContent = note.id + " " + note.title + " was added:" + wasAdded;
            }
        };
    document.querySelector('#showgettree').onclick = function (e) {
        // clear the list
        document.querySelector('#notestree').textContent = '';

    var notes = chrome.notes.getTree( showNotes );
    };

    document.querySelector('#searchtree').onclick = function (e) {
        // clear the list
        document.querySelector('#notestree').textContent = '';

    var notes = chrome.notes.search("yo", showNotes );

    };

}
