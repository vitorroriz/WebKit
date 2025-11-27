/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include "RenderElement.h"

namespace WebCore {

class RenderInline;
class RenderListBox;
struct PaintInfo;

class OutlinePainter {
public:
    OutlinePainter(const RenderElement&, const PaintInfo&);

    void paintOutline(const LayoutRect&) const;
    void paintOutline(const LayoutPoint& paintOffset, const Vector<LayoutRect>& lineRects) const;

    void paintFocusRing(const Vector<LayoutRect>&) const;

    static void collectFocusRingRects(const RenderElement&, Vector<LayoutRect>&, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer);

private:
    static void collectFocusRingRectsForInline(const RenderInline&, Vector<LayoutRect>&, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer);
    static void collectFocusRingRectsForListBox(const RenderListBox&, Vector<LayoutRect>&, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer);
    static void collectFocusRingRectsForBlock(const RenderBlock&, Vector<LayoutRect>&, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer);
    static void collectFocusRingRectsForInlineChildren(const RenderBlockFlow&, Vector<LayoutRect>&, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer);
    static void collectFocusRingRectsForChildBox(const RenderBox&, Vector<LayoutRect>&, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer);

    float deviceScaleFactor() const;

    CheckedRef<const RenderElement> m_renderer;
    const PaintInfo& m_paintInfo;
};

}
