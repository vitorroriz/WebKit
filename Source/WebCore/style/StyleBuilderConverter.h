/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2014-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2023 ChangSeok Oh <changseok@webkit.org>
 * Copyright (C) 2024-2025 Samuel Weinig <sam@webkit.org>
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

#include "AnchorPositionEvaluator.h"
#include "CSSCalcSymbolTable.h"
#include "CSSCalcValue.h"
#include "CSSCounterStyleRegistry.h"
#include "CSSDynamicRangeLimitValue.h"
#include "CSSFontFeatureValue.h"
#include "CSSFontStyleWithAngleValue.h"
#include "CSSFontVariationValue.h"
#include "CSSFunctionValue.h"
#include "CSSImageSetValue.h"
#include "CSSImageValue.h"
#include "CSSOffsetRotateValue.h"
#include "CSSPathValue.h"
#include "CSSPositionValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSPropertyParserConsumer+Font.h"
#include "CSSSubgridValue.h"
#include "CSSURLValue.h"
#include "CSSValuePair.h"
#include "DocumentQuirks.h"
#include "DocumentView.h"
#include "FontSelectionValueInlines.h"
#include "FontSizeAdjust.h"
#include "FrameDestructionObserverInlines.h"
#include "LocalFrame.h"
#include "RenderStyleInlines.h"
#include "RotateTransformOperation.h"
#include "SVGElementTypeHelpers.h"
#include "SVGPathElement.h"
#include "ScaleTransformOperation.h"
#include "ScopedName.h"
#include "ScrollAxis.h"
#include "Settings.h"
#include "StyleBasicShape.h"
#include "StyleBuilderChecking.h"
#include "StyleCalculationValue.h"
#include "StyleClipPath.h"
#include "StyleColorScheme.h"
#include "StyleCornerShapeValue.h"
#include "StyleDynamicRangeLimit.h"
#include "StyleEasingFunction.h"
#include "StyleFlexBasis.h"
#include "StyleInset.h"
#include "StyleLengthWrapper+CSSValueConversion.h"
#include "StyleMargin.h"
#include "StyleMaximumSize.h"
#include "StyleMinimumSize.h"
#include "StyleOffsetAnchor.h"
#include "StyleOffsetDistance.h"
#include "StyleOffsetPath.h"
#include "StyleOffsetPosition.h"
#include "StyleOffsetRotate.h"
#include "StylePadding.h"
#include "StylePerspective.h"
#include "StylePreferredSize.h"
#include "StylePrimitiveKeyword+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "StyleRayFunction.h"
#include "StyleResolveForFont.h"
#include "StyleRotate.h"
#include "StyleSVGPaint.h"
#include "StyleScale.h"
#include "StyleScrollMargin.h"
#include "StyleScrollPadding.h"
#include "StyleTextEdge+CSSValueConversion.h"
#include "StyleTranslate.h"
#include "StyleURL.h"
#include "StyleValueTypes+CSSValueConversion.h"
#include "TextSpacing.h"
#include "ViewTimeline.h"
#include <ranges>
#include <wtf/text/MakeString.h>

namespace WebCore {
namespace Style {

// FIXME: Some of those functions assume the CSS parser only allows valid CSSValue types.
// This might not be true if we pass the CSSValue from js via CSS Typed OM.

class BuilderConverter {
public:
    template<typename T, typename... Rest> static T convertStyleType(BuilderState&, const CSSValue&, Rest&&...);

    static TextAlignMode convertTextAlign(BuilderState&, const CSSValue&);
    static TextAlignLast convertTextAlignLast(BuilderState&, const CSSValue&);
    static Resize convertResize(BuilderState&, const CSSValue&);

    static FixedVector<PositionTryFallback> convertPositionTryFallbacks(BuilderState&, const CSSValue&);

