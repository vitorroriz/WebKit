/*
 * Copyright (C) 2021 Igalia S.L.
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

#include "CredentialBase.h"
#include <wtf/glib/GRefPtr.h>

typedef struct _GTlsCertificate GTlsCertificate;

namespace WebCore {

class Credential : public CredentialBase {
public:
    Credential()
        : CredentialBase()
    {
    }

    Credential(const String& user, const String& password, CredentialPersistence persistence)
        : CredentialBase(user, password, persistence)
    {
    }

    Credential(const Credential&, CredentialPersistence);

    WEBCORE_EXPORT Credential(GTlsCertificate*, CredentialPersistence);

    WEBCORE_EXPORT bool isEmpty() const;

    static bool platformCompare(const Credential&, const Credential&);

    bool encodingRequiresPlatformData() const { return !!m_certificate; }

    GTlsCertificate* certificate() const { return m_certificate.get(); }

    struct PlatformData {
        GRefPtr<GTlsCertificate> certificate;
        CredentialPersistence persistence;
    };

    using IPCData = Variant<NonPlatformData, PlatformData>;
    WEBCORE_EXPORT static Credential fromIPCData(IPCData&&);
    WEBCORE_EXPORT IPCData ipcData() const;

private:
    GRefPtr<GTlsCertificate> m_certificate;
};

} // namespace WebCore
