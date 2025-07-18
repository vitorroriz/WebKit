/*
 * Copyright (C) 2021-2025 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS)

#include "RemoteDeviceProxy.h"
#include "WebGPUIdentifier.h"
#include <WebCore/WebGPUCommandEncoder.h>
#include <wtf/TZoneMalloc.h>

namespace WebKit::WebGPU {

class ConvertToBackingContext;

class RemoteCommandEncoderProxy final : public WebCore::WebGPU::CommandEncoder {
    WTF_MAKE_TZONE_ALLOCATED(RemoteCommandEncoderProxy);
public:
    static Ref<RemoteCommandEncoderProxy> create(RemoteGPUProxy& root, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier)
    {
        return adoptRef(*new RemoteCommandEncoderProxy(root, convertToBackingContext, identifier));
    }

    virtual ~RemoteCommandEncoderProxy();

    RemoteGPUProxy& root() const { return m_root; }

private:
    friend class DowncastConvertToBackingContext;

    RemoteCommandEncoderProxy(RemoteGPUProxy&, ConvertToBackingContext&, WebGPUIdentifier);

    RemoteCommandEncoderProxy(const RemoteCommandEncoderProxy&) = delete;
    RemoteCommandEncoderProxy(RemoteCommandEncoderProxy&&) = delete;
    RemoteCommandEncoderProxy& operator=(const RemoteCommandEncoderProxy&) = delete;
    RemoteCommandEncoderProxy& operator=(RemoteCommandEncoderProxy&&) = delete;

    WebGPUIdentifier backing() const { return m_backing; }
    
    template<typename T>
    WARN_UNUSED_RETURN IPC::Error send(T&& message)
    {
        return root().protectedStreamClientConnection()->send(WTFMove(message), backing());
    }
    template<typename T>
    WARN_UNUSED_RETURN IPC::Connection::SendSyncResult<T> sendSync(T&& message)
    {
        return root().protectedStreamClientConnection()->sendSync(WTFMove(message), backing());
    }

    RefPtr<WebCore::WebGPU::RenderPassEncoder> beginRenderPass(const WebCore::WebGPU::RenderPassDescriptor&) final;
    RefPtr<WebCore::WebGPU::ComputePassEncoder> beginComputePass(const std::optional<WebCore::WebGPU::ComputePassDescriptor>&) final;

    void copyBufferToBuffer(
        const WebCore::WebGPU::Buffer& source,
        WebCore::WebGPU::Size64 sourceOffset,
        const WebCore::WebGPU::Buffer& destination,
        WebCore::WebGPU::Size64 destinationOffset,
        WebCore::WebGPU::Size64) final;

    void copyBufferToTexture(
        const WebCore::WebGPU::ImageCopyBuffer& source,
        const WebCore::WebGPU::ImageCopyTexture& destination,
        const WebCore::WebGPU::Extent3D& copySize) final;

    void copyTextureToBuffer(
        const WebCore::WebGPU::ImageCopyTexture& source,
        const WebCore::WebGPU::ImageCopyBuffer& destination,
        const WebCore::WebGPU::Extent3D& copySize) final;

    void copyTextureToTexture(
        const WebCore::WebGPU::ImageCopyTexture& source,
        const WebCore::WebGPU::ImageCopyTexture& destination,
        const WebCore::WebGPU::Extent3D& copySize) final;

    void clearBuffer(
        const WebCore::WebGPU::Buffer&,
        WebCore::WebGPU::Size64 offset = 0,
        std::optional<WebCore::WebGPU::Size64> = std::nullopt) final;

    void pushDebugGroup(String&& groupLabel) final;
    void popDebugGroup() final;
    void insertDebugMarker(String&& markerLabel) final;

    void writeTimestamp(const WebCore::WebGPU::QuerySet&, WebCore::WebGPU::Size32 queryIndex) final;

    void resolveQuerySet(
        const WebCore::WebGPU::QuerySet&,
        WebCore::WebGPU::Size32 firstQuery,
        WebCore::WebGPU::Size32 queryCount,
        const WebCore::WebGPU::Buffer& destination,
        WebCore::WebGPU::Size64 destinationOffset) final;

    RefPtr<WebCore::WebGPU::CommandBuffer> finish(const WebCore::WebGPU::CommandBufferDescriptor&) final;

    void setLabelInternal(const String&) final;

    WebGPUIdentifier m_backing;
    const Ref<ConvertToBackingContext> m_convertToBackingContext;
    const Ref<RemoteGPUProxy> m_root;
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