    static MaskMode convertSingleMaskMode(BuilderState&, const CSSValue&);
};

template<typename T, typename... Rest> inline T BuilderConverter::convertStyleType(BuilderState& builderState, const CSSValue& value, Rest&&... rest)
{
    return toStyleFromCSSValue<T>(builderState, value, std::forward<Rest>(rest)...);
}

inline TextAlignMode BuilderConverter::convertTextAlign(BuilderState& builderState, const CSSValue& value)
{
    auto* primitiveValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return { };
    ASSERT(primitiveValue->isValueID());

    const auto& parentStyle = builderState.parentStyle();

    // User agents are expected to have a rule in their user agent stylesheet that matches th elements that have a parent
    // node whose computed value for the 'text-align' property is its initial value, whose declaration block consists of
    // just a single declaration that sets the 'text-align' property to the value 'center'.
    // https://html.spec.whatwg.org/multipage/rendering.html#rendering
    if (primitiveValue->valueID() == CSSValueInternalThCenter) {
        if (parentStyle.textAlign() == RenderStyle::initialTextAlign())
            return TextAlignMode::Center;
        return parentStyle.textAlign();
    }

    if (primitiveValue->valueID() == CSSValueWebkitMatchParent || primitiveValue->valueID() == CSSValueMatchParent) {
        const auto* element = builderState.element();

        if (element && element == builderState.document().documentElement())
            return TextAlignMode::Start;
        if (parentStyle.textAlign() == TextAlignMode::Start)
            return parentStyle.writingMode().isBidiLTR() ? TextAlignMode::Left : TextAlignMode::Right;
        if (parentStyle.textAlign() == TextAlignMode::End)
            return parentStyle.writingMode().isBidiLTR() ? TextAlignMode::Right : TextAlignMode::Left;

        return parentStyle.textAlign();
    }

    return fromCSSValue<TextAlignMode>(value);
}

inline TextAlignLast BuilderConverter::convertTextAlignLast(BuilderState& builderState, const CSSValue& value)
{
    auto* primitiveValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return { };
    ASSERT(primitiveValue->isValueID());

    if (primitiveValue->valueID() != CSSValueMatchParent)
        return fromCSSValue<TextAlignLast>(value);

    auto& parentStyle = builderState.parentStyle();
    if (parentStyle.textAlignLast() == TextAlignLast::Start)
        return parentStyle.writingMode().isBidiLTR() ? TextAlignLast::Left : TextAlignLast::Right;
    if (parentStyle.textAlignLast() == TextAlignLast::End)
        return parentStyle.writingMode().isBidiLTR() ? TextAlignLast::Right : TextAlignLast::Left;
    return parentStyle.textAlignLast();
}

inline Resize BuilderConverter::convertResize(BuilderState& builderState, const CSSValue& value)
{
    auto* primitiveValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return { };

    auto resize = Resize::None;
    if (primitiveValue->valueID() == CSSValueInternalTextareaAuto)
        resize = builderState.document().settings().textAreasAreResizable() ? Resize::Both : Resize::None;
    else
        resize = fromCSSValue<Resize>(value);

    return resize;
}

inline float zoomWithTextZoomFactor(BuilderState& builderState)
{
    if (auto* frame = builderState.document().frame()) {
        float textZoomFactor = builderState.style().textZoom() != TextZoom::Reset ? frame->textZoomFactor() : 1.0f;
        auto usedZoom = evaluationTimeZoomEnabled(builderState) ? 1.0f : builderState.style().usedZoom();
        return usedZoom * textZoomFactor;
    }
    return builderState.cssToLengthConversionData().zoom();
}

inline FixedVector<PositionTryFallback> BuilderConverter::convertPositionTryFallbacks(BuilderState& builderState, const CSSValue& value)
{
    // FIXME: SaferCPP analysis reports that 'builderState' is an unsafe capture, even though this lambda does not escape.
    SUPPRESS_UNCOUNTED_LAMBDA_CAPTURE auto convertFallback = [&](const CSSValue& fallbackValue) -> std::optional<PositionTryFallback> {
        auto* valueList = dynamicDowncast<CSSValueList>(fallbackValue);
        if (!valueList) {
            // Turn the inlined position-area fallback into properties object that can be applied similarly to @position-try declarations.
            auto property = CSSProperty { CSSPropertyPositionArea, Ref { const_cast<CSSValue&>(fallbackValue) } };
            return PositionTryFallback {
                .positionAreaProperties = ImmutableStyleProperties::createDeduplicating(std::span { &property, 1 }, HTMLStandardMode)
            };
        }

        if (valueList->separator() != CSSValueList::SpaceSeparator)
            return { };

        auto fallback = PositionTryFallback { };

        for (auto& item : *valueList) {
            if (item.isCustomIdent())
                fallback.positionTryRuleName = ScopedName { AtomString { item.customIdent() }, builderState.styleScopeOrdinal() };
            else {
                auto tacticValue = fromCSSValueID<PositionTryFallback::Tactic>(item.valueID());
                if (fallback.tactics.contains(tacticValue)) {
                    ASSERT_NOT_REACHED();
                    return { };
                }

                fallback.tactics.append(tacticValue);
            }
        }
        return fallback;
    };

    if (value.valueID() == CSSValueNone)
        return { };

    if (auto fallback = convertFallback(value))
        return { *fallback };

    auto* list = dynamicDowncast<CSSValueList>(value);
    if (!list)
        return { };

    return FixedVector<PositionTryFallback>::map(*list, [&](auto& item) {
        auto fallback = convertFallback(item);
        return fallback ? *fallback : PositionTryFallback { };
    });
}

inline MaskMode BuilderConverter::convertSingleMaskMode(BuilderState& builderState, const CSSValue& value)
{
    switch (value.valueID()) {
    case CSSValueAlpha:
        return MaskMode::Alpha;
    case CSSValueLuminance:
        return MaskMode::Luminance;
    case CSSValueMatchSource:
        return MaskMode::MatchSource;
    case CSSValueAuto: // -webkit-mask-source-type
        return MaskMode::MatchSource;
    default:
        builderState.setCurrentPropertyInvalidAtComputedValueTime();
        return MaskMode::MatchSource;
    }
}

} // namespace Style
} // namespace WebCore
