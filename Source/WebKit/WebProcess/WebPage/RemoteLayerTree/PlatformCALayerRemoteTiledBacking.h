/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#pragma once

#include "PlatformCALayerRemote.h"
#include <WebCore/TileController.h>

namespace WebKit {

class PlatformCALayerRemoteTiledBacking final : public PlatformCALayerRemote {
    friend class PlatformCALayerRemote;
public:
    virtual ~PlatformCALayerRemoteTiledBacking();

private:
    PlatformCALayerRemoteTiledBacking(WebCore::PlatformCALayer::LayerType, WebCore::PlatformCALayerClient* owner, RemoteLayerTreeContext&);

    WebCore::TiledBacking* tiledBacking() final { return m_tileController.ptr(); }

    void setNeedsDisplayInRect(const WebCore::FloatRect& dirtyRect) override;
    void setNeedsDisplay() override;

    const WebCore::PlatformCALayerList* customSublayers() const override;

    void setBounds(const WebCore::FloatRect&) override;
    
    bool isOpaque() const override;
    void setOpaque(bool) override;
    
    bool acceleratesDrawing() const override;
    void setAcceleratesDrawing(bool) override;

#if HAVE(SUPPORT_HDR_DISPLAY)
    bool setNeedsDisplayIfEDRHeadroomExceeds(float) override;
    void setTonemappingEnabled(bool) override;
#endif

    WebCore::ContentsFormat contentsFormat() const override;
    void setContentsFormat(WebCore::ContentsFormat) override;

    float contentsScale() const override;
    void setContentsScale(float) override;
    
    void setBorderWidth(float) override;
    void setBorderColor(const WebCore::Color&) override;

    const UniqueRef<WebCore::TileController> m_tileController;
    mutable WebCore::PlatformCALayerList m_customSublayers;
};

} // namespace WebKit
