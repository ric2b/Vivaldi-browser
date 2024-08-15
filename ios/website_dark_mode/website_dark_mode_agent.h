// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_WEBSITE_DARK_MODE_WEBSITE_DARK_MODE_AGENT_H_
#define IOS_WEBSITE_DARK_MODE_WEBSITE_DARK_MODE_AGENT_H_

#import <Foundation/Foundation.h>

class PrefService;

namespace web {
class WebState;
}

// Handles the actions for rendering the webpages in dark mode when Force Dark
// Web Pages setting is enabled. This class observes the changes to the prefs,
// webstate and frame to invoke the JS.
@interface WebsiteDarkModeAgent : NSObject

// Designated initializer. Arguments |prefService| and |webState| should not be
// null.
- (instancetype)initWithPrefService:(PrefService*)prefService
                           webState:(web::WebState*)webState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_WEBSITE_DARK_MODE_WEBSITE_DARK_MODE_AGENT_H_
