// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#include "renderer/blink/vivaldi_spatnav_quad.h"

#define _USE_MATH_DEFINES

#include <cmath>
#include <math.h>
#include <vector>

namespace vivaldi {

Quad::Quad(int x,
           int y,
           int width,
           int height,
           std::string href,
           blink::WebElement element) {
  x_ = x;
  y_ = y;
  width_ = width;
  height_ = height;
  left_ = x;
  right_ = x + width;
  top_ = y;
  bottom_ = y + height;
  weight_ = direction_ = 0;
  href_ = std::move(href);
  web_element_ = element;
  id_ = -1;
}

Quad::~Quad() {}

blink::Element* Quad::GetElement() {
  if (web_element_.IsNull())
    return nullptr;
  return web_element_.Unwrap<blink::Element>();
}

blink::WebElement Quad::GetWebElement() {
  return web_element_;
}

gfx::Rect Quad::GetRect() {
  gfx::Rect r = gfx::Rect(x_, y_, width_, height_);
  return r;
}

bool Quad::IsContainedIn(QuadPtr q) {
  return left_ >= q->left_ &&
         top_ >= q->top_ &&
         right_ <= q->right_ &&
         bottom_ <= q->bottom_;
}

bool Quad::Overlaps(QuadPtr q) {
  return ((bottom_ >= q->top_ && bottom_ <= q->bottom_) &&
          (right_ >= q->left_ && right_ <= q->right_)) ||
         ((top_ <= q->bottom_ && top_ >= q->top_) &&
          (left_ >= q->left_ && left_ <= q->right_));
}

void Quad::AddQuadBelow(QuadPtr quad) {
  below_.push_back(quad);
}

void Quad::AddQuadAbove(QuadPtr quad) {
  above_.push_back(quad);
}

void Quad::AddQuadToRight(QuadPtr quad) {
  toRight_.push_back(quad);
}

void Quad::AddQuadToLeft(QuadPtr quad) {
  toLeft_.push_back(quad);
}

// Vertical line drawn through x intersects our quad.
bool Quad::IsOnVerticalLine(int x) {
  return x >= left_ && x <= right_;
}

// Horizontal line through y intersects.
bool Quad::IsOnHorizontalLine(int y) {
  return y >= top_ && y <= bottom_;
}

int Quad::FindBestX(QuadPtr quad, int& quality) {
  const int midQ1 = MidX();
  const int midQ2 = quad->MidX();
  if (quad->IsOnVerticalLine(midQ1)) {
    quality = 1;
    return midQ1;
  } else if (IsOnVerticalLine(midQ2)) {
    quality = 1;
    return midQ2;
  } else if (quad->IsOnVerticalLine(left_)) {
    quality = 2;
    return left_;
  } else if (quad->IsOnVerticalLine(right_)) {
    quality = 2;
    return right_;
  } else if (IsOnVerticalLine(quad->left_)) {
    quality = 2;
    return quad->left_;
  } else if (IsOnVerticalLine(quad->right_)) {
    quality = 2;
    return quad->right_;
  } else if (left_ > quad->right_) {
    quality = 2;
    return left_;
  } else if (right_ < quad->left_) {
    quality = 2;
    return right_;
  } else {
    quality = 1;
    return midQ1;
  }
}

int Quad::FindBestY(QuadPtr quad, int& quality) {
  const int midQ1 = MidY();
  const int midQ2 = quad->MidY();
  if (quad->IsOnHorizontalLine(midQ1)) {
    quality = 1;
    return midQ1;
  } else if (IsOnHorizontalLine(midQ2)) {
    quality = 1;
    return midQ2;
  } else if (quad->IsOnHorizontalLine(top_)) {
    quality = 2;
    return top_;
  } else if (quad->IsOnHorizontalLine(bottom_)) {
    quality = 2;
    return bottom_;
  } else if (IsOnHorizontalLine(quad->top_)) {
    quality = 2;
    return quad->top_;
  } else if (IsOnHorizontalLine(quad->bottom_)) {
    quality = 2;
    return quad->bottom_;
  } else if (top_ > quad->bottom_) {
    quality = 2;
    return top_;
  } else if (bottom_ < quad->top_) {
    quality = 2;
    return bottom_;
  } else {
    quality = 1;
    return midQ1;
  }
}

int getWeight(SpatnavPoint p) {
  int dx = p.x - p.sx;
  int dy = p.y - p.sy;
  return dx*dx + dy*dy;
}

int findDirection(SpatnavPoint p2, int direction) {
  int dx = p2.x - p2.sx;
  int dy = p2.y - p2.sy;
  if (dx == 0 && dy == 0) {
    // going to a touching quad may result in distance
    // of 0. Then we say that we are going directly in
    // the requested direction.
    return direction;
  }
  int d = (int)(atan2((double)dy, (double)dx) * (180.0 / M_PI));
  return d;
}

int deviation(SpatnavPoint pt, int navDir) {
  // our circle is like so:
  //         -90
  //           |
  //           |
  //180-----------------0
  //           |
  //           |
  //          90
  // hence the modulus 90 to get deviation
  // for -135 and 180 to 45 instead of 315
  // the deviation should never by more than
  // 90° because we only search the pts that
  // are in the half of the graph that the
  // navDir points to.
  int dev;
  if (navDir == 180 && pt.direction < 0) {
    dev = navDir + pt.direction;
  } else {
    dev = abs(pt.direction - navDir);
  }
  return dev;
}

int weightWithDeviation(int weight, int deviation) {
  return weight * (30 + deviation);
}

std::vector<SpatnavPoint> minWeights(const std::vector<SpatnavPoint>& pts,
                                     int navDir,
                                     int maxDev) {
  int min = INT_MAX;
  for (size_t i = 0; i < pts.size(); i++) {
    SpatnavPoint pt = pts[i];
    const int dev = deviation(pt, navDir);
    const int weight = weightWithDeviation(pt.weight, dev);
    if (dev <= maxDev && weight < min) {
      min = weight;
    }
  }
  std::vector<SpatnavPoint> minPts;
  for (size_t i = 0; i < pts.size(); i++) {
    SpatnavPoint pt = pts[i];
    const int dev = deviation(pt, navDir);
    const int weight = weightWithDeviation(pt.weight, dev);
    if (dev <= maxDev && weight == min) {
      minPts.push_back(pt);
    }
  }
  return minPts;
}

void getMinDeviations(const std::vector<SpatnavPoint>& pts,
                      std::vector<SpatnavPoint> minPts,
                      const std::vector<int>& deviations,
                      int navDir) {
  int min = INT_MAX;
  for (size_t i = 0; i < pts.size(); i++) {
    int d = deviation(pts[i], navDir);
    if (d < min) {
      min = d;
    }
  }
  for (size_t i = 0; i < pts.size(); i++) {
    SpatnavPoint pt = pts[i];
    int d = deviation(pts[i], navDir);
    if (d == min) {
      minPts.push_back(pt);
    }
  }
}

SpatnavPoint bestDirection(const std::vector<SpatnavPoint>& pts, int navDir) {
  // we can have 3 candidates where one
  // of them is exactly in the direction
  // we are headed. Then that is best option.
  // Otherwise we select the smaller of the
  // other two unless we are going left. Then
  // we select the bigger one. Whoa!

  std::vector<SpatnavPoint> optimal;
  for (size_t i = 0; i < pts.size(); i++) {
    if (pts[i].direction == navDir) {
      optimal.push_back(pts[i]);
    }
  }

  if (optimal.size() > 0) {
    return optimal[0];
  }

  std::vector<SpatnavPoint> minPts;
  std::vector<int> deviations;
  getMinDeviations(pts, minPts, deviations, navDir);

  if (minPts.size() == 0) {
    return SpatnavPoint();
  }

  SpatnavPoint lastp;
  for (size_t i = 0; i < minPts.size(); i++) {
    SpatnavPoint p = minPts[i];
    int d1 = p.direction;
    int d2 = lastp.direction;

    if (navDir == 180 && d1 != d2) {
      lastp = (d1 > d2) ? p : lastp;
    } else {
      lastp = (d1 < d2) ? lastp : p;
    }
  }
  return lastp;
}

SpatnavPoint nextPt(const std::vector<SpatnavPoint>& pts, int navDir) {
  std::vector<SpatnavPoint> weighted_pts = pts;
  for (auto& pt: weighted_pts) {
    pt.direction = findDirection(pt, navDir);
    pt.weight = getWeight(pt);
  }
  std::vector<SpatnavPoint> min90 = minWeights(weighted_pts, navDir, 90);
  std::vector<SpatnavPoint> min45 = minWeights(weighted_pts, navDir, 45);

  std::vector<SpatnavPoint> *candidates = min45.size() ? &min45 : &min90;

  if (candidates->size() == 0) {
    return SpatnavPoint();
    //TODO: Implement wrapping
  } else if (candidates->size() == 1) {
    return (*candidates)[0];
  } else {
    return bestDirection(*candidates, navDir);
  }
}

void Quad::FindNeighborQuads(const std::vector<QuadPtr>& quads) {
  for (auto q: quads) {
    if (q->id_ == id_) {
      // No need to compare to myself
      continue;
    }
    if (Overlaps(q)) {
      if (q->left_ < left_) {
        AddQuadToLeft(q);
      }
      if (q->right_ > right_) {
        AddQuadToRight(q);
      }
      if (q->top_ < top_) {
        AddQuadAbove(q);
      }
      if (q->bottom_ > bottom_) {
        AddQuadBelow(q);
      }
      continue;
    }
    if (q->top_ > top_) {
      // q is below or nested inside
      AddQuadBelow(q);
    } else if (q->bottom_ <= top_) {
      // q is strictly above me
      AddQuadAbove(q);
    }
    if (q->left_ > left_) {
      // q is to the right of me or nested inside
      AddQuadToRight(q);
    } else if (q->right_ <= left_) {
      // q is strictly to the left of me
      AddQuadToLeft(q);
    }
  }
}

//TODO: Review this
std::vector<SpatnavPoint> GetBestQuality(const std::vector<SpatnavPoint>& pts) {
  std::vector<SpatnavPoint> topQuality;
  std::vector<SpatnavPoint> lowQuality;
  for (auto p : pts) {
    if (p.quality == 1) {
      topQuality.push_back(p);
    } else if (p.quality == 2) {
      lowQuality.push_back(p);
    }
  }
  return topQuality.size() > 0 ? topQuality : lowQuality;
}

// static
void Quad::BuildPoints(const std::vector<QuadPtr>& quads) {
  if (quads.size() == 0) {
    return;
  }
  int id = 0;
  for (auto q : quads) {
    q->id_ = id++;
    q->FindNeighborQuads(quads);
  }
}

// static
QuadPtr Quad::GetInitialQuad(const std::vector<QuadPtr>& quads,
                       vivaldi::mojom::SpatnavDirection direction) {
  using vivaldi::mojom::SpatnavDirection;
  if (quads.size() == 0) {
    return nullptr;
  }

  QuadPtr match = quads[0];
  if (direction == SpatnavDirection::kUp) {
    for (size_t i = 0; i < quads.size(); i++) {
      if (quads[i]->Top() > match->Top()) {
        match = quads[i];
      }
    }
  } else if (direction == SpatnavDirection::kLeft) {
    for (size_t i = 0; i < quads.size(); i++) {
      if (quads[i]->Left() > match->Left()) {
        match = quads[i];
      }
    }
  } else if (direction == SpatnavDirection::kDown ||
             direction == SpatnavDirection::kRight) {
    for (size_t i = 0; i < quads.size(); i++) {
      if (match->Left() + match->Top() > quads[i]->Left() + quads[i]->Top()) {
        match = quads[i];
      }
    }
  } else {
    return nullptr;
  }
  return match;
}

QuadPtr Quad::NextUp() {
  if (above_.size() == 0) {
    return nullptr;
  }

  std::vector<SpatnavPoint> ds;
  for (size_t i = 0; i < above_.size(); i++) {
    SpatnavPoint p;
    QuadPtr q = above_[i];
    int quality;

    int best_x = FindBestX(q, quality);
    int best_source_y =
        Overlaps(q) ? std::min(bottom_, q->bottom_) : top_;
    p.sx = best_x;
    p.sy = best_source_y;
    p.x = q->right_ < best_x ? q->right_ :
          q->left_ > best_x ? q->left_ : best_x;
    p.y = q->bottom_;
    p.index = i;
    p.weight = 1;
    p.quality = quality;
    ds.push_back(p);
  }

  std::vector<SpatnavPoint> candidates = GetBestQuality(ds);
  SpatnavPoint pt = nextPt(ds, -90);
  if (pt.IsNull()) {
    return nullptr;
  }
  return above_[pt.index];
}

QuadPtr Quad::NextDown() {
  if (below_.size() == 0) {
    return nullptr;
  }

  std::vector<SpatnavPoint> ds;
  for (size_t i = 0; i < below_.size(); i++) {
    SpatnavPoint p;
    QuadPtr q = below_[i];
    int quality;

    int best_x = FindBestX(q, quality);
    int best_source_y = IsContainedIn(q) ?
        top_ : Overlaps(q) ? std::max(top_, q->top_) : bottom_;
    p.sx = best_x;
    p.sy = best_source_y;
    p.x = q->right_ < best_x ?
          q->right_ : q->left_ > best_x ? q->left_ : best_x;
    p.y = q->top_;
    p.index = i;
    p.weight = 1;
    p.quality = quality;
    ds.push_back(p);
  }

  std::vector<SpatnavPoint> candidates = GetBestQuality(ds);
  SpatnavPoint pt = nextPt(ds, 90);
  if (pt.IsNull()) {
    return nullptr;
  }
  return below_[pt.index];
}

QuadPtr Quad::NextRight() {
  if (toRight_.size() == 0) {
    return nullptr;
  }

  std::vector<SpatnavPoint> ds;
  for (size_t i = 0; i < toRight_.size(); i++) {
    SpatnavPoint p;
    QuadPtr q = toRight_[i];
    int quality;

    int best_source_x = IsContainedIn(q) ? left_ :
                        Overlaps(q) ? std::max(left_, q->left_) : right_;
    int best_y = FindBestY(q, quality);

    p.sx = best_source_x;
    p.sy = best_y;
    p.x = q->left_;
    p.y = q->bottom_ < best_y ?
          q->bottom_ : q->top_ > best_y ? q->top_ : best_y;
    p.index = i;
    p.weight = 1;
    p.quality = quality;
    ds.push_back(p);
  }

  std::vector<SpatnavPoint> candidates = GetBestQuality(ds);
  SpatnavPoint pt = nextPt(candidates, 0);
  if (pt.IsNull()) {
    return nullptr;
  }
  return toRight_[pt.index];
}

QuadPtr Quad::NextLeft() {
  if (toLeft_.size() == 0) {
    return nullptr;
  }

  std::vector<SpatnavPoint> ds;
  for (size_t i = 0; i < toLeft_.size(); i++) {
    SpatnavPoint p;
    QuadPtr q = toLeft_[i];
    int quality;

    int best_y = FindBestY(q, quality);
    int best_source_x =
        Overlaps(q) ? std::min(right_, q->right_) : left_;
    p.sx = best_source_x;
    p.sy = best_y;

    p.x = q->right_;
    p.y = q->bottom_ < best_y ? q->bottom_ : q->top_ > best_y ? q->top_ : best_y;

    p.index = i;
    p.weight = 1;
    p.quality = quality;
    ds.push_back(p);
  }

  std::vector<SpatnavPoint> candidates = GetBestQuality(ds);
  SpatnavPoint pt = nextPt(candidates, 180);
  if (pt.IsNull()) {
    return nullptr;
  }
  return toLeft_[pt.index];
}

}  // namespace vivaldi
