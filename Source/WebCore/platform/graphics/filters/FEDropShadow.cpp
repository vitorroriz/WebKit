/*
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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

#include "config.h"
#include "FEDropShadow.h"

#include "ColorSerialization.h"
#include "FEDropShadowSoftwareApplier.h"
#include "FEGaussianBlur.h"
#include "Filter.h"
#include "GraphicsContext.h"
#include <wtf/text/TextStream.h>

#if USE(SKIA)
#include "FEDropShadowSkiaApplier.h"
#endif

namespace WebCore {

Ref<FEDropShadow> FEDropShadow::create(float stdX, float stdY, float dx, float dy, const Color& shadowColor, float shadowOpacity, DestinationColorSpace colorSpace)
{
    return adoptRef(*new FEDropShadow(stdX, stdY, dx, dy, shadowColor, shadowOpacity, colorSpace));
}

FEDropShadow::FEDropShadow(float stdX, float stdY, float dx, float dy, const Color& shadowColor, float shadowOpacity, DestinationColorSpace colorSpace)
    : FilterEffect(FilterEffect::Type::FEDropShadow, colorSpace)
    , m_stdX(stdX)
    , m_stdY(stdY)
    , m_dx(dx)
    , m_dy(dy)
    , m_shadowColor(shadowColor)
    , m_shadowOpacity(shadowOpacity)
{
}

bool FEDropShadow::operator==(const FEDropShadow& other) const
{
    return FilterEffect::operator==(other)
        && m_stdX == other.m_stdX
        && m_stdY == other.m_stdY
        && m_dx == other.m_dx
        && m_dy == other.m_dy
        && m_shadowColor == other.m_shadowColor
        && m_shadowOpacity == other.m_shadowOpacity;
}

bool FEDropShadow::setStdDeviationX(float stdX)
{
    if (m_stdX == stdX)
        return false;
    m_stdX = stdX;
    return true;
}

bool FEDropShadow::setStdDeviationY(float stdY)
{
    if (m_stdY == stdY)
        return false;
    m_stdY = stdY;
    return true;
}

bool FEDropShadow::setDx(float dx)
{
    if (m_dx == dx)
        return false;
    m_dx = dx;
    return true;
}

bool FEDropShadow::setDy(float dy)
{
    if (m_dy == dy)
        return false;
    m_dy = dy;
    return true;
}

bool FEDropShadow::setShadowColor(const Color& shadowColor)
{
    if (m_shadowColor == shadowColor)
        return false;
    m_shadowColor = shadowColor;
    return true;
}

bool FEDropShadow::setShadowOpacity(float shadowOpacity)
{
    if (m_shadowOpacity == shadowOpacity)
        return false;
    m_shadowOpacity = shadowOpacity;
    return true;
}

FloatRect FEDropShadow::calculateImageRect(const Filter& filter, std::span<const FloatRect> inputImageRects, const FloatRect& primitiveSubregion) const
{
    auto imageRect = inputImageRects[0];
    auto imageRectWithOffset(imageRect);
    imageRectWithOffset.move(filter.resolvedSize({ m_dx, m_dy }));
    imageRect.unite(imageRectWithOffset);

    auto kernelSize = FEGaussianBlur::calculateUnscaledKernelSize(filter.resolvedSize({ m_stdX, m_stdY }));

    // We take the half kernel size and multiply it with three, because we run box blur three times.
    imageRect.inflateX(3 * kernelSize.width() * 0.5f);
    imageRect.inflateY(3 * kernelSize.height() * 0.5f);

    return filter.clipToMaxEffectRect(imageRect, primitiveSubregion);
}

IntOutsets FEDropShadow::calculateOutsets(const FloatSize& offset, const FloatSize& stdDeviation)
{
    IntSize outsetSize = FEGaussianBlur::calculateOutsetSize(stdDeviation);

    int top = std::max<int>(0, outsetSize.height() - offset.height());
    int right = std::max<int>(0, outsetSize.width() + offset.width());
    int bottom = std::max<int>(0, outsetSize.height() + offset.height());
    int left = std::max<int>(0, outsetSize.width() - offset.width());

    return { top, right, bottom, left };
}

OptionSet<FilterRenderingMode> FEDropShadow::supportedFilterRenderingModes() const
{
    OptionSet<FilterRenderingMode> modes = FilterRenderingMode::Software;
#if USE(SKIA)
    modes.add(FilterRenderingMode::Accelerated);
#endif
#if USE(CG)
    if (m_stdX == m_stdY)
        modes.add(FilterRenderingMode::GraphicsContext);
#endif
    return modes;
}

std::optional<GraphicsStyle> FEDropShadow::createGraphicsStyle(GraphicsContext& context, const Filter& filter) const
{
    ASSERT(m_stdX == m_stdY);

    auto offset = filter.resolvedSize(context.platformShadowOffset({ m_dx, m_dy }));
    auto radius = FEGaussianBlur::calculateUnscaledKernelSize(filter.resolvedSize({ m_stdX, m_stdY }));

    return GraphicsDropShadow { offset, static_cast<float>(radius.width()), m_shadowColor, ShadowRadiusMode::Default, m_shadowOpacity };
}

std::unique_ptr<FilterEffectApplier> FEDropShadow::createAcceleratedApplier() const
{
#if USE(SKIA)
    return FilterEffectApplier::create<FEDropShadowSkiaApplier>(*this);
#else
    return nullptr;
#endif
}

std::unique_ptr<FilterEffectApplier> FEDropShadow::createSoftwareApplier() const
{
#if USE(SKIA)
    return FilterEffectApplier::create<FEDropShadowSkiaApplier>(*this);
#else
    return FilterEffectApplier::create<FEDropShadowSoftwareApplier>(*this);
#endif
}

TextStream& FEDropShadow::externalRepresentation(TextStream& ts, FilterRepresentation representation) const
{
    ts << indent << "[feDropShadow"_s;
    FilterEffect::externalRepresentation(ts, representation);

    ts << " stdDeviation=\""_s << m_stdX << ", "_s << m_stdY << '"';
    ts << " dx=\""_s << m_dx << "\" dy=\"" << m_dy << '"';
    ts << " flood-color=\""_s << serializationForRenderTreeAsText(m_shadowColor) << '"';
    ts << " flood-opacity=\""_s << m_shadowOpacity << '"';

    ts << "]\n"_s;
    return ts;
}
    
} // namespace WebCore
