/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if USE(AVFOUNDATION)

#import <CoreMedia/CMFormatDescription.h>
#import <JavaScriptCore/JavaScriptCore.h>
#import <WebCore/CMUtilities.h>
#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <wtf/RetainPtr.h>

#import <pal/cf/CoreMediaSoftLink.h>

namespace TestWebKitAPI {

TEST(PAL, MediaTime)
{
    EXPECT_EQ(PAL::toMediaTime(PAL::toCMTime(MediaTime::invalidTime())), MediaTime::invalidTime());
}

#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)

static RetainPtr<CMFormatDescriptionRef> makeFormatDescriptionWithTencData(std::span<const uint8_t> tencData)
{
    auto data = adoptCF(CFDataCreate(kCFAllocatorDefault, tencData.data(), tencData.size()));
    auto extensions = adoptNS([[NSDictionary alloc] initWithObjectsAndKeys:(__bridge NSData *)data.get(), @"CommonEncryptionTrackEncryptionBox", nil]);
    CMFormatDescriptionRef desc = nullptr;
    PAL::CMVideoFormatDescriptionCreate(kCFAllocatorDefault, kCMVideoCodecType_H264, 1920, 1080, (__bridge CFDictionaryRef)extensions.get(), &desc);
    return adoptCF(desc);
}

TEST(CMUtilities, GetKeyIDsEmptyTencData)
{
    auto desc = makeFormatDescriptionWithTencData({ });
    ASSERT_TRUE(desc);
    EXPECT_TRUE(WebCore::getKeyIDs(desc.get()).isEmpty());
}

TEST(CMUtilities, GetKeyIDsTruncatedTencData)
{
    // 3 bytes — not enough to read the 4-byte version+flags field.
    constexpr uint8_t truncated[] = { 0x00, 0x00, 0x00 };
    auto desc = makeFormatDescriptionWithTencData(truncated);
    ASSERT_TRUE(desc);
    EXPECT_TRUE(WebCore::getKeyIDs(desc.get()).isEmpty());
}

TEST(CMUtilities, GetKeyIDsValidTencData)
{
    // Minimal v0 tenc box payload (no size/type prefix):
    // version(1) + flags(3) + reserved(1) + reserved(1) + defaultIsProtected(1)
    // + defaultPerSampleIVSize(1) + KID(16).
    constexpr uint8_t validTenc[] = {
        0x00, // version = 0
        0x00, 0x00, 0x00, // flags
        0x00, // reserved
        0x00, // reserved (v0)
        0x01, // defaultIsProtected = 1
        0x08, // defaultPerSampleIVSize = 8
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, // defaultKID (16 bytes)
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
    };
    auto desc = makeFormatDescriptionWithTencData(validTenc);
    ASSERT_TRUE(desc);
    auto keyIDs = WebCore::getKeyIDs(desc.get());
    ASSERT_EQ(keyIDs.size(), 1u);
    ASSERT_EQ(keyIDs[0]->size(), 16u);
    auto span = keyIDs[0]->span();
    for (size_t i = 0; i < 16; ++i)
        EXPECT_EQ(span[i], uint8_t(i));
}

TEST(CMUtilities, GetKeyIDsWithActiveGigacage)
{
    // Creating a JSContext initialises a JSC VM, which calls Gigacage::ensureGigacage()
    // and sets g_gigacageConfig.isEnabled = true — the same state as WebContent.
    @autoreleasepool {
        JSContext * jsContext = [[JSContext alloc] init];
        (void)jsContext;

        constexpr uint8_t validTenc[] = {
            0x00, // version = 0
            0x00, 0x00, 0x00, // flags
            0x00, // reserved
            0x00, // reserved (v0)
            0x01, // defaultIsProtected = 1
            0x08, // defaultPerSampleIVSize = 8
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, // defaultKID (16 bytes)
            0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
        };
        auto desc = makeFormatDescriptionWithTencData(validTenc);
        ASSERT_TRUE(desc);
        auto keyIDs = WebCore::getKeyIDs(desc.get());
        ASSERT_EQ(keyIDs.size(), 1u);
        ASSERT_EQ(keyIDs[0]->size(), 16u);
        auto span = keyIDs[0]->span();
        for (size_t i = 0; i < 16; ++i)
            EXPECT_EQ(span[i], uint8_t(i));
    }
}

#endif // ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)

} // namespace TestWebKitAPI

#endif // USE(AVFOUNDATION)
