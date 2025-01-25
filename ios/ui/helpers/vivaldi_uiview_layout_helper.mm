// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"

@implementation UIView(VivaldiLayout)

#pragma mark:- SETTERS

// Applies constraint to the view with passed anchors alongside padding and size
- (void) anchorTop:(NSLayoutYAxisAnchor*)top
           leading:(NSLayoutXAxisAnchor*)leading
            bottom:(NSLayoutYAxisAnchor*)bottom
          trailing:(NSLayoutXAxisAnchor*)trailing
           padding:(UIEdgeInsets)padding
              size:(CGSize)size {
  self.translatesAutoresizingMaskIntoConstraints = NO;

  NSMutableArray* constraints = [[NSMutableArray alloc] init];

  // Top anchor
  if (top) {
    NSLayoutConstraint* topAnchor =
      [self.topAnchor
        constraintEqualToAnchor:top
          constant:padding.top];
    [constraints addObject:topAnchor];
  }

  // Leading/Left anchor
  if (leading) {
    NSLayoutConstraint* leadingAnchor =
      [self.leadingAnchor
        constraintEqualToAnchor:leading
          constant:padding.left];
    [constraints addObject: leadingAnchor];
  }

  // Bottom anchor
  if (bottom) {
    NSLayoutConstraint* bottomAnchor =
      [self.bottomAnchor
        constraintEqualToAnchor:bottom
          constant:-padding.bottom];
    [constraints addObject:bottomAnchor];
  }

  // Trailing/Right anchor
  if (trailing) {
    NSLayoutConstraint* trailingAnchor =
      [self.trailingAnchor
        constraintEqualToAnchor:trailing
          constant:-padding.right];
    [constraints addObject:trailingAnchor];
  }

  // Width anchor
  if (size.width != 0) {
    NSLayoutConstraint* widthAnchor =
      [self.widthAnchor constraintEqualToConstant:size.width];
    [constraints addObject:widthAnchor];
  }

  // Height anchor
  if (size.height != 0) {
    NSLayoutConstraint* heightAnchor =
      [self.heightAnchor constraintEqualToConstant:size.height];
    [constraints addObject:heightAnchor];
  }

  // Activate constraints
  [NSLayoutConstraint activateConstraints:constraints];
}

// Applies constraint to the view with passed anchors.
// The default padding is zero and size is not affected.
- (void) anchorTop:(NSLayoutYAxisAnchor*)top
           leading:(NSLayoutXAxisAnchor*)leading
            bottom:(NSLayoutYAxisAnchor*)bottom
          trailing:(NSLayoutXAxisAnchor*)trailing {
  self.translatesAutoresizingMaskIntoConstraints = NO;

  [self anchorTop:top
          leading:leading
           bottom:bottom
         trailing:trailing
          padding:UIEdgeInsetsZero
             size:CGSizeZero];
}

// Applies constraint to the view with passed anchors with padding.
// Size is not affected.

- (void) anchorTop:(NSLayoutYAxisAnchor*)top
           leading:(NSLayoutXAxisAnchor*)leading
            bottom:(NSLayoutYAxisAnchor*)bottom
          trailing:(NSLayoutXAxisAnchor*)trailing
           padding:(UIEdgeInsets)padding {
  self.translatesAutoresizingMaskIntoConstraints = NO;

  // When no size is supplied, default size is zero. But, the size will be
  // calculated in the run time based on the constraints of the child views.
  [self anchorTop:top
          leading:leading
           bottom:bottom
         trailing:trailing
          padding:padding
             size:CGSizeZero];
}

// Applies constraint to the view with passed anchors with size.
// The default padding is zero.
- (void) anchorTop:(NSLayoutYAxisAnchor*)top
           leading:(NSLayoutXAxisAnchor*)leading
            bottom:(NSLayoutYAxisAnchor*)bottom
          trailing:(NSLayoutXAxisAnchor*)trailing
              size:(CGSize)size {
  self.translatesAutoresizingMaskIntoConstraints = NO;

  // When no padding is supplied, default padding is zero.
  [self anchorTop:top
          leading:leading
           bottom:bottom
         trailing:trailing
          padding:UIEdgeInsetsZero
             size:size];
}

