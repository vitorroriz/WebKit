/*
 * Copyright (C) 2006-2021 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "Pattern.h"

#include "Image.h"
#include "ImageBuffer.h"
#include "NativeImage.h"

namespace WebCore {

Ref<Pattern> Pattern::create(SourceImage&& tileImage, const Parameters& parameters)
{
    return adoptRef(*new Pattern(WTFMove(tileImage), parameters));
}

Pattern::Pattern(SourceImage&& tileImage, const Parameters& parameters)
    : m_tileImage(WTFMove(tileImage))
    , m_parameters(parameters)
{
}

Pattern::~Pattern() = default;

void Pattern::setPatternSpaceTransform(const AffineTransform& patternSpaceTransform)
{
    m_parameters.patternSpaceTransform = patternSpaceTransform;
}

const SourceImage& Pattern::tileImage() const
{
    return m_tileImage;
}

RefPtr<NativeImage> Pattern::tileNativeImage() const
{
    return m_tileImage.nativeImage();
}

RefPtr<ImageBuffer> Pattern::tileImageBuffer() const
{
    return m_tileImage.imageBuffer();
}

void Pattern::setTileImage(SourceImage&& tileImage)
{
    m_tileImage = WTFMove(tileImage);
}

}
