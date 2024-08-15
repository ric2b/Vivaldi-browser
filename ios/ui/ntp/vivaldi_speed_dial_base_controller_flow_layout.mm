// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "ios/ui/ntp/vivaldi_speed_dial_base_controller_flow_layout.h"

@implementation VivaldiSpeedDialBaseControllerFlowLayout

- (void)prepareLayout {
  [super prepareLayout];
  UICollectionView *collectionView = self.collectionView;
  if (!collectionView) return;

  self.itemSize = CGSizeMake(collectionView.bounds.size.width,
                             collectionView.bounds.size.height);
  self.minimumLineSpacing = 0;
  self.minimumInteritemSpacing = 0;
  self.sectionInset = UIEdgeInsetsZero;
  self.scrollDirection = UICollectionViewScrollDirectionHorizontal;

  CGFloat offsetX = self.collectionView.contentOffset.x;
  CGFloat offsety = self.collectionView.contentOffset.y;
  CGFloat width = self.collectionView.frame.size.width;
  self.collectionView.contentOffset =
      CGPointMake(ceil(offsetX/width)*width, offsety);
}

@end
