/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"

#import <CoreFoundation/CoreFoundation.h>
#import <WebCore/CaptionUserPreferencesMediaAF.h>
#import <WebCore/MediaAccessibilitySoftLink.h>
#import <WebCore/PageGroup.h>
#import "SoftLinkShim.h"

namespace TestWebKitAPI {

using namespace WebCore;

#define SOFT_LINK_SHIM(framework, functionName, resultType, args...) \
    static resultType functionName##Result; \
    static resultType shimmed##functionName(args) { return functionName##Result; } \
    SoftLinkShim<resultType, args> functionName##Shim { &softLink##framework##functionName, shimmed##functionName }; \

#define SOFT_LINK_SHIM_CF_COPY(framework, functionName, resultType, args...) \
static RetainPtr<resultType> functionName##Result; \
    static resultType shimmed##functionName(args) { return (resultType)CFRetain(functionName##Result.get()); } \
    SoftLinkShim<resultType, args> functionName##Shim { &softLink##framework##functionName, shimmed##functionName }; \

#define DOMAIN_AND_BEHAVIOR MACaptionAppearanceDomain, MACaptionAppearanceBehavior*

class MediaAccessibilityShim {
public:
    SOFT_LINK_SHIM(MediaAccessibility, MACaptionAppearanceGetDisplayType, MACaptionAppearanceDisplayType, MACaptionAppearanceDomain);
    SOFT_LINK_SHIM_CF_COPY(MediaAccessibility, MACaptionAppearanceCopyForegroundColor, CGColorRef, DOMAIN_AND_BEHAVIOR);
    SOFT_LINK_SHIM_CF_COPY(MediaAccessibility, MACaptionAppearanceCopyBackgroundColor, CGColorRef, DOMAIN_AND_BEHAVIOR);
    SOFT_LINK_SHIM_CF_COPY(MediaAccessibility, MACaptionAppearanceCopyWindowColor, CGColorRef, DOMAIN_AND_BEHAVIOR);
    SOFT_LINK_SHIM(MediaAccessibility, MACaptionAppearanceGetForegroundOpacity, CGFloat, DOMAIN_AND_BEHAVIOR);
    SOFT_LINK_SHIM(MediaAccessibility, MACaptionAppearanceGetBackgroundOpacity, CGFloat, DOMAIN_AND_BEHAVIOR);
    SOFT_LINK_SHIM(MediaAccessibility, MACaptionAppearanceGetWindowOpacity, CGFloat, DOMAIN_AND_BEHAVIOR);
    SOFT_LINK_SHIM(MediaAccessibility, MACaptionAppearanceGetWindowRoundedCornerRadius, CGFloat, DOMAIN_AND_BEHAVIOR);
    SOFT_LINK_SHIM(MediaAccessibility, MACaptionAppearanceGetRelativeCharacterSize, CGFloat, DOMAIN_AND_BEHAVIOR);
    SOFT_LINK_SHIM_CF_COPY(MediaAccessibility, MACaptionAppearanceCopyFontDescriptorForStyle, CTFontDescriptorRef, DOMAIN_AND_BEHAVIOR, MACaptionAppearanceFontStyle)
    SOFT_LINK_SHIM_CF_COPY(MediaAccessibility, MACaptionAppearanceCopySelectedLanguages, CFArrayRef, MACaptionAppearanceDomain);
    SOFT_LINK_SHIM_CF_COPY(MediaAccessibility, MACaptionAppearanceCopyPreferredCaptioningMediaCharacteristics, CFArrayRef, MACaptionAppearanceDomain);
    SOFT_LINK_SHIM_CF_COPY(MediaAccessibility, MACaptionAppearanceCopyFontDescriptorWithStrokeForStyle, CTFontDescriptorRef, DOMAIN_AND_BEHAVIOR, MACaptionAppearanceFontStyle, CFStringRef, CGFloat, CGFloat *);
};

#define SOFT_LINK_SHIM_SET_RESULT(functionName, defaultValue) \
MediaAccessibilityShim::functionName##Result = defaultValue; \

#define SOFT_LINK_SHIM_DEFAULT_RESULT(functionName, defaultValue) \
auto MediaAccessibilityShim::functionName##Result { defaultValue }; \

SOFT_LINK_SHIM_DEFAULT_RESULT(MACaptionAppearanceGetDisplayType, kMACaptionAppearanceDisplayTypeAutomatic);
SOFT_LINK_SHIM_DEFAULT_RESULT(MACaptionAppearanceCopyForegroundColor, adoptCF(CGColorGetConstantColor(kCGColorWhite)));
SOFT_LINK_SHIM_DEFAULT_RESULT(MACaptionAppearanceCopyBackgroundColor, adoptCF(CGColorGetConstantColor(kCGColorBlack)));
SOFT_LINK_SHIM_DEFAULT_RESULT(MACaptionAppearanceCopyWindowColor, adoptCF(CGColorGetConstantColor(kCGColorClear)));
SOFT_LINK_SHIM_DEFAULT_RESULT(MACaptionAppearanceCopyFontDescriptorForStyle, adoptCF(CTFontDescriptorCreateWithNameAndSize(CFSTR(".AppleSystemUIFont"), 10)));
SOFT_LINK_SHIM_DEFAULT_RESULT(MACaptionAppearanceGetForegroundOpacity, 1.);
SOFT_LINK_SHIM_DEFAULT_RESULT(MACaptionAppearanceGetBackgroundOpacity, 1.);
SOFT_LINK_SHIM_DEFAULT_RESULT(MACaptionAppearanceGetWindowOpacity, 0.);
SOFT_LINK_SHIM_DEFAULT_RESULT(MACaptionAppearanceGetWindowRoundedCornerRadius, (CGFloat)0.f);
SOFT_LINK_SHIM_DEFAULT_RESULT(MACaptionAppearanceCopySelectedLanguages, adoptCF((__bridge CFArrayRef)@[@"English"]));
SOFT_LINK_SHIM_DEFAULT_RESULT(MACaptionAppearanceCopyPreferredCaptioningMediaCharacteristics, adoptCF((__bridge CFArrayRef)@[@"MAMediaCharacteristicDescribesMusicAndSoundForAccessibility"]));
SOFT_LINK_SHIM_DEFAULT_RESULT(MACaptionAppearanceCopyFontDescriptorWithStrokeForStyle, adoptCF(CTFontDescriptorCreateWithNameAndSize(CFSTR(".AppleSystemUIFont"), 10)));
SOFT_LINK_SHIM_DEFAULT_RESULT(MACaptionAppearanceGetRelativeCharacterSize, (CGFloat)1.f);

TEST(CaptionPreferenceTests, ShimTest)
{
    auto originalOpacity = MACaptionAppearanceGetForegroundOpacity(kMACaptionAppearanceDomainDefault, nullptr);
    {
        MediaAccessibilityShim shim;
        SOFT_LINK_SHIM_SET_RESULT(MACaptionAppearanceGetForegroundOpacity, 0.5f);

        EXPECT_EQ(0.5f, MACaptionAppearanceGetForegroundOpacity(kMACaptionAppearanceDomainDefault, nullptr));
    }

    EXPECT_EQ(originalOpacity, MACaptionAppearanceGetForegroundOpacity(kMACaptionAppearanceDomainDefault, nullptr));
}

TEST(CaptionPreferenceTests, FontFace)
{
    MediaAccessibilityShim shim;

    PageGroup group { "CaptionPreferenceTests"_s };
    auto preferences = CaptionUserPreferencesMediaAF::create(group);

    SOFT_LINK_SHIM_SET_RESULT(MACaptionAppearanceCopyFontDescriptorForStyle, adoptCF(CTFontDescriptorCreateWithNameAndSize(CFSTR(".AppleSystemUIFontMonospaced-Romulan"), 10)));
    EXPECT_EQ(preferences->captionsDefaultFontCSS(), "font-family: \"ui-monospace\";"_s);

    SOFT_LINK_SHIM_SET_RESULT(MACaptionAppearanceCopyFontDescriptorForStyle, adoptCF(CTFontDescriptorCreateWithNameAndSize(CFSTR(".AppleSystemUIFont-Klingon"), 10)));
    EXPECT_EQ(preferences->captionsDefaultFontCSS(), "font-family: \"system-ui\";"_s);

    SOFT_LINK_SHIM_SET_RESULT(MACaptionAppearanceCopyFontDescriptorForStyle, adoptCF(CTFontDescriptorCreateWithNameAndSize(CFSTR("WingDings"), 10)));
    EXPECT_EQ(preferences->captionsDefaultFontCSS(), "font-family: \"WingDings\";"_s);
}

}