// The child view takes the size of the parent view with user provided padding.
- (void) fillSuperviewWithPadding:(UIEdgeInsets)padding {
  self.translatesAutoresizingMaskIntoConstraints = NO;

  if (self.superview == nil)
    return;

  // Superview anchors
  NSLayoutYAxisAnchor* superViewTopAnchor = self.superview.topAnchor;
  NSLayoutXAxisAnchor* superViewLeadingAnchor = self.superview.leadingAnchor;
  NSLayoutYAxisAnchor* superViewBottomAnchor = self.superview.bottomAnchor;
  NSLayoutXAxisAnchor* superViewTrailingAnchor = self.superview.trailingAnchor;

  NSMutableArray* constraints = [[NSMutableArray alloc] init];

  // Top anchor
  NSLayoutConstraint* topAnchor =
    [self.topAnchor
      constraintEqualToAnchor:superViewTopAnchor
        constant:padding.top];
  [constraints addObject:topAnchor];

  // Leading/Left anchor
  NSLayoutConstraint* leadingAnchor =
    [self.leadingAnchor
      constraintEqualToAnchor:superViewLeadingAnchor
        constant:padding.left];
  [constraints addObject: leadingAnchor];

  // Bottom anchor
  NSLayoutConstraint* bottomAnchor =
    [self.bottomAnchor
      constraintEqualToAnchor:superViewBottomAnchor
        constant:-padding.bottom];
  [constraints addObject:bottomAnchor];

  // Trailing/Right anchor
  NSLayoutConstraint* trailingAnchor =
    [self.trailingAnchor
      constraintEqualToAnchor:superViewTrailingAnchor
        constant:-padding.right];
  [constraints addObject:trailingAnchor];

  // Activate constraints
  [NSLayoutConstraint activateConstraints: constraints];
}

// The child view takes the size of the parent view without padding.
// The default padding is zero.
- (void) fillSuperview {
  [self fillSuperviewWithPadding:UIEdgeInsetsZero];
}

// The child view follows the safe area insets in all side with user provided
// padding.
- (void) fillSuperviewToSafeAreaInsetWithPadding:(UIEdgeInsets)padding {
  if (self.superview == nil)
    return;

  [self anchorTop:self.superview.safeTopAnchor
          leading:self.superview.safeLeftAnchor
           bottom:self.superview.safeBottomAnchor
         trailing:self.superview.safeRightAnchor
          padding:padding];
}

// The child view follows the safe area insets of the parent in all side
// without padding. The default padding is zero.
- (void) fillSuperviewToSafeAreaInset {
  [self fillSuperviewToSafeAreaInsetWithPadding:UIEdgeInsetsZero];
}


// The child view placed in the center of the superview with user provided size.
- (void) centerInSuperviewWithSize:(CGSize)size {
  self.translatesAutoresizingMaskIntoConstraints = NO;

  if (self.superview == nil)
    return;

  NSMutableArray* constraints = [[NSMutableArray alloc] init];

  // Center X in Superview
  NSLayoutConstraint* centerXAnchor =
  [self.centerXAnchor constraintEqualToAnchor:self.superview.centerXAnchor];
  [constraints addObject:centerXAnchor];

  // Center Y in Superview
  NSLayoutConstraint* centerYAnchor =
  [self.centerYAnchor constraintEqualToAnchor:self.superview.centerYAnchor];
  [constraints addObject:centerYAnchor];

  // Width anchor
  if (size.width != 0) {
    NSLayoutConstraint* widthAnchor =
      [self.widthAnchor constraintEqualToConstant:size.width];
    [constraints addObject:widthAnchor];
  }

  // Height anchor
  if (size.height != 0) {
    NSLayoutConstraint* heightAnchor =
      [self.heightAnchor constraintEqualToConstant:size.height];
    [constraints addObject:heightAnchor];
  }

  // Activate constraints
  [NSLayoutConstraint activateConstraints:constraints];
}

// The child view placed in the center of the superview.
// The size is calculated from the constaints of the child view of this view.
- (void) centerInSuperview {
  [self centerInSuperviewWithSize:CGSizeZero];
}

