/*
 * Copyright (C) 2022-2025 Apple Inc. All rights reserved.
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
#include "StyleOriginatedAnimationEvent.h"

#include "Node.h"
#include "NodeInlines.h"
#include "WebAnimationUtilities.h"

#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(StyleOriginatedAnimationEvent);

StyleOriginatedAnimationEvent::StyleOriginatedAnimationEvent(enum EventInterfaceType eventInterface, const AtomString& type, WebAnimation* animation, std::optional<Seconds> scheduledTime, double elapsedTime, const std::optional<Style::PseudoElementIdentifier>& pseudoElementIdentifier)
    : AnimationEventBase(eventInterface, type, animation, scheduledTime)
    , m_elapsedTime(elapsedTime)
    , m_pseudoElementIdentifier(pseudoElementIdentifier)
{
}

StyleOriginatedAnimationEvent::StyleOriginatedAnimationEvent(enum EventInterfaceType eventInterface, const AtomString& type, const EventInit& init, IsTrusted isTrusted, double elapsedTime, const String& pseudoElement)
    : AnimationEventBase(eventInterface, type, init, isTrusted)
    , m_elapsedTime(elapsedTime)
    , m_pseudoElement(pseudoElement)
{
    RefPtr node = dynamicDowncast<Node>(target());
    auto [parsed, pseudoElementIdentifier] = pseudoElementIdentifierFromString(m_pseudoElement, node ? &node->document() : nullptr);
    m_pseudoElementIdentifier = parsed ? pseudoElementIdentifier : std::nullopt;
}

StyleOriginatedAnimationEvent::~StyleOriginatedAnimationEvent() = default;

const String& StyleOriginatedAnimationEvent::pseudoElement()
{
    if (m_pseudoElementIdentifier && m_pseudoElement.isNull())
        m_pseudoElement = pseudoElementIdentifierAsString(m_pseudoElementIdentifier);
    return m_pseudoElement;
}

} // namespace WebCore
