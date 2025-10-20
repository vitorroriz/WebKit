/*
 * Copyright (C) 2025 Igalia S.L. All rights reserved.
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

#include "config.h"
#include "WebXRRay.h"

#include <wtf/TZoneMallocInlines.h>

#if ENABLE(WEBXR_HIT_TEST)

#include "DOMPointReadOnly.h"
#include "WebXRRigidTransform.h"
#include "XRRayDirectionInit.h"

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(WebXRRay);

ExceptionOr<Ref<WebXRRay>> WebXRRay::create(const DOMPointInit& origin, const XRRayDirectionInit& direction)
{
    auto exceptionOrTransform = WebXRRigidTransform::create(origin, DOMPointInit { direction.x, direction.y, direction.z, direction.w });
    if (exceptionOrTransform.hasException())
        return exceptionOrTransform.releaseException();
    return adoptRef(*new WebXRRay(exceptionOrTransform.releaseReturnValue()));
}

Ref<WebXRRay> WebXRRay::create(WebXRRigidTransform& transform)
{
    return adoptRef(*new WebXRRay(transform));
}

WebXRRay::WebXRRay(WebXRRigidTransform& transform)
    : m_transform(transform)
{
}

WebXRRay::~WebXRRay() = default;

const DOMPointReadOnly& WebXRRay::origin()
{
    return m_transform->position();
}

const DOMPointReadOnly& WebXRRay::direction()
{
    return m_transform->orientation();
}

const Float32Array& WebXRRay::matrix()
{
    return m_transform->matrix();
}

} // namespace WebCore

#endif // ENABLE(WEBXR_HIT_TEST)
