/*
 * Copyright (C) 2023 Igalia S.L.
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

#pragma once

#include <wtf/Function.h>
#include <wtf/TZoneMalloc.h>

namespace WebKit {

using PlatformDisplayID = uint32_t;

class DisplayVBlankMonitor {
    WTF_MAKE_TZONE_ALLOCATED(DisplayVBlankMonitor);
public:
    static std::unique_ptr<DisplayVBlankMonitor> create(PlatformDisplayID);
    virtual ~DisplayVBlankMonitor();

    enum class Type {
        Drm,
        Timer,
#if ENABLE(WPE_PLATFORM)
        Wpe,
#endif
    };
    virtual Type type() const = 0;

    unsigned refreshRate() const { return m_refreshRate; }

    virtual void start() = 0;
    virtual void stop() = 0;
    virtual bool isActive() = 0;
    virtual void invalidate() = 0;

    virtual void setHandler(Function<void()>&&);

protected:
    explicit DisplayVBlankMonitor(unsigned);

    unsigned m_refreshRate;
    Function<void()> m_handler;
};

} // namespace WebKit
