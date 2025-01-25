#include "components/favicon/core/favicon_backend.h"

#include "base/base64.h"
#include "base/containers/contains.h"
#include "components/favicon/core/favicon_database.h"
#include "components/favicon_base/favicon_util.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "app/vivaldi_apptools.h"

namespace favicon {

namespace {

struct VivaldiPreloadedFavicon {
  std::string_view page_url;
  std::string_view favicon_url;
  std::string_view favicon_png_base64;
};

#include "components/favicon/vivaldi_preloaded_favicons.inc"
}

void FaviconBackend::SetVivaldiPreloadedFavicons() {
  if (!vivaldi::IsVivaldiRunning())
    return;
  db_->DeleteVivaldiPreloadedFavicons();

  for (size_t i = 0; i < std::size(kPreloadedFavicons); i++) {
    auto png = base::Base64Decode(kPreloadedFavicons[i].favicon_png_base64);
    if (!png) {
      NOTREACHED();
    }
    gfx::Image image = gfx::Image::CreateFrom1xPNGBytes(*png);
    gfx::ImageSkia image_skia = image.AsImageSkia();
    image_skia.EnsureRepsForSupportedScales();
    std::vector<SkBitmap> bitmaps;
    const std::vector<float> favicon_scales = favicon_base::GetFaviconScales();
    for (const gfx::ImageSkiaRep& rep : image_skia.image_reps()) {
      // Only save images with a supported sale.
      if (base::Contains(favicon_scales, rep.scale()))
        bitmaps.push_back(rep.GetBitmap());
    }
    SetFavicons(
      { GURL(kPreloadedFavicons[i].page_url) },
      favicon_base::IconType::kFavicon,
      GURL(kPreloadedFavicons[i].favicon_url),
      bitmaps,
      FaviconBitmapType::VIVALDI_PRELOADED);
  }
}

void FaviconDatabase::DeleteVivaldiPreloadedFavicons() {
  // Restrict to vivaldi preloaded bitmaps (i.e. with last_requested == -1).
  // This is called once on starutp to cleanup preloaded favicons before
  // setting them up anew, so not worth caching.
  sql::Statement vivaldi_icons(db_.GetUniqueStatement(
      "SELECT favicons.id "
      "FROM favicons "
      "JOIN favicon_bitmaps ON (favicon_bitmaps.icon_id = favicons.id) "
      "WHERE (favicon_bitmaps.last_requested = ?)"));
  vivaldi_icons.BindInt64(0, -1);

  std::set<favicon_base::FaviconID> icon_ids;

  while (vivaldi_icons.Step()) {
    icon_ids.insert(vivaldi_icons.ColumnInt64(0));
  }

  for (favicon_base::FaviconID icon_id : icon_ids) {
    DeleteFavicon(icon_id);
    DeleteIconMappingsForFaviconId(icon_id);
  }
}

} // namespace favicon