// Center the view horizontally within the superview
- (void) centerXInSuperview {
  self.translatesAutoresizingMaskIntoConstraints = NO;

  if (!self.superview)
    return;

  [[self.centerXAnchor
    constraintEqualToAnchor:self.superview.centerXAnchor] setActive:YES];
}

// Center the view vertically within the superview
- (void) centerYInSuperview {
  self.translatesAutoresizingMaskIntoConstraints = NO;

  if (!self.superview)
    return;

  [[self.centerYAnchor
    constraintEqualToAnchor:self.superview.centerYAnchor] setActive:YES];
}


- (void) centerToView:(UIView*)view {
  [self centerXToView:view];
  [self centerYToView:view];
}

// Center the view horizontally with respect to a given value
- (void) centerXToView:(UIView*)view {
  self.translatesAutoresizingMaskIntoConstraints = NO;

  if (!view)
    return;

  [[self.centerXAnchor
      constraintEqualToAnchor:view.centerXAnchor] setActive:YES];
}

// Center the view vertically with respect to a given value
- (void) centerYToView:(UIView*)view {
  self.translatesAutoresizingMaskIntoConstraints = NO;

  if (!view)
    return;

  [[self.centerYAnchor
    constraintEqualToAnchor:view.centerYAnchor] setActive:YES];
}

// Match the constraints of a given view
- (void) matchToView:(UIView*)view {
  [self matchToView:view
            padding:UIEdgeInsetsZero];
}

// Match the constraints of a given view with provided padding.
- (void) matchToView:(UIView*)view
             padding:(UIEdgeInsets)padding {
  self.translatesAutoresizingMaskIntoConstraints = NO;

  if (!view)
    return;

  NSMutableArray* constraints = [[NSMutableArray alloc] init];

  // Top anchor
  NSLayoutConstraint* topAnchor =
    [self.topAnchor
      constraintEqualToAnchor:view.topAnchor
        constant:padding.top];
  [constraints addObject:topAnchor];

  // Leading/Left anchor
  NSLayoutConstraint* leadingAnchor =
    [self.leadingAnchor
      constraintEqualToAnchor:view.leadingAnchor
        constant:padding.left];
  [constraints addObject: leadingAnchor];

  // Bottom anchor
  NSLayoutConstraint* bottomAnchor =
    [self.bottomAnchor
      constraintEqualToAnchor:view.bottomAnchor
        constant:-padding.bottom];
  [constraints addObject:bottomAnchor];

  // Trailing/Right anchor
  NSLayoutConstraint* trailingAnchor =
    [self.trailingAnchor
      constraintEqualToAnchor:view.trailingAnchor
        constant:-padding.right];
  [constraints addObject:trailingAnchor];

  // Activate constraints
  [NSLayoutConstraint activateConstraints: constraints];
}

// Set width for a view
- (void) setWidthWithConstant:(CGFloat)constant {
  self.translatesAutoresizingMaskIntoConstraints = NO;
  [[self.widthAnchor constraintEqualToConstant:constant] setActive:YES];
}

// Set height for a view
- (void) setHeightWithConstant:(CGFloat)constant {
  self.translatesAutoresizingMaskIntoConstraints = NO;
  [[self.heightAnchor constraintEqualToConstant:constant] setActive:YES];
}

// Set size for a view
- (void) setViewSize:(CGSize)size {
  self.translatesAutoresizingMaskIntoConstraints = NO;
  [self setWidthWithConstant:size.width];
  [self setHeightWithConstant:size.height];
}

//#pragma mark:- GETTERS

- (NSLayoutYAxisAnchor*)safeTopAnchor {
  return self.safeAreaLayoutGuide.topAnchor;
}

- (NSLayoutXAxisAnchor*)safeLeftAnchor {
  return self.safeAreaLayoutGuide.leadingAnchor;
}

- (NSLayoutXAxisAnchor*)safeRightAnchor {
  return self.safeAreaLayoutGuide.trailingAnchor;
}

- (NSLayoutYAxisAnchor*)safeBottomAnchor {
  return self.safeAreaLayoutGuide.bottomAnchor;
}

@end
