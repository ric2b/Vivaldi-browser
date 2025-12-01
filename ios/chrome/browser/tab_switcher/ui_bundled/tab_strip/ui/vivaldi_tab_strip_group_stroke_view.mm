// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note:(prio@vivaldi.com) - It is a chromium implementation which we moved
// to Vivaldi module to reduce the patch on Chromium git.
// This file is more heavily changed hence no // Vivaldi mark is added
// for the patches.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_strip/ui/vivaldi_tab_strip_group_stroke_view.h"

#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_strip/ui/swift_constants_for_objective_c.h"

@implementation VivaldiTabStripGroupStrokeView {
  CAShapeLayer* _topStrokeLayer;
  CAShapeLayer* _bottomStrokeLayer;
  CAShapeLayer* _leftCornerLayer;
  CAShapeLayer* _rightCornerLayer;


  UIColor* _strokeColor;
  BOOL _fillLeftCorner;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    // Make the view transparent
    self.backgroundColor = UIColor.clearColor;

    // Setup top stroke layer
    _topStrokeLayer = [CAShapeLayer layer];
    _topStrokeLayer.fillColor = UIColor.clearColor.CGColor;
    _topStrokeLayer.lineWidth = TabStripCollectionViewConstants.groupStrokeLineWidth;

    // Setup bottom stroke layer
    _bottomStrokeLayer = [CAShapeLayer layer];
    _bottomStrokeLayer.fillColor = UIColor.clearColor.CGColor;
    _bottomStrokeLayer.lineWidth = TabStripCollectionViewConstants.groupStrokeLineWidth;

    // Setup left corner layer for last tab
    _leftCornerLayer = [CAShapeLayer layer];
    _leftCornerLayer.fillColor = UIColor.clearColor.CGColor;
    _leftCornerLayer.lineWidth = TabStripCollectionViewConstants.groupStrokeLineWidth;

    // Setup right corner layer for last tab
    _rightCornerLayer = [CAShapeLayer layer];
    _rightCornerLayer.fillColor = UIColor.clearColor.CGColor;
    _rightCornerLayer.lineWidth = TabStripCollectionViewConstants.groupStrokeLineWidth;

    // Add layers to the view
    [self.layer addSublayer:_topStrokeLayer];
    [self.layer addSublayer:_bottomStrokeLayer];
    [self.layer addSublayer:_leftCornerLayer];
    [self.layer addSublayer:_rightCornerLayer];

    // Set up self
    self.translatesAutoresizingMaskIntoConstraints = NO;
  }
  return self;
}

- (void)layoutSubviews {
  [super layoutSubviews];
  _topStrokeLayer.frame = self.bounds;
  _bottomStrokeLayer.frame = self.bounds;
  _leftCornerLayer.frame = self.bounds;
  _rightCornerLayer.frame = self.bounds;
}

- (void)setBackgroundColor:(UIColor*)color {
  [super setBackgroundColor:color];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  _topStrokeLayer.strokeColor = _strokeColor.CGColor;
  _bottomStrokeLayer.strokeColor = _strokeColor.CGColor;
  _leftCornerLayer.strokeColor = _strokeColor.CGColor;
  if (_fillLeftCorner) {
    _leftCornerLayer.fillColor = _strokeColor.CGColor;
  } else {
    _leftCornerLayer.fillColor = UIColor.clearColor.CGColor;
  }
  _rightCornerLayer.strokeColor = _strokeColor.CGColor;
}

// Methods to set the paths

- (void)setStrokeColor:(UIColor*)color {
  if (_strokeColor == color)
    return;
  _strokeColor = color;
  _topStrokeLayer.strokeColor = color.CGColor;
  _bottomStrokeLayer.strokeColor = color.CGColor;
  _leftCornerLayer.strokeColor = color.CGColor;
  if (_fillLeftCorner) {
    _leftCornerLayer.fillColor = color.CGColor;
  } else {
    _leftCornerLayer.fillColor = UIColor.clearColor.CGColor;
  }
  _rightCornerLayer.strokeColor = color.CGColor;
}

- (void)setTopPath:(CGPathRef)path {
  _topStrokeLayer.path = path;
}

- (void)setBottomPath:(CGPathRef)path {
  _bottomStrokeLayer.path = path;
}

- (void)setLeftCornerPath:(CGPathRef)path {
  _leftCornerLayer.path = path;
}

- (void)setLeftCornerFill:(BOOL)fillLeftCorner {
  _fillLeftCorner = fillLeftCorner;
  if (fillLeftCorner) {
    _leftCornerLayer.fillColor = _leftCornerLayer.strokeColor;
  } else {
    _leftCornerLayer.fillColor = UIColor.clearColor.CGColor;
  }
}

- (void)setRightCornerPath:(CGPathRef)path {
  _rightCornerLayer.path = path;
}

@end
