/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/EnumTraits.h>

namespace WebCore {

enum class NodeListInvalidationType : uint8_t {
    DoNotInvalidateOnAttributeChanges,
    InvalidateOnClassAttrChange,
    InvalidateOnIdNameAttrChange,
    InvalidateOnNameAttrChange,
    InvalidateOnForTypeAttrChange,
    InvalidateForFormControls,
    InvalidateOnHRefAttrChange,
    InvalidateOnAnyAttrChange,
};
constexpr auto numNodeListInvalidationTypes = enumToUnderlyingType(NodeListInvalidationType::InvalidateOnAnyAttrChange) + 1;

enum class DocumentCompatibilityMode : uint8_t {
    NoQuirksMode = 1,
    QuirksMode = 1 << 1,
    LimitedQuirksMode = 1 << 2
};

enum class LayoutOptions : uint8_t {
    RunPostLayoutTasksSynchronously = 1 << 0,
    IgnorePendingStylesheets = 1 << 1,
    TreatContentVisibilityHiddenAsVisible = 1 << 2,
    TreatContentVisibilityAutoAsVisible = 1 << 3,
    TreatRevealedWhenFoundAsVisible = 1 << 4,
    UpdateCompositingLayers = 1 << 5,
    DoNotLayoutAncestorDocuments = 1 << 6,
    // Doesn't call RenderLayer::recursiveUpdateLayerPositionsAfterLayout if
    // possible. The caller should use a LocalFrameView::AutoPreventLayerAccess
    // for the scope that layout is expected to be flushed to stop any access to
    // the stale RenderLayers.
    CanDeferUpdateLayerPositions = 1 << 7
};

enum class CanNavigateState : uint8_t {
    Unchecked,
    Unable,
    Able
};

} // namespace WebCore
