/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2009 Google, Inc. All rights reserved.
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 */

#pragma once

#include "LegacyRenderSVGModelObject.h"

namespace WebCore {

class SVGElement;

class LegacyRenderSVGContainer : public LegacyRenderSVGModelObject {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(LegacyRenderSVGContainer);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(LegacyRenderSVGContainer);
public:
    virtual ~LegacyRenderSVGContainer();

    void paint(PaintInfo&, const LayoutPoint&) override;
    void setNeedsBoundariesUpdate() final { m_needsBoundariesUpdate = true; }
    virtual bool didTransformToRootUpdate() { return false; }
    bool isObjectBoundingBoxValid() const { return static_cast<bool>(m_objectBoundingBox); }
    bool isRepaintSuspendedForChildren() const { return m_repaintIsSuspendedForChildrenDuringLayout; }

protected:
    LegacyRenderSVGContainer(Type, SVGElement&, RenderStyle&&, OptionSet<SVGModelObjectFlag> = { });

    ASCIILiteral renderName() const override { return "RenderSVGContainer"_s; }

    bool canHaveChildren() const final { return true; }

    void layout() override;

    void addFocusRingRects(Vector<LayoutRect>&, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer = 0) const final;

    FloatRect objectBoundingBox() const final { return m_objectBoundingBox.value_or(FloatRect()); }
    FloatRect strokeBoundingBox() const final;
    FloatRect repaintRectInLocalCoordinates(RepaintRectCalculation = RepaintRectCalculation::Fast) const final;

    bool nodeAtFloatPoint(const HitTestRequest&, HitTestResult&, const FloatPoint& pointInParent, HitTestAction) override;

    // Allow LegacyRenderSVGTransformableContainer to hook in at the right time in layout()
    virtual bool calculateLocalTransform() { return false; }

    // Allow RenderSVGViewportContainer to hook in at the right times in layout(), paint() and nodeAtFloatPoint()
    virtual void calcViewport() { }
    virtual void applyViewportClip(PaintInfo&) { }
    virtual bool pointIsInsideViewportClip(const FloatPoint& /*pointInParent*/) { return true; }

    virtual void determineIfLayoutSizeChanged() { }

    bool selfWillPaint();
    void updateCachedBoundaries();

private:
    Markable<FloatRect> m_objectBoundingBox;
    mutable Markable<FloatRect> m_strokeBoundingBox;
    FloatRect m_repaintBoundingBox;
    mutable Markable<FloatRect> m_accurateRepaintBoundingBox;

    bool m_needsBoundariesUpdate { true };
    bool m_repaintIsSuspendedForChildrenDuringLayout { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(LegacyRenderSVGContainer, isLegacyRenderSVGContainer())
