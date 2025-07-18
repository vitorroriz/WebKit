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

#include "ActiveDOMObject.h"
#include "Event.h"
#include "EventTarget.h"
#include "EventTargetInterfaces.h"
#include "RTCDtlsTransportBackend.h"
#include <JavaScriptCore/ArrayBuffer.h>

namespace WebCore {

class PeerConnectionBackend;
class RTCIceTransport;
class RTCPeerConnection;
class ScriptExecutionContext;

class RTCDtlsTransport final : public RefCounted<RTCDtlsTransport>, public ActiveDOMObject, public EventTarget, public RTCDtlsTransportBackendClient {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(RTCDtlsTransport);
public:
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    static Ref<RTCDtlsTransport> create(ScriptExecutionContext&, UniqueRef<RTCDtlsTransportBackend>&&, Ref<RTCIceTransport>&&);
    ~RTCDtlsTransport();

    RTCIceTransport& iceTransport() { return m_iceTransport.get(); }
    RTCDtlsTransportState state() { return m_state; }
    Vector<Ref<JSC::ArrayBuffer>> getRemoteCertificates();

    const RTCDtlsTransportBackend& backend() const { return m_backend.get(); }

    void close() { stop(); }

private:
    RTCDtlsTransport(ScriptExecutionContext&, UniqueRef<RTCDtlsTransportBackend>&&, Ref<RTCIceTransport>&&);

    // EventTarget
    enum EventTargetInterfaceType eventTargetInterface() const final { return EventTargetInterfaceType::RTCDtlsTransport; }
    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    // ActiveDOMObject
    void stop() final;
    bool virtualHasPendingActivity() const final;

    // RTCDtlsTransportBackendClient
    void onStateChanged(RTCDtlsTransportState, Vector<Ref<JSC::ArrayBuffer>>&&) final;
    void onError() final;

    const UniqueRef<RTCDtlsTransportBackend> m_backend;
    const Ref<RTCIceTransport> m_iceTransport;
    RTCDtlsTransportState m_state { RTCDtlsTransportState::New };
    Vector<Ref<JSC::ArrayBuffer>> m_remoteCertificates;
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
