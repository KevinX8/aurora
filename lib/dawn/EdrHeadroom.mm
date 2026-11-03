// Apple EDR headroom query: the screen's extended-range maximum over its SDR white.
#import <Foundation/Foundation.h>
#if TARGET_OS_IPHONE || TARGET_OS_TV
#import <UIKit/UIKit.h>
#else
#import <AppKit/AppKit.h>
#endif

#include "webgpu/hdr.hpp"

namespace aurora::webgpu {

float aurora_edr_headroom() {
#if TARGET_OS_IPHONE || TARGET_OS_TV
  // Live headroom tracks brightness and Low Power Mode on iOS.
  if (@available(iOS 16.4, tvOS 16.4, *)) {
    return (float)UIScreen.mainScreen.currentExtendedEDRContentHeadroom.maximumEDRHeadroom;
  }
  return 1.f;
#else
  // shortcut: macOS public APIs expose the screen's maximum, not the live brightness-adjusted
  // headroom; upgrade if a public current-headroom API appears.
  if (@available(macOS 10.11, *)) {
    return (float)NSScreen.mainScreen.maximumExtendedDynamicRangeColorComponentValue;
  }
  return 1.f;
#endif
}

} // namespace aurora::webgpu
