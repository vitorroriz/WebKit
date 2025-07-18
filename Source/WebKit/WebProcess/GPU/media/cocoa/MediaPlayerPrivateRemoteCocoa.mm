/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#import "MediaPlayerPrivateRemote.h"

#if ENABLE(GPU_PROCESS) && PLATFORM(COCOA)

#import "Logging.h"
#import "RemoteAudioSourceProvider.h"
#import "RemoteMediaPlayerProxyMessages.h"
#import "VideoLayerRemote.h"
#import <WebCore/ColorSpaceCG.h>
#import <WebCore/VideoLayerManager.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/MachSendRightAnnotated.h>

#import <WebCore/CoreVideoSoftLink.h>

namespace WebKit {
using namespace WebCore;

#if ENABLE(VIDEO_PRESENTATION_MODE)
PlatformLayerContainer MediaPlayerPrivateRemote::createVideoFullscreenLayer()
{
    return adoptNS([[CALayer alloc] init]);
}
#endif

void MediaPlayerPrivateRemote::pushVideoFrameMetadata(WebCore::VideoFrameMetadata&& videoFrameMetadata, RemoteVideoFrameProxy::Properties&& properties)
{
    auto videoFrame = RemoteVideoFrameProxy::create(protectedConnection(), protectedVideoFrameObjectHeapProxy(), WTFMove(properties));
    if (!m_isGatheringVideoFrameMetadata)
        return;
    m_videoFrameMetadata = WTFMove(videoFrameMetadata);
    m_videoFrameGatheredWithVideoFrameMetadata = WTFMove(videoFrame);
}

RefPtr<NativeImage> MediaPlayerPrivateRemote::nativeImageForCurrentTime()
{
    if (readyState() < MediaPlayer::ReadyState::HaveCurrentData)
        return { };

    RefPtr videoFrame = videoFrameForCurrentTime();
    if (!videoFrame)
        return nullptr;

    return WebProcess::singleton().ensureProtectedGPUProcessConnection()->protectedVideoFrameObjectHeapProxy()->getNativeImage(*videoFrame);
}

WebCore::DestinationColorSpace MediaPlayerPrivateRemote::colorSpace()
{
    if (readyState() < MediaPlayer::ReadyState::HaveCurrentData)
        return DestinationColorSpace::SRGB();

    auto sendResult = protectedConnection()->sendSync(Messages::RemoteMediaPlayerProxy::ColorSpace(), m_id);
    auto [colorSpace] = sendResult.takeReplyOr(DestinationColorSpace::SRGB());
    return colorSpace;
}

void MediaPlayerPrivateRemote::layerHostingContextChanged(WebCore::HostingContext&& inlineLayerHostingContext, const FloatSize& presentationSize)
{
    RELEASE_LOG_FORWARDABLE(Media, MEDIAPLAYERPRIVATEREMOTE_LAYERHOSTINGCONTEXTCHANGED);

    RefPtr player = m_player.get();
    if (!player)
        return;

    if (!inlineLayerHostingContext.contextID) {
        m_videoLayer = nullptr;
        m_videoLayerManager->didDestroyVideoLayer();
        return;
    }
    setLayerHostingContext(WTFMove(inlineLayerHostingContext));
    player->videoLayerSizeDidChange(presentationSize);
}

WebCore::FloatSize MediaPlayerPrivateRemote::videoLayerSize() const
{
    if (RefPtr player = m_player.get())
        return player->videoLayerSize();
    return { };
}

void MediaPlayerPrivateRemote::setVideoLayerSizeFenced(const FloatSize& size, WTF::MachSendRightAnnotated&& sendRightAnnotated)
{
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::SetVideoLayerSizeFenced(size, WTFMove(sendRightAnnotated)), m_id);
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && PLATFORM(COCOA)
