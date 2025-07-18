/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "config.h"
#include "LibWebRTCCertificateGenerator.h"

#if ENABLE(WEB_RTC) && USE(LIBWEBRTC)

#include "ExceptionCode.h"
#include "ExceptionOr.h"
#include "LibWebRTCMacros.h"
#include "LibWebRTCProvider.h"
#include "LibWebRTCUtils.h"
#include "RTCCertificate.h"

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN

#include <webrtc/rtc_base/ref_counted_object.h>
#include <webrtc/rtc_base/rtc_certificate_generator.h>
#include <webrtc/rtc_base/ssl_certificate.h>

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

namespace WebCore {

namespace LibWebRTCCertificateGenerator {

class RTCCertificateGeneratorCallbackWrapper : public ThreadSafeRefCounted<RTCCertificateGeneratorCallbackWrapper, WTF::DestructionThread::Main> {
public:
    static Ref<RTCCertificateGeneratorCallbackWrapper> create(Ref<SecurityOrigin>&& origin, Function<void(ExceptionOr<Ref<RTCCertificate>>&&)>&& resultCallback)
    {
        return adoptRef(*new RTCCertificateGeneratorCallbackWrapper(WTFMove(origin), WTFMove(resultCallback)));
    }

    void process(webrtc::scoped_refptr<webrtc::RTCCertificate> certificate)
    {
        callOnMainThread([origin = m_origin.releaseNonNull(), callback = WTFMove(m_resultCallback), certificate = WTFMove(certificate)]() mutable {
            if (!certificate) {
                callback(Exception { ExceptionCode::TypeError, "Unable to create a certificate"_s });
                return;
            }

            Vector<RTCCertificate::DtlsFingerprint> fingerprints;
            auto stats = certificate->GetSSLCertificate().GetStats();
            auto* info = stats.get();
            while (info) {
                StringView fingerprint { std::span { info->fingerprint } };
                fingerprints.append({ fromStdString(info->fingerprint_algorithm), fingerprint.convertToASCIILowercase() });
                info = info->issuer.get();
            };

            auto pem = certificate->ToPEM();
            callback(RTCCertificate::create(WTFMove(origin), certificate->Expires(), WTFMove(fingerprints), fromStdString(pem.certificate()), fromStdString(pem.private_key())));
        });
    }

private:
    RTCCertificateGeneratorCallbackWrapper(Ref<SecurityOrigin>&& origin, Function<void(ExceptionOr<Ref<RTCCertificate>>&&)>&& resultCallback)
        : m_origin(WTFMove(origin))
        , m_resultCallback(WTFMove(resultCallback))
    {
    }

    RefPtr<SecurityOrigin> m_origin;
    Function<void(ExceptionOr<Ref<RTCCertificate>>&&)> m_resultCallback;
};

static inline webrtc::KeyParams keyParamsFromCertificateType(const PeerConnectionBackend::CertificateInformation& info)
{
    switch (info.type) {
    case PeerConnectionBackend::CertificateInformation::Type::ECDSAP256:
        return webrtc::KeyParams::ECDSA();
    case PeerConnectionBackend::CertificateInformation::Type::RSASSAPKCS1v15:
        if (info.rsaParameters)
            return webrtc::KeyParams::RSA(info.rsaParameters->modulusLength, info.rsaParameters->publicExponent);
        return webrtc::KeyParams::RSA(2048, 65537);
    }

    RELEASE_ASSERT_NOT_REACHED();
}

void generateCertificate(Ref<SecurityOrigin>&& origin, LibWebRTCProvider& provider, const PeerConnectionBackend::CertificateInformation& info, Function<void(ExceptionOr<Ref<RTCCertificate>>&&)>&& resultCallback)
{
    auto callbackWrapper = RTCCertificateGeneratorCallbackWrapper::create(WTFMove(origin), WTFMove(resultCallback));

    std::optional<uint64_t> expiresMs;
    if (info.expires)
        expiresMs = static_cast<uint64_t>(*info.expires);

    provider.prepareCertificateGenerator([info, expiresMs, callbackWrapper = WTFMove(callbackWrapper)](auto& generator) mutable {
        generator.GenerateCertificateAsync(keyParamsFromCertificateType(info), expiresMs, [callbackWrapper = WTFMove(callbackWrapper)](webrtc::scoped_refptr<webrtc::RTCCertificate> certificate) mutable {
            callbackWrapper->process(WTFMove(certificate));
        });
    });
}

} // namespace LibWebRTCCertificateGenerator

} // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(LIBWEBRTC)
