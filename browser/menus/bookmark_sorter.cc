// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.

#include "browser/menus/bookmark_sorter.h"

#include "base/i18n/string_compare.h"
#include "components/bookmarks/browser/bookmark_model.h"

namespace vivaldi {

BookmarkSorter::BookmarkSorter(SortField sort_field, SortOrder sort_order,
                               bool group_folders)
  :sort_field_(sort_field)
  ,sort_order_(sort_order)
  ,group_folders_(group_folders) {

  if (sort_order_ == ORDER_NONE)
    sort_field_ = FIELD_NONE;

  UErrorCode error = U_ZERO_ERROR;
  collator_.reset(icu::Collator::createInstance(error));
  if (U_FAILURE(error)) {
    collator_.reset(nullptr);
  }
}

BookmarkSorter::~BookmarkSorter() {
}

void BookmarkSorter::sort(std::vector<bookmarks::BookmarkNode*>& vector) {
  switch (sort_field_) {
    case FIELD_TITLE:
      std::sort(vector.begin(), vector.end(),
          [this](bookmarks::BookmarkNode *b1, bookmarks::BookmarkNode *b2) {
        if ((b1->type() == b2->type()) || !group_folders_) {
          const icu::Collator* collator = collator_.get();

          int l1 = b1->GetTitle().length();
          int l2 = b2->GetTitle().length();
          if (l1 == 0 || l2 == 0) {
            return fallbackToTitleSort(collator, b1, b2, l1, l2);
          }

          if (sort_order_ == ORDER_ASCENDING) {
            return base::i18n::CompareString16WithCollator(
                *collator, b1->GetTitle(), b2->GetTitle()) == UCOL_LESS;
          } else {
            return base::i18n::CompareString16WithCollator(
                *collator, b2->GetTitle(), b1->GetTitle()) == UCOL_LESS;
          }
        }
        return b1->is_folder();
      });
      break;
    case FIELD_URL:
      std::sort(vector.begin(), vector.end(),
          [this](bookmarks::BookmarkNode *b1, bookmarks::BookmarkNode *b2) {
        if ((b1->type() == b2->type()) || !group_folders_) {

          int l1 = b1->url().spec().length();
          int l2 = b2->url().spec().length();
          if (l1 == 0 || l2 == 0) {
            return fallbackToTitleSort(collator_.get(), b1, b2, l1, l2);
          }

          if (sort_order_ == ORDER_ASCENDING) {
            return b1->url().spec() < b2->url().spec();
          } else {
            return b2->url().spec() < b1->url().spec();
          }
        }
        return b1->is_folder();
      });
      break;
    case FIELD_NICKNAME:
      std::sort(vector.begin(), vector.end(),
          [this](bookmarks::BookmarkNode *b1, bookmarks::BookmarkNode *b2) {
        if ((b1->type() == b2->type()) || !group_folders_) {
          const icu::Collator* collator = collator_.get();

          int l1 = b1->GetNickName().length();
          int l2 = b2->GetNickName().length();
          if (l1 == 0 || l2 == 0) {
            return fallbackToTitleSort(collator, b1, b2, l1, l2);
          }

          if (sort_order_ == ORDER_ASCENDING) {
            return base::i18n::CompareString16WithCollator(
                *collator, b1->GetNickName(), b2->GetNickName()) == UCOL_LESS;
          } else {
            return base::i18n::CompareString16WithCollator(
                *collator, b2->GetNickName(), b1->GetNickName()) == UCOL_LESS;
          }
        }
        return b1->is_folder();
      });
      break;
    case FIELD_DESCRIPTION:
      std::sort(vector.begin(), vector.end(),
          [this](bookmarks::BookmarkNode *b1, bookmarks::BookmarkNode *b2) {
        if ((b1->type() == b2->type()) || !group_folders_) {
          const icu::Collator* collator = collator_.get();

          int l1 = b1->GetDescription().length();
          int l2 = b2->GetDescription().length();
          if (l1 == 0 || l2 == 0) {
            return fallbackToTitleSort(collator, b1, b2, l1, l2);
          }

          if (sort_order_ == ORDER_ASCENDING) {
            return base::i18n::CompareString16WithCollator(
                *collator, b1->GetDescription(), b2->GetDescription())
                    == UCOL_LESS;
          } else {
            return base::i18n::CompareString16WithCollator(
                *collator, b2->GetDescription(), b1->GetDescription())
                    == UCOL_LESS;
          }
        }
        return b1->is_folder();
      });
      break;
    case FIELD_DATEADDED:
      std::sort(vector.begin(), vector.end(),
          [this](bookmarks::BookmarkNode *b1, bookmarks::BookmarkNode *b2) {
        if ((b1->type() == b2->type()) || !group_folders_) {
          if (sort_order_ == ORDER_ASCENDING) {
            return b1->date_added() < b2->date_added();
          } else {
            return b2->date_added() < b1->date_added();
          }
        }
        return b1->is_folder();
      });
      break;
    // Keep compiler happy.
    case FIELD_NONE:
      break;
  }
}

bool BookmarkSorter::fallbackToTitleSort(const icu::Collator* collator,
                                         bookmarks::BookmarkNode *b1,
                                         bookmarks::BookmarkNode *b2,
                                         int l1,
                                         int l2) {
  if (l1 == 0 && l2 == 0) {
    l1 = b1->GetTitle().length();
    l2 = b2->GetTitle().length();
    if (l1 == 0 || l2 == 0) {
      // Sort by date if there is missing title information
      return fallbackToDateSort(b1, b2, l1, l2);
    }
    if (sort_order_ == ORDER_ASCENDING) {
      return base::i18n::CompareString16WithCollator(
          *collator, b1->GetTitle(), b2->GetTitle()) == UCOL_LESS;
    } else {
      return base::i18n::CompareString16WithCollator(
          *collator, b2->GetTitle(), b1->GetTitle()) == UCOL_LESS;
    }
  } else if (l1 == 0) {
    return sort_order_ == ORDER_ASCENDING ? false : true;
  } else {
    return sort_order_ == ORDER_ASCENDING ? true : false;
  }
}

bool BookmarkSorter::fallbackToDateSort(bookmarks::BookmarkNode *b1,
                                        bookmarks::BookmarkNode *b2,
                                        int l1,
                                        int l2) {
  if (l1 == 0 && l2 == 0) {
    if (sort_order_ == ORDER_ASCENDING) {
      return b1->date_added() < b2->date_added();
    } else {
      return b2->date_added() < b1->date_added();
    }
  } else if (l1 == 0) {
    return sort_order_ == ORDER_ASCENDING ? false : true;
  } else {
    return sort_order_ == ORDER_ASCENDING ? true : false;
  }
}

}  // vivaldi
