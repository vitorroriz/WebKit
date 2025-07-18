/*
 * Copyright (C) 2020 Igalia S.L. All rights reserved.
 * Copyright (C) 2023 Apple, Inc. All rights reserved.
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

#include "config.h"
#include "WebXRWebGLLayer.h"

#if ENABLE(WEBXR)

#include "ContextDestructionObserverInlines.h"
#include "ExceptionOr.h"
#include "HTMLCanvasElement.h"
#include "IntSize.h"
#include "OffscreenCanvas.h"
#include "WebGL2RenderingContext.h"
#include "WebGLFramebuffer.h"
#include "WebGLRenderingContext.h"
#include "WebGLRenderingContextBase.h"
#include "WebXRFrame.h"
#include "WebXROpaqueFramebuffer.h"
#include "WebXRSession.h"
#include "WebXRView.h"
#include "WebXRViewport.h"
#include "XRWebGLLayerInit.h"
#include <wtf/Scope.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(WebXRWebGLLayer);

// Arbitrary value for minimum framebuffer scaling.
// Below this threshold the resulting framebuffer would be too small to see.
constexpr double MinFramebufferScalingFactor = 0.2;

static ExceptionOr<std::unique_ptr<WebXROpaqueFramebuffer>> createOpaqueFramebuffer(WebXRSession& session, WebGLRenderingContextBase& context, const XRWebGLLayerInit& init)
{
    auto device = session.device();
    if (!device)
        return Exception { ExceptionCode::OperationError, "Cannot create an XRWebGLLayer with an XRSession that has ended."_s };

    // 9.1. Initialize layer’s antialias to layerInit’s antialias value.
    // 9.2. Let framebufferSize be the recommended WebGL framebuffer resolution multiplied by layerInit's framebufferScaleFactor.

    float scaleFactor = std::clamp(init.framebufferScaleFactor, MinFramebufferScalingFactor, device->maxFramebufferScalingFactor());

    FloatSize recommendedSize = session.recommendedWebGLFramebufferResolution();
    IntSize size = expandedIntSize(recommendedSize.scaled(scaleFactor));

    // 9.3. Initialize layer’s framebuffer to a new opaque framebuffer with the dimensions framebufferSize
    //      created with context, session initialized to session, and layerInit’s depth, stencil, and alpha values.
    // 9.4. Allocate and initialize resources compatible with session’s XR device, including GPU accessible memory buffers,
    //      as required to support the compositing of layer.
    // 9.5. If layer’s resources were unable to be created for any reason, throw an OperationError and abort these steps.
    auto layerHandle = device->createLayerProjection(size.width(), size.height(), init.alpha);
    if (!layerHandle)
        return Exception { ExceptionCode::OperationError, "Unable to allocate XRWebGLLayer GPU resources."_s };

    WebXROpaqueFramebuffer::Attributes attributes {
        .alpha = init.alpha,
        .antialias = init.antialias,
        .depth = init.depth,
        .stencil = init.stencil
    };

    auto framebuffer = WebXROpaqueFramebuffer::create(*layerHandle, context, WTFMove(attributes), size);
    if (!framebuffer)
        return Exception { ExceptionCode::OperationError, "Unable to create a framebuffer."_s };
    
    return framebuffer;
}

// https://immersive-web.github.io/webxr/#dom-xrwebgllayer-xrwebgllayer
ExceptionOr<Ref<WebXRWebGLLayer>> WebXRWebGLLayer::create(Ref<WebXRSession>&& session, WebXRRenderingContext&& context, const XRWebGLLayerInit& init)
{
    // 1. Let layer be a new XRWebGLLayer
    // 2. If session’s ended value is true, throw an InvalidStateError and abort these steps.
    if (session->ended())
        return Exception { ExceptionCode::InvalidStateError, "Cannot create an XRWebGLLayer with an XRSession that has ended."_s };

    // 3. If context is lost, throw an InvalidStateError and abort these steps.
    // 4. If session is an immersive session and context’s XR compatible boolean is false, throw
    //    an InvalidStateError and abort these steps.
    return WTF::switchOn(context,
        [&](const RefPtr<WebGLRenderingContextBase>& baseContext) -> ExceptionOr<Ref<WebXRWebGLLayer>>
        {
            if (baseContext->isContextLost())
                return Exception { ExceptionCode::InvalidStateError, "Cannot create an XRWebGLLayer with a lost WebGL context."_s };

            auto mode = session->mode();
            if ((mode == XRSessionMode::ImmersiveAr || mode == XRSessionMode::ImmersiveVr) && !baseContext->isXRCompatible())
                return Exception { ExceptionCode::InvalidStateError, "Cannot create an XRWebGLLayer with WebGL context not marked as XR compatible."_s };


            // 5. Initialize layer’s context to context. (see constructor)
            // 6. Initialize layer’s session to session. (see constructor)
            // 7. Initialize layer’s ignoreDepthValues as follows. (see constructor)
            //   7.1 If layerInit’s ignoreDepthValues value is false and the XR Compositor will make use of depth values,
            //       Initialize layer’s ignoreDepthValues to false.
            //   7.2. Else Initialize layer’s ignoreDepthValues to true
            // TODO: ask XR compositor for depth value usages support
            bool ignoreDepthValues = true;
            // 8. Initialize layer’s composition disabled boolean as follows.
            //    If session is an inline session -> Initialize layer's composition disabled to true
            //    Otherwise -> Initialize layer's composition disabled boolean to false
            bool isCompositionEnabled = session->mode() != XRSessionMode::Inline;
            bool antialias = false;
            std::unique_ptr<WebXROpaqueFramebuffer> framebuffer;

            // 9. If layer's composition enabled boolean is true: 
            if (isCompositionEnabled) {
                auto createResult = createOpaqueFramebuffer(session.get(), *baseContext, init);
                if (createResult.hasException())
                    return createResult.releaseException();
                framebuffer = createResult.releaseReturnValue();
            } else {
                // Otherwise
                // 9.1. Initialize layer’s antialias to layer’s context's actual context parameters antialias value.
                if (auto attributes = baseContext->getContextAttributes())
                    antialias = attributes->antialias;
                // 9.2 Initialize layer's framebuffer to null.
            }

            // 10. Return layer.
            return adoptRef(*new WebXRWebGLLayer(WTFMove(session), WTFMove(context), WTFMove(framebuffer), antialias, ignoreDepthValues, isCompositionEnabled));
        },
        [](std::monostate) {
            ASSERT_NOT_REACHED();
            return Exception { ExceptionCode::InvalidStateError };
        }
    );
}

WebXRWebGLLayer::WebXRWebGLLayer(Ref<WebXRSession>&& session, WebXRRenderingContext&& context, std::unique_ptr<WebXROpaqueFramebuffer>&& framebuffer,
    bool antialias, bool ignoreDepthValues, bool isCompositionEnabled)
    : WebXRLayer(session->scriptExecutionContext())
    , m_session(WTFMove(session))
    , m_context(WTFMove(context))
    , m_leftViewportData({ WebXRViewport::create({ }) })
    , m_rightViewportData({ WebXRViewport::create({ }) })
    , m_framebuffer(WTFMove(framebuffer))
    , m_antialias(antialias)
    , m_ignoreDepthValues(ignoreDepthValues)
    , m_isCompositionEnabled(isCompositionEnabled)
{
}

WebXRWebGLLayer::~WebXRWebGLLayer()
{
    auto canvasElement = canvas();
    if (canvasElement)
        canvasElement->removeObserver(*this);
}

bool WebXRWebGLLayer::antialias() const
{
    return m_antialias;
}

bool WebXRWebGLLayer::ignoreDepthValues() const
{
    return m_ignoreDepthValues;
}

const WebGLFramebuffer* WebXRWebGLLayer::framebuffer() const
{
    return m_framebuffer ? &m_framebuffer->framebuffer() : nullptr;
}

unsigned WebXRWebGLLayer::framebufferWidth() const
{
    if (m_framebuffer)
        return std::max<unsigned>(1, m_framebuffer->drawFramebufferSize().width());

    return WTF::switchOn(m_context,
        [&](const RefPtr<WebGLRenderingContextBase>& baseContext) {
            return std::max<unsigned>(1, baseContext->drawingBufferWidth());
        });
}

unsigned WebXRWebGLLayer::framebufferHeight() const
{
    if (m_framebuffer)
        return std::max<unsigned>(1, m_framebuffer->drawFramebufferSize().height());

    return WTF::switchOn(m_context,
        [&](const RefPtr<WebGLRenderingContextBase>& baseContext) {
            return std::max<unsigned>(1, baseContext->drawingBufferHeight());
        });
}

// https://immersive-web.github.io/webxr/#dom-xrwebgllayer-getviewport
ExceptionOr<RefPtr<WebXRViewport>> WebXRWebGLLayer::getViewport(WebXRView& view)
{
    // 1. Let session be view’s session.
    // 2. Let frame be session’s animation frame.
    // 3. If session is not equal to layer’s session, throw an InvalidStateError and abort these steps.
    if (&view.frame().session() != m_session.get())
        return Exception { ExceptionCode::InvalidStateError };

    // 4. If frame’s active boolean is false, throw an InvalidStateError and abort these steps.
    // 5. If view’s frame is not equal to frame, throw an InvalidStateError and abort these steps.
    if (!view.frame().isActive() || !view.frame().isAnimationFrame())
        return Exception { ExceptionCode::InvalidStateError };

    auto& viewportData = view.eye() == XREye::Right ? m_rightViewportData : m_leftViewportData;

    // 7. Set the view’s viewport modifiable flag to false.
    view.setViewportModifiable(false);

    computeViewports();

    // 8. Let viewport be the XRViewport from the list of viewport objects associated with view.
    // 9. Return viewport.
    auto result = RefPtr<WebXRViewport>(viewportData.viewport.copyRef());
    if (!result->width() || !result->height())
        result->updateViewport(IntRect(0, 0, 1, 1));

    return result;
}

double WebXRWebGLLayer::getNativeFramebufferScaleFactor(const WebXRSession& session)
{
    if (session.ended())
        return 0.0;

    IntSize nativeSize = session.nativeWebGLFramebufferResolution();
    IntSize recommendedSize = session.recommendedWebGLFramebufferResolution();

    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!recommendedSize.isZero());
    return (nativeSize / recommendedSize).width();
}

HTMLCanvasElement* WebXRWebGLLayer::canvas() const
{
    return WTF::switchOn(m_context, [](const RefPtr<WebGLRenderingContextBase>& baseContext) {
        auto canvas = baseContext->canvas();
        return WTF::switchOn(canvas, [](const RefPtr<HTMLCanvasElement>& canvas) {
            return canvas.get();
        }, [](const RefPtr<OffscreenCanvas>) -> HTMLCanvasElement* {
            ASSERT_NOT_REACHED("baseLayer of a WebXRWebGLLayer must be an HTMLCanvasElement");
            return nullptr;
        });
    });
}

void WebXRWebGLLayer::sessionEnded()
{
#if PLATFORM(COCOA)
    ASSERT(m_session);

    if (m_framebuffer) {
        auto device = m_session->device();
        if (device)
            device->deleteLayer(m_framebuffer->handle());
        m_framebuffer = nullptr;
    }

    m_session = nullptr;
#endif
}

void WebXRWebGLLayer::startFrame(PlatformXR::FrameData& data)
{
    ASSERT(m_framebuffer);

    auto it = data.layers.find(m_framebuffer->handle());
    if (it == data.layers.end()) {
        // For some reason the device didn't provide a texture for this frame.
        // The frame is ignored and the device can recover the texture in future frames;
        return;
    }

    m_framebuffer->startFrame(it->value);
}

PlatformXR::Device::Layer WebXRWebGLLayer::endFrame()
{
    ASSERT(m_framebuffer);
    m_framebuffer->endFrame();

    Vector<PlatformXR::Device::LayerView> views {
        { PlatformXR::Eye::Left, m_leftViewportData.viewport->rect() },
        { PlatformXR::Eye::Right, m_rightViewportData.viewport->rect() }
    };

    return PlatformXR::Device::Layer { .handle = m_framebuffer->handle(), .visible = true, .views = WTFMove(views) };
}

void WebXRWebGLLayer::canvasResized(CanvasBase&)
{
}

// https://immersive-web.github.io/webxr/#xrview-obtain-a-scaled-viewport
void WebXRWebGLLayer::computeViewports()
{
    ASSERT(m_session);

    auto roundDown = [](IntSize size, double scale) -> IntSize {
        // Round down to integer value and ensure that the value is not zero.
        size.scale(scale);
        size.clampToMinimumSize({ 1, 1 });
        return size;
    };

    auto roundDownShared = [](double value) -> int {
        return std::max(1, static_cast<int>(std::floor(value)));
    };

    auto width = framebufferWidth();
    auto height = framebufferHeight();

    if (m_session->mode() == XRSessionMode::ImmersiveVr && m_session->views().size() > 1) {
        if (m_framebuffer && m_framebuffer->usesLayeredMode()) {
            auto scale = m_leftViewportData.currentScale;
            auto viewport = m_framebuffer->drawViewport(PlatformXR::Eye::Left);
            viewport.setSize(roundDown(viewport.size(), scale));
            m_leftViewportData.viewport->updateViewport(viewport);

            scale = m_rightViewportData.currentScale;
            viewport = m_framebuffer->drawViewport(PlatformXR::Eye::Right);
            viewport.setSize(roundDown(viewport.size(), scale));
            m_rightViewportData.viewport->updateViewport(viewport);
            return;
        }

        auto leftScale = m_leftViewportData.currentScale;
        m_leftViewportData.viewport->updateViewport(IntRect(0, 0, roundDownShared(width * 0.5 * leftScale), roundDownShared(height * leftScale)));
        auto rightScale = m_rightViewportData.currentScale;
        m_rightViewportData.viewport->updateViewport(IntRect(width * 0.5, 0, roundDownShared(width * 0.5 * rightScale), roundDownShared(height * rightScale)));
    } else {
        auto viewport = m_framebuffer ? m_framebuffer->drawViewport(PlatformXR::Eye::None) : IntRect(0, 0, framebufferWidth(), framebufferHeight());
        m_leftViewportData.viewport->updateViewport(viewport);
    }
}

} // namespace WebCore

#endif // ENABLE(WEBXR)
