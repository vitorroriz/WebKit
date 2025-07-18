/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "InspectorNetworkAgent.h"
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class InspectorBackendClient;
class Page;

class PageNetworkAgent final : public InspectorNetworkAgent {
    WTF_MAKE_NONCOPYABLE(PageNetworkAgent);
    WTF_MAKE_TZONE_ALLOCATED(PageNetworkAgent);
public:
    PageNetworkAgent(PageAgentContext&, InspectorBackendClient*);
    ~PageNetworkAgent();

private:
    Inspector::Protocol::Network::LoaderId loaderIdentifier(DocumentLoader*);
    Inspector::Protocol::Network::FrameId frameIdentifier(DocumentLoader*);
    Vector<WebSocket*> activeWebSockets() WTF_REQUIRES_LOCK(WebSocket::allActiveWebSocketsLock());
    void setResourceCachingDisabledInternal(bool);
#if ENABLE(INSPECTOR_NETWORK_THROTTLING)
    bool setEmulatedConditionsInternal(std::optional<int>&& bytesPerSecondLimit);
#endif
    ScriptExecutionContext* scriptExecutionContext(Inspector::Protocol::ErrorString&, const Inspector::Protocol::Network::FrameId&);
    void addConsoleMessage(std::unique_ptr<Inspector::ConsoleMessage>&&);
    bool shouldForceBufferingNetworkResourceData() const { return false; }

    WeakRef<Page> m_inspectedPage;
#if ENABLE(INSPECTOR_NETWORK_THROTTLING)
    InspectorBackendClient* m_client { nullptr };
#endif
};

} // namespace WebCore
