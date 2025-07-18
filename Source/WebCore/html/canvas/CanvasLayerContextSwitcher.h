/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "FloatRect.h"
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class CanvasRenderingContext2DBase;
class Filter;
class GraphicsContext;
class GraphicsContextSwitcher;

class CanvasLayerContextSwitcher : public RefCounted<CanvasLayerContextSwitcher> {
public:
    static RefPtr<CanvasLayerContextSwitcher> create(CanvasRenderingContext2DBase&, const FloatRect& bounds, RefPtr<Filter>&&);

    ~CanvasLayerContextSwitcher();

    GraphicsContext* drawingContext() const;
    FloatRect expandedBounds() const { return m_bounds + outsets(); }

private:
    CanvasLayerContextSwitcher(CanvasRenderingContext2DBase&, const FloatRect& bounds, std::unique_ptr<GraphicsContextSwitcher>&&);

    FloatBoxExtent outsets() const;
    Ref<CanvasRenderingContext2DBase> protectedContext() const { return m_context.get(); }

    WeakRef<CanvasRenderingContext2DBase> m_context;
    GraphicsContext* m_effectiveDrawingContext;
    FloatRect m_bounds;
    std::unique_ptr<GraphicsContextSwitcher> m_targetSwitcher;
};

} // namespace WebCore
