// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef RENDERER_BLINK_VIVALDI_SPATNAV_QUAD_H_
#define RENDERER_BLINK_VIVALDI_SPATNAV_QUAD_H_

#define INSIDE_BLINK 1

#include <vector>

#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "ui/gfx/geometry/rect.h"

// For the direction type
#include "renderer/mojo/vivaldi_frame_service.mojom.h"

namespace vivaldi {

class Quad;
using QuadPtr = scoped_refptr<Quad>;

struct SpatnavPoint {
  bool IsNull() { return index == -1; }

  int sx;
  int sy;
  int x;
  int y;
  int index = -1;
  int weight;
  int direction;
  int quality;
};

class Quad : public base::RefCountedThreadSafe<Quad> {
 public:
  Quad(int left,
       int right,
       int top,
       int bottom,
       std::string href,
       blink::WebElement element);

  static void BuildPoints(const std::vector<QuadPtr>& quads);
  static QuadPtr GetInitialQuad(const std::vector<QuadPtr>& quads,
                                vivaldi::mojom::SpatnavDirection);

  // Returns null if element is destroyed during navigation.
  blink::Element* GetElement();

  blink::WebElement GetWebElement();

  gfx::Rect GetRect();

  bool IsContainedIn(QuadPtr q);
  bool Overlaps(QuadPtr q);
  void AddQuadBelow(QuadPtr quad);
  void AddQuadAbove(QuadPtr quad);
  void AddQuadToRight(QuadPtr quad);
  void AddQuadToLeft(QuadPtr quad);
  int MidX() { return (left_ + right_) / 2; }
  int MidY() { return (bottom_ + top_) / 2; }
  bool IsOnVerticalLine(int x);
  bool IsOnHorizontalLine(int y);
  void FindNeighborQuads(const std::vector<QuadPtr>& quads);
  int FindBestX(QuadPtr quad, int& quality);
  int FindBestY(QuadPtr quad, int& quality);

  QuadPtr NextUp();
  QuadPtr NextRight();
  QuadPtr NextDown();
  QuadPtr NextLeft();

  int Left() { return left_; }
  int Right() { return right_; }
  int Top() { return top_; }
  std::string Href() { return href_; }

 private:
  friend class base::RefCountedThreadSafe<Quad>;
  ~Quad();

  int x_;
  int y_;
  int width_;
  int height_;
  int left_;
  int right_;
  int top_;
  int bottom_;
  std::string href_;

  int id_;
  int weight_;
  int direction_;

  blink::WebElement web_element_;

  std::vector<QuadPtr> above_;
  std::vector<QuadPtr> below_;
  std::vector<QuadPtr> toLeft_;
  std::vector<QuadPtr> toRight_;
};

int findDirection(SpatnavPoint p2, int direction);
int weightWithDeviation(int weight, int deviation);
std::vector<SpatnavPoint> minWeights(const std::vector<SpatnavPoint>& pts,
                                     int navDir,
                                     int maxDev);
int deviation(SpatnavPoint pt, int navDir);

}  // namespace vivaldi

#endif  // RENDERER_BLINK_VIVALDI_SPATNAV_QUAD_H_
