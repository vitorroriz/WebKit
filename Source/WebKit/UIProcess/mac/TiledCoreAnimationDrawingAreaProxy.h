/*
 * Copyright (C) 2011-2020 Apple Inc. All rights reserved.
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

#if ENABLE(TILED_CA_DRAWING_AREA)

#include "DrawingAreaProxy.h"
#include <wtf/RefCounted.h>

namespace WebKit {

class TiledCoreAnimationDrawingAreaProxy final : public DrawingAreaProxy, public RefCounted<TiledCoreAnimationDrawingAreaProxy> {
    WTF_MAKE_TZONE_ALLOCATED(TiledCoreAnimationDrawingAreaProxy);
    WTF_MAKE_NONCOPYABLE(TiledCoreAnimationDrawingAreaProxy);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(TiledCoreAnimationDrawingAreaProxy);
public:
    static Ref<TiledCoreAnimationDrawingAreaProxy> create(WebPageProxy&, WebProcessProxy&);
    virtual ~TiledCoreAnimationDrawingAreaProxy();

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

private:
    TiledCoreAnimationDrawingAreaProxy(WebPageProxy&, WebProcessProxy&);

#if ENABLE(TILED_CA_DRAWING_AREA)
    DrawingAreaType type() const final { return DrawingAreaType::TiledCoreAnimation; }
#endif

    // DrawingAreaProxy
    void deviceScaleFactorDidChange(CompletionHandler<void()>&&) override;
    void sizeDidChange() override;
    void colorSpaceDidChange() override;
    void minimumSizeForAutoLayoutDidChange() override;
    void sizeToContentAutoSizeMaximumSizeDidChange() override;

    void enterAcceleratedCompositingMode(uint64_t backingStoreStateID, const LayerTreeContext&) override;
    void updateAcceleratedCompositingMode(uint64_t backingStoreStateID, const LayerTreeContext&) override;
    void didFirstLayerFlush(uint64_t /* backingStoreStateID */, const LayerTreeContext&) override;

    void adjustTransientZoom(double scale, WebCore::FloatPoint originInLayerForPageScale, WebCore::FloatPoint originInVisibleRect) override;
    void commitTransientZoom(double scale, WebCore::FloatPoint originInLayerForPageScale) override;

    void waitForDidUpdateActivityState(ActivityStateChangeID) override;
    void dispatchPresentationCallbacksAfterFlushingLayers(IPC::Connection&, Vector<IPC::AsyncReplyID>&&) final;

    std::optional<WebCore::FramesPerSecond> displayNominalFramesPerSecond() final;

    void willSendUpdateGeometry();

    WTF::MachSendRight createFence() override;

    bool shouldSendWheelEventsToEventDispatcher() const override { return true; }

    void didUpdateGeometry();

    void sendUpdateGeometry();

    // Whether we're waiting for a DidUpdateGeometry message from the web process.
    bool m_isWaitingForDidUpdateGeometry { false };

    // The last size we sent to the web process.
    WebCore::IntSize m_lastSentSize;

    // The last minimum layout size we sent to the web process.
    WebCore::IntSize m_lastSentMinimumSizeForAutoLayout;

    // The last maxmium size for size-to-content auto-sizing we sent to the web process.
    WebCore::IntSize m_lastSentSizeToContentAutoSizeMaximumSize;
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_DRAWING_AREA_PROXY(TiledCoreAnimationDrawingAreaProxy, DrawingAreaType::TiledCoreAnimation)

#endif // ENABLE(TILED_CA_DRAWING_AREA)
