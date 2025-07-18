/*
 * Copyright (C) 2008-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Torch Mobile, Inc.
 * Copyright 2010, The Android Open Source Project
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

#include "config.h"
#include "GeoNotifier.h"

#if ENABLE(GEOLOCATION)

#include "DocumentInlines.h"
#include "Geolocation.h"

namespace WebCore {

GeoNotifier::GeoNotifier(Geolocation& geolocation, Ref<PositionCallback>&& successCallback, RefPtr<PositionErrorCallback>&& errorCallback, PositionOptions&& options)
    : m_geolocation(geolocation)
    , m_successCallback(WTFMove(successCallback))
    , m_errorCallback(WTFMove(errorCallback))
    , m_options(WTFMove(options))
    , m_timer(*this, &GeoNotifier::timerFired)
    , m_useCachedPosition(false)
{
}

void GeoNotifier::setFatalError(RefPtr<GeolocationPositionError>&& error)
{
    // If a fatal error has already been set, stick with it. This makes sure that
    // when permission is denied, this is the error reported, as required by the
    // spec.
    if (m_fatalError)
        return;

    m_fatalError = WTFMove(error);
    // An existing timer may not have a zero timeout.
    m_timer.stop();
    m_timer.startOneShot(0_s);
}

void GeoNotifier::setUseCachedPosition()
{
    m_useCachedPosition = true;
    m_timer.startOneShot(0_s);
}

bool GeoNotifier::hasZeroTimeout() const
{
    return !m_options.timeout;
}

void GeoNotifier::runSuccessCallback(GeolocationPosition* position)
{
    // If we are here and the Geolocation permission is not approved, something has
    // gone horribly wrong.
    if (!m_geolocation->isAllowed())
        CRASH();

    m_successCallback->invoke(position);
}

void GeoNotifier::runErrorCallback(GeolocationPositionError& error)
{
    if (m_errorCallback)
        m_errorCallback->invoke(error);
}

void GeoNotifier::startTimerIfNeeded()
{
    m_timer.startOneShot(1_ms * m_options.timeout);
}

void GeoNotifier::stopTimer()
{
    m_timer.stop();
}

void GeoNotifier::timerFired()
{
    m_timer.stop();

    // Test for fatal error first. This is required for the case where the Frame is
    // disconnected and requests are cancelled.
    Ref geolocation = m_geolocation;
    if (RefPtr fatalError = m_fatalError) {
        runErrorCallback(*fatalError);
        // This will cause this notifier to be deleted.
        geolocation->fatalErrorOccurred(this);
        return;
    }

    if (m_useCachedPosition) {
        // Clear the cached position flag in case this is a watch request, which
        // will continue to run.
        m_useCachedPosition = false;
        geolocation->requestUsesCachedPosition(this);
        return;
    }
    
    if (m_errorCallback) {
        auto error = GeolocationPositionError::create(GeolocationPositionError::TIMEOUT, "Timeout expired"_s);
        m_errorCallback->invoke(error);
    }
    geolocation->requestTimedOut(this);
}

} // namespace WebCore

#endif // ENABLE(GEOLOCATION)
