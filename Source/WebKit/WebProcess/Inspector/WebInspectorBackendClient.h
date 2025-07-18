/*
 * Copyright (C) 2010, 2015 Apple Inc. All rights reserved.
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

#pragma once

#include <WebCore/InspectorBackendClient.h>
#include <WebCore/PageOverlay.h>
#include <wtf/HashSet.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {
class GraphicsContext;
class GraphicsLayer;
class IntRect;
class PageOverlay;
}

namespace WebKit {

class WebPage;
class RepaintIndicatorLayerClient;

class WebInspectorBackendClient : public WebCore::InspectorBackendClient, private WebCore::PageOverlayClient {
    WTF_MAKE_TZONE_ALLOCATED(WebInspectorBackendClient);
friend class RepaintIndicatorLayerClient;
public:
    WebInspectorBackendClient(WebPage*);
    virtual ~WebInspectorBackendClient();

private:
    // WebCore::InspectorBackendClient
    void inspectedPageDestroyed() override;
    void frontendCountChanged(unsigned) override;

    Inspector::FrontendChannel* openLocalFrontend(WebCore::InspectorController*) override;
    void bringFrontendToFront() override;
    void didResizeMainFrame(WebCore::LocalFrame*) override;

    void highlight() override;
    void hideHighlight() override;

#if PLATFORM(IOS_FAMILY)
    void showInspectorIndication() override;
    void hideInspectorIndication() override;

    void didSetSearchingForNode(bool) override;
#endif

    void elementSelectionChanged(bool) override;
    void timelineRecordingChanged(bool) override;

    bool overridesShowPaintRects() const override { return true; }
    void showPaintRect(const WebCore::FloatRect&) override;
    unsigned paintRectCount() const override { return m_paintRectLayers.size(); }

    void setDeveloperPreferenceOverride(WebCore::InspectorBackendClient::DeveloperPreference, std::optional<bool>) final;
#if ENABLE(INSPECTOR_NETWORK_THROTTLING)
    bool setEmulatedConditions(std::optional<int64_t>&& bytesPerSecondLimit) final;
#endif

    // PageOverlayClient
    void willMoveToPage(WebCore::PageOverlay&, WebCore::Page*) override;
    void didMoveToPage(WebCore::PageOverlay&, WebCore::Page*) override;
    void drawRect(WebCore::PageOverlay&, WebCore::GraphicsContext&, const WebCore::IntRect&) override;
    bool mouseEvent(WebCore::PageOverlay&, const WebCore::PlatformMouseEvent&) override;

    void animationEndedForLayer(const WebCore::GraphicsLayer*);

    WeakPtr<WebPage> m_page;
    WeakPtr<WebCore::PageOverlay> m_highlightOverlay;

    RefPtr<WebCore::PageOverlay> m_paintRectOverlay;
    std::unique_ptr<RepaintIndicatorLayerClient> m_paintIndicatorLayerClient;
    HashSet<Ref<WebCore::GraphicsLayer>> m_paintRectLayers;
};

} // namespace WebKit
