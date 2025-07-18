/*
 * Copyright (C) 2021-2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WEB_RTC)

#include "WorkQueueMessageReceiver.h"
#include <WebCore/ProcessQualified.h>
#include <WebCore/RTCDataChannelRemoteHandler.h>
#include <WebCore/RTCDataChannelRemoteHandlerConnection.h>
#include <WebCore/RTCDataChannelRemoteSource.h>
#include <WebCore/RTCDataChannelRemoteSourceConnection.h>
#include <wtf/WorkQueue.h>

namespace WebKit {

class RTCDataChannelRemoteManager final : private IPC::MessageReceiver {
public:
    static RTCDataChannelRemoteManager& singleton();

    // Do nothing since this is a singleton.
    void ref() const { }
    void deref() const { }

    WebCore::RTCDataChannelRemoteHandlerConnection& remoteHandlerConnection();
    bool connectToRemoteSource(WebCore::RTCDataChannelIdentifier source, WebCore::RTCDataChannelIdentifier handler);

private:
    RTCDataChannelRemoteManager();
    void initialize();

    // IPC::MessageReceiver overrides.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    // Messages
    void sendData(WebCore::RTCDataChannelIdentifier, bool isRaw, std::span<const uint8_t>);
    void close(WebCore::RTCDataChannelIdentifier);

    // To handler
    void changeReadyState(WebCore::RTCDataChannelIdentifier, WebCore::RTCDataChannelState);
    void receiveData(WebCore::RTCDataChannelIdentifier, bool isRaw, std::span<const uint8_t>);
    void detectError(WebCore::RTCDataChannelIdentifier, WebCore::RTCErrorDetailType, String&&);
    void bufferedAmountIsDecreasing(WebCore::RTCDataChannelIdentifier, uint64_t);

    WebCore::RTCDataChannelRemoteSourceConnection& remoteSourceConnection();
    void postTaskToHandler(WebCore::RTCDataChannelIdentifier, Function<void(WebCore::RTCDataChannelRemoteHandler&)>&&);
    WebCore::RTCDataChannelRemoteSource* sourceFromIdentifier(WebCore::RTCDataChannelIdentifier);

    class RemoteHandlerConnection : public WebCore::RTCDataChannelRemoteHandlerConnection {
    public:
        static Ref<RemoteHandlerConnection> create(Ref<WorkQueue>&&);

        void connectToSource(WebCore::RTCDataChannelRemoteHandler&, std::optional<WebCore::ScriptExecutionContextIdentifier>, WebCore::RTCDataChannelIdentifier, WebCore::RTCDataChannelIdentifier) final;
        void sendData(WebCore::RTCDataChannelIdentifier, bool isRaw, std::span<const uint8_t>) final;
        void close(WebCore::RTCDataChannelIdentifier) final;

    private:
        explicit RemoteHandlerConnection(Ref<WorkQueue>&&);

        const Ref<IPC::Connection> m_connection;
        const Ref<WorkQueue> m_queue;
    };

    class RemoteSourceConnection : public WebCore::RTCDataChannelRemoteSourceConnection {
    public:
        static Ref<RemoteSourceConnection> create();

    private:
        RemoteSourceConnection();

        void didChangeReadyState(WebCore::RTCDataChannelIdentifier, WebCore::RTCDataChannelState) final;
        void didReceiveStringData(WebCore::RTCDataChannelIdentifier, const String&) final;
        void didReceiveRawData(WebCore::RTCDataChannelIdentifier, std::span<const uint8_t>) final;
        void didDetectError(WebCore::RTCDataChannelIdentifier, WebCore::RTCErrorDetailType, const String&) final;
        void bufferedAmountIsDecreasing(WebCore::RTCDataChannelIdentifier, uint64_t) final;

        const Ref<IPC::Connection> m_connection;
    };

    struct RemoteHandler {
        WeakPtr<WebCore::RTCDataChannelRemoteHandler> handler;
        Markable<WebCore::ScriptExecutionContextIdentifier> contextIdentifier;
    };

    const Ref<WorkQueue> m_queue;
    const Ref<IPC::Connection> m_connection;
    RefPtr<RemoteHandlerConnection> m_remoteHandlerConnection;
    RefPtr<RemoteSourceConnection> m_remoteSourceConnection;
    HashMap<WebCore::RTCDataChannelLocalIdentifier, UniqueRef<WebCore::RTCDataChannelRemoteSource>> m_sources;
    HashMap<WebCore::RTCDataChannelLocalIdentifier, RemoteHandler> m_handlers;
};

} // namespace WebKit

#endif // ENABLE(WEB_RTC)
