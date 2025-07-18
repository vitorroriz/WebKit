/*
 * Copyright (C) 2011-2025 Apple Inc. All rights reserved.
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

#include "MessageReceiver.h"
#include "WebGeolocationPosition.h"
#include "WebProcessSupplement.h"
#include <WebCore/RegistrableDomain.h>
#include <wtf/CheckedRef.h>
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakHashMap.h>
#include <wtf/WeakHashSet.h>

namespace WebCore {
class Geolocation;
class GeolocationPositionData;
}

namespace WebKit {

class WebProcess;
class WebPage;

class WebGeolocationManager : public WebProcessSupplement, public IPC::MessageReceiver {
    WTF_MAKE_TZONE_ALLOCATED(WebGeolocationManager);
    WTF_MAKE_NONCOPYABLE(WebGeolocationManager);
public:
    explicit WebGeolocationManager(WebProcess&);
    ~WebGeolocationManager();

    void ref() const final;
    void deref() const final;

    static ASCIILiteral supplementName();

    void registerWebPage(WebPage&, const String& authorizationToken, bool needsHighAccuracy);
    void unregisterWebPage(WebPage&);
    void setEnableHighAccuracyForPage(WebPage&, bool);

private:
    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    void didChangePosition(const WebCore::RegistrableDomain&, const WebCore::GeolocationPositionData&);
    void didFailToDeterminePosition(const WebCore::RegistrableDomain&, const String& errorMessage);
#if PLATFORM(IOS_FAMILY)
    void resetPermissions(const WebCore::RegistrableDomain&);
#endif // PLATFORM(IOS_FAMILY)

    struct PageSets {
        WeakHashSet<WebPage> pageSet;
        WeakHashSet<WebPage> highAccuracyPageSet;
    };
    bool isUpdating(const PageSets&) const;
    bool isHighAccuracyEnabled(const PageSets&) const;

    const CheckedRef<WebProcess> m_process;
    HashMap<WebCore::RegistrableDomain, PageSets> m_pageSets;
    WeakHashMap<WebPage, WebCore::RegistrableDomain> m_pageToRegistrableDomain;
};

} // namespace WebKit
