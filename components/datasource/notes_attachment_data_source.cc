// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved.

#include "components/datasource/notes_attachment_data_source.h"

#include "base/base64.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "notes/note_attachment.h"
#include "notes/note_node.h"
#include "notes/notes_factory.h"
#include "notes/notes_model.h"
#include "third_party/re2/src/re2/re2.h"

void NotesAttachmentDataClassHandler::GetData(
    Profile* profile,
    const std::string& data_id,
    content::URLDataSource::GotDataCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // data_id should be noteId/attachmentChecksum|attachmentsize
  const char pattern[] = R"(^(\d+)\/(.*?)%7C(\d+)$)";
  std::string note_id, checksum, size;
  const vivaldi::NoteNode* note = nullptr;
  if (RE2::FullMatch(data_id, pattern, &note_id, &checksum, &size)) {
    int64_t int_id;
    if (base::StringToInt64(note_id, &int_id)) {
      const vivaldi::NotesModel* notes_model =
          vivaldi::NotesModelFactory::GetForBrowserContext(profile);
      note = vivaldi::GetNotesNodeByID(notes_model, int_id);
    }
  }

  scoped_refptr<base::RefCountedMemory> data;
  if (!note) {
    LOG(ERROR) << "Unknown note, dataid=" << data_id;
  } else {
    std::string attachment_id = std::move(checksum);
    attachment_id += '|';
    attachment_id += size;
    const vivaldi::NoteAttachments& att = note->GetAttachments();
    auto it = att.find(attachment_id);
    if (it == att.end()) {
      LOG(ERROR) << "Unknown note attachment, dataid=" << data_id;
    } else {
      base::StringPiece content(it->second.content());
      size_t comma = content.find(',');
      if (comma == base::StringPiece::npos) {
        LOG(ERROR) << "Invalid note content format, dataid=" << data_id;
      } else {
        std::string img_src;
        base::Base64Decode(content.substr(comma + 1), &img_src);
        data = base::RefCountedString::TakeString(&img_src);
      }
    }
  }
  std::move(callback).Run(std::move(data));
}
