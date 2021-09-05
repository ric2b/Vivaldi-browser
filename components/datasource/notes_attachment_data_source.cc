// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved.

#include "components/datasource/notes_attachment_data_source.h"

#include "base/base64.h"
#include "chrome/browser/profiles/profile.h"
#include "components/datasource/vivaldi_data_source_api.h"
#include "content/public/browser/browser_thread.h"
#include "notes/note_attachment.h"
#include "notes/note_node.h"
#include "notes/notes_factory.h"
#include "notes/notes_model.h"
#include "third_party/re2/src/re2/re2.h"

NotesAttachmentDataClassHandler::NotesAttachmentDataClassHandler(
    Profile* profile)
    : profile_(profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile_);
}

NotesAttachmentDataClassHandler::~NotesAttachmentDataClassHandler() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void NotesAttachmentDataClassHandler::GetData(
    const std::string& data_id,
    content::URLDataSource::GotDataCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // data_id should be noteId/attachmentChecksum|attachmentsize
  std::string note_id, checksum, size;
  scoped_refptr<base::RefCountedMemory> data;

  if (RE2::FullMatch(data_id, R"(^(\d+)\/(.*?)%7C(\d+)$)", &note_id, &checksum,
                     &size)) {
    int64_t int_id;
    if (base::StringToInt64(note_id, &int_id)) {
      const vivaldi::NotesModel* notes_model =
          vivaldi::NotesModelFactory::GetForBrowserContext(profile_);
      const vivaldi::NoteNode* note =
          vivaldi::GetNotesNodeByID(notes_model, int_id);

      const vivaldi::NoteAttachments& att = note->GetAttachments();
      const vivaldi::NoteAttachments::const_iterator it =
          att.find(checksum + "|" + size);
      if (it != att.end()) {
        const std::string content = it->second.content();
        std::string img_src;
        base::Base64Decode(content.substr(content.find(",") + 1), &img_src);

        std::vector<unsigned char> buffer;
        std::copy(img_src.begin(), img_src.end(), std::back_inserter(buffer));
        data = base::RefCountedBytes::TakeVector(&buffer);
      }
    };
  }
  std::move(callback).Run(std::move(data));
}
