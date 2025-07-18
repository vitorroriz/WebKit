/*
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2008-2009 Torch Mobile, Inc.
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "ScalableImageDecoder.h"

#include "NotImplemented.h"
#include "SharedBuffer.h"
#include <wtf/TZoneMallocInlines.h>

#if !PLATFORM(COCOA)
#include "BMPImageDecoder.h"
#include "GIFImageDecoder.h"
#include "ICOImageDecoder.h"
#include "JPEGImageDecoder.h"
#include "PNGImageDecoder.h"
#include "WEBPImageDecoder.h"
#endif
#if USE(AVIF)
#include "AVIFImageDecoder.h"
#endif
#if USE(JPEGXL)
#include "JPEGXLImageDecoder.h"
#endif

#if USE(CG)
#include "ImageDecoderCG.h"
#include <ImageIO/ImageIO.h>
#endif

#include <algorithm>
#include <cmath>

#if PLATFORM(COCOA) && USE(JPEGXL)
#include <wtf/darwin/WeakLinking.h>

WTF_WEAK_LINK_FORCE_IMPORT(JxlSignatureCheck);
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ScalableImageDecoder);

namespace {

#if !PLATFORM(COCOA)
static bool matchesGIFSignature(std::span<const uint8_t> contents)
{
    return spanHasPrefix(contents, "GIF87a"_span) || spanHasPrefix(contents, "GIF89a"_span);
}

static bool matchesPNGSignature(std::span<const uint8_t> contents)
{
    return spanHasPrefix(contents, unsafeMakeSpan("\x89\x50\x4E\x47\x0D\x0A\x1A\x0A", 8));
}

static bool matchesJPEGSignature(std::span<const uint8_t> contents)
{
    return spanHasPrefix(contents, unsafeMakeSpan("\xFF\xD8\xFF", 3));
}

static bool matchesBMPSignature(std::span<const uint8_t> contents)
{
    return spanHasPrefix(contents, "BM"_span);
}

static bool matchesICOSignature(std::span<const uint8_t> contents)
{
    return spanHasPrefix(contents, unsafeMakeSpan("\x00\x00\x01\x00", 4));
}

static bool matchesCURSignature(std::span<const uint8_t> contents)
{
    return spanHasPrefix(contents, unsafeMakeSpan("\x00\x00\x02\x00", 4));
}

static bool matchesWebPSignature(std::span<const uint8_t> contents)
{
    return spanHasPrefix(contents, "RIFF"_span) && spanHasPrefix(contents.subspan(8), "WEBPVP"_span);
}
#endif

#if USE(AVIF)
static bool matchesAVIFSignature(std::span<const uint8_t> contents, FragmentedSharedBuffer& data)
{
#if USE(CG)
    UNUSED_PARAM(contents);
    auto sharedBuffer = data.makeContiguous();
    auto cfData = sharedBuffer->createCFData();
    auto imageSource = adoptCF(CGImageSourceCreateWithData(cfData.get(), nullptr));
    auto uti = ImageDecoderCG::decodeUTI(imageSource.get(), sharedBuffer.get());
    return uti == "public.avif"_s || uti == "public.avis"_s;
#else
    UNUSED_PARAM(data);
    return spanHasPrefix(contents.subspan(4), unsafeMakeSpan("\x66\x74\x79\x70", 4));
#endif
}
#endif // USE(AVIF)

#if USE(JPEGXL)
static bool matchesJPEGXLSignature(std::span<const uint8_t> contents)
{
#if PLATFORM(COCOA)
    if (!&JxlSignatureCheck)
        return false;
#endif
    JxlSignature signature = JxlSignatureCheck(contents.data(), contents.size());
    return signature != JXL_SIG_NOT_ENOUGH_BYTES && signature != JXL_SIG_INVALID;
}
#endif

} // Anonymous namespace

RefPtr<ScalableImageDecoder> ScalableImageDecoder::create(FragmentedSharedBuffer& data, AlphaOption alphaOption, GammaAndColorProfileOption gammaAndColorProfileOption)
{
    constexpr size_t lengthOfLongestSignature = 14; // To wit: "RIFF????WEBPVP"
    if (data.size() < lengthOfLongestSignature)
        return nullptr;

    std::array<uint8_t, lengthOfLongestSignature> contents;
    data.copyTo(std::span { contents });

    std::span contentsSpan { contents };

#if !PLATFORM(COCOA)
    if (matchesGIFSignature(contentsSpan))
        return GIFImageDecoder::create(alphaOption, gammaAndColorProfileOption);

    if (matchesPNGSignature(contentsSpan))
        return PNGImageDecoder::create(alphaOption, gammaAndColorProfileOption);

    if (matchesICOSignature(contentsSpan) || matchesCURSignature(contentsSpan))
        return ICOImageDecoder::create(alphaOption, gammaAndColorProfileOption);

    if (matchesJPEGSignature(contentsSpan))
        return JPEGImageDecoder::create(alphaOption, gammaAndColorProfileOption);

    if (matchesBMPSignature(contentsSpan))
        return BMPImageDecoder::create(alphaOption, gammaAndColorProfileOption);

    if (matchesWebPSignature(contentsSpan))
        return WEBPImageDecoder::create(alphaOption, gammaAndColorProfileOption);
#endif

#if USE(AVIF)
    if (matchesAVIFSignature(contentsSpan, data))
        return AVIFImageDecoder::create(alphaOption, gammaAndColorProfileOption);
#else
    UNUSED_PARAM(alphaOption);
    UNUSED_PARAM(gammaAndColorProfileOption);
#endif

#if USE(JPEGXL)
    if (matchesJPEGXLSignature(contentsSpan))
        return JPEGXLImageDecoder::create(alphaOption, gammaAndColorProfileOption);
#endif

    return nullptr;
}

bool ScalableImageDecoder::frameIsCompleteAtIndex(size_t index) const
{
    Locker locker { m_lock };
    if (index >= m_frameBufferCache.size())
        return false;

    auto& frame = m_frameBufferCache[index];
    return frame.isComplete();
}

bool ScalableImageDecoder::frameHasAlphaAtIndex(size_t index) const
{
    Locker locker { m_lock };
    if (m_frameBufferCache.size() <= index)
        return true;

    auto& frame = m_frameBufferCache[index];
    if (!frame.isComplete())
        return true;
    return frame.hasAlpha();
}

Seconds ScalableImageDecoder::frameDurationAtIndex(size_t index) const
{
    Locker locker { m_lock };
    if (index >= m_frameBufferCache.size())
        return 0_s;

    auto& frame = m_frameBufferCache[index];
    if (!frame.isComplete())
        return 0_s;

    // Many annoying ads specify a 0 duration to make an image flash as quickly as possible.
    // We follow Firefox's behavior and use a duration of 100 ms for any frames that specify
    // a duration of <= 10 ms. See <rdar://problem/7689300> and <http://webkit.org/b/36082>
    // for more information.
    Seconds duration = frame.duration();
    if (duration < 11_ms)
        return 100_ms;
    return duration;
}

PlatformImagePtr ScalableImageDecoder::createFrameImageAtIndex(size_t index, SubsamplingLevel, const DecodingOptions&)
{
    Locker locker { m_lock };
    // Zero-height images can cause problems for some ports. If we have an empty image dimension, just bail.
    if (size().isEmpty())
        return nullptr;

    auto* buffer = frameBufferAtIndex(index);
    if (!buffer || buffer->isInvalid() || !buffer->hasBackingStore())
        return nullptr;

    // Return the buffer contents as a native image. For some ports, the data
    // is already in a native container, and this just increments its refcount.
    return buffer->backingStore()->image();
}

}
