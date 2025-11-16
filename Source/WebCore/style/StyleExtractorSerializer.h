/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSDynamicRangeLimitMix.h"
#include "CSSMarkup.h"
#include "CSSPrimitiveNumericTypes+Serialization.h"
#include "CSSPrimitiveNumericTypes.h"
#include "CSSValueTypes.h"
#include "ColorSerialization.h"
#include "StyleExtractorConverter.h"
#include "StylePrimitiveKeyword+Serialization.h"
#include "StylePrimitiveNumericTypes+Serialization.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {
namespace Style {

class ExtractorSerializer {
public:
    // MARK: Strong value conversions

    template<typename T, typename... Rest> static void serializeStyleType(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const T&, Rest&&...);

    // MARK: Primitive serializations

    template<typename ConvertibleType>
    static void serialize(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const ConvertibleType&);
    static void serialize(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, double);
    static void serialize(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, float);
    static void serialize(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, unsigned);
    static void serialize(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, int);
    static void serialize(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, unsigned short);
    static void serialize(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, short);
    static void serialize(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const ScopedName&);

    template<typename T> static void serializeNumber(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, T);
    template<typename T> static void serializeNumberAsPixels(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, T);

    template<CSSValueID> static void serializeCustomIdentAtomOrKeyword(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const AtomString&);

    // MARK: Transform serializations

    static void serializeTransformationMatrix(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const TransformationMatrix&);
    static void serializeTransformationMatrix(const RenderStyle&, StringBuilder&, const CSS::SerializationContext&, const TransformationMatrix&);

    // MARK: Shared serializations

    static void serializeSmoothScrolling(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, bool);
    static void serializePositionTryFallbacks(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const FixedVector<PositionTryFallback>&);
    static void serializeTabSize(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const TabSize&);
    static void serializeTextUnderlinePosition(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, OptionSet<TextUnderlinePosition>);
    static void serializeTextEmphasisPosition(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, OptionSet<TextEmphasisPosition>);
    static void serializeSpeakAs(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, OptionSet<SpeakAs>);
    static void serializeHangingPunctuation(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, OptionSet<HangingPunctuation>);
    static void serializePositionAnchor(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const std::optional<ScopedName>&);
    static void serializePositionArea(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const std::optional<PositionArea>&);
    static void serializeNameScope(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const NameScope&);

    // MARK: MaskLayer property serializations

    static void serializeSingleMaskComposite(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, CompositeOperator);
    static void serializeSingleWebkitMaskComposite(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, CompositeOperator);
    static void serializeSingleMaskMode(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, MaskMode);
    static void serializeSingleWebkitMaskSourceType(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, MaskMode);
};

// MARK: - Strong value serializations

template<typename T, typename... Rest> void ExtractorSerializer::serializeStyleType(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const T& value, Rest&&... rest)
{
    serializationForCSS(builder, context, state.style, value, std::forward<Rest>(rest)...);
}

// MARK: - Primitive serializations

template<typename ConvertibleType>
void ExtractorSerializer::serialize(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const ConvertibleType& value)
{
    serializationForCSS(builder, context, state.style, value);
}

inline void ExtractorSerializer::serialize(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext& context, double value)
{
    CSS::serializationForCSS(builder, context, CSS::NumberRaw<> { value });
}

inline void ExtractorSerializer::serialize(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext& context, float value)
{
    CSS::serializationForCSS(builder, context, CSS::NumberRaw<> { value });
}

inline void ExtractorSerializer::serialize(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext& context, unsigned value)
{
    CSS::serializationForCSS(builder, context, CSS::IntegerRaw<CSS::All, unsigned> { value });
}

inline void ExtractorSerializer::serialize(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext& context, int value)
{
    CSS::serializationForCSS(builder, context, CSS::IntegerRaw<CSS::All, int> { value });
}

inline void ExtractorSerializer::serialize(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext& context, unsigned short value)
{
    CSS::serializationForCSS(builder, context, CSS::IntegerRaw<CSS::All, unsigned short> { value });
}

inline void ExtractorSerializer::serialize(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext& context, short value)
{
    CSS::serializationForCSS(builder, context, CSS::IntegerRaw<CSS::All, short> { value });
}

inline void ExtractorSerializer::serialize(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const ScopedName& scopedName)
{
    if (scopedName.isIdentifier)
        serializationForCSS(builder, context, state.style, CustomIdentifier { scopedName.name });
    else
        serializationForCSS(builder, context, state.style, scopedName.name);
}

template<typename T> void ExtractorSerializer::serializeNumber(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, T number)
{
    serialize(state, builder, context, number);
}

template<typename T> void ExtractorSerializer::serializeNumberAsPixels(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, T number)
{
    CSS::serializationForCSS(builder, context, CSS::LengthRaw<> { CSS::LengthUnit::Px, adjustFloatForAbsoluteZoom(number, state.style) });
}

template<CSSValueID keyword> void ExtractorSerializer::serializeCustomIdentAtomOrKeyword(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const AtomString& string)
{
    if (string.isNull()) {
        serializationForCSS(builder, context, state.style, Constant<keyword> { });
        return;
    }

    serializationForCSS(builder, context, state.style, CustomIdentifier { string });
}

// MARK: - Transform serializations

inline void ExtractorSerializer::serializeTransformationMatrix(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const TransformationMatrix& transform)
{
    serializeTransformationMatrix(state.style, builder, context, transform);
}

inline void ExtractorSerializer::serializeTransformationMatrix(const RenderStyle& style, StringBuilder& builder, const CSS::SerializationContext& context, const TransformationMatrix& transform)
{
    auto zoom = style.usedZoom();
    if (transform.isAffine()) {
        std::array values { transform.a(), transform.b(), transform.c(), transform.d(), transform.e() / zoom, transform.f() / zoom };
        builder.append(nameLiteral(CSSValueMatrix), '(', interleave(values, [&](auto& builder, auto& value) {
            CSS::serializationForCSS(builder, context, CSS::NumberRaw<> { value });
        }, ", "_s), ')');
        return;
    }

    std::array values {
        transform.m11(), transform.m12(), transform.m13(), transform.m14() * zoom,
        transform.m21(), transform.m22(), transform.m23(), transform.m24() * zoom,
        transform.m31(), transform.m32(), transform.m33(), transform.m34() * zoom,
        transform.m41() / zoom, transform.m42() / zoom, transform.m43() / zoom, transform.m44()
    };
    builder.append(nameLiteral(CSSValueMatrix3d), '(', interleave(values, [&](auto& builder, auto& value) {
        CSS::serializationForCSS(builder, context, CSS::NumberRaw<> { value });
    }, ", "_s), ')');
}

// MARK: - Shared serializations

inline void ExtractorSerializer::serializePositionTryFallbacks(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const FixedVector<PositionTryFallback>& fallbacks)
{
    if (fallbacks.isEmpty()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }

    CSSValueListBuilder list;
    for (auto& fallback : fallbacks) {
        if (RefPtr positionAreaProperties = fallback.positionAreaProperties) {
            auto areaValue = positionAreaProperties->getPropertyCSSValue(CSSPropertyPositionArea);
            if (areaValue)
                list.append(*areaValue);
            continue;
        }

        CSSValueListBuilder singleFallbackList;
        if (fallback.positionTryRuleName)
            singleFallbackList.append(ExtractorConverter::convert(state, *fallback.positionTryRuleName));
        for (auto& tactic : fallback.tactics)
            singleFallbackList.append(ExtractorConverter::convert(state, tactic));
        list.append(CSSValueList::createSpaceSeparated(singleFallbackList));
    }

    builder.append(CSSValueList::createCommaSeparated(WTFMove(list))->cssText(context));
}

inline void ExtractorSerializer::serializeTextUnderlinePosition(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, OptionSet<TextUnderlinePosition> textUnderlinePosition)
{
    ASSERT(!((textUnderlinePosition & TextUnderlinePosition::FromFont) && (textUnderlinePosition & TextUnderlinePosition::Under)));
    ASSERT(!((textUnderlinePosition & TextUnderlinePosition::Left) && (textUnderlinePosition & TextUnderlinePosition::Right)));

    if (textUnderlinePosition.isEmpty()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Auto { });
        return;
    }

    bool isFromFont = textUnderlinePosition.contains(TextUnderlinePosition::FromFont);
    bool isUnder = textUnderlinePosition.contains(TextUnderlinePosition::Under);
    bool isLeft = textUnderlinePosition.contains(TextUnderlinePosition::Left);
    bool isRight = textUnderlinePosition.contains(TextUnderlinePosition::Right);

    auto metric = isUnder ? CSSValueUnder : CSSValueFromFont;
    auto side = isLeft ? CSSValueLeft : CSSValueRight;
    if (!isFromFont && !isUnder) {
        builder.append(nameLiteralForSerialization(side));
        return;
    }
    if (!isLeft && !isRight) {
        builder.append(nameLiteralForSerialization(metric));
        return;
    }

    builder.append(nameLiteralForSerialization(metric), ' ', nameLiteralForSerialization(side));
}

inline void ExtractorSerializer::serializeTextEmphasisPosition(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext&, OptionSet<TextEmphasisPosition> textEmphasisPosition)
{
    ASSERT(!((textEmphasisPosition & TextEmphasisPosition::Over) && (textEmphasisPosition & TextEmphasisPosition::Under)));
    ASSERT(!((textEmphasisPosition & TextEmphasisPosition::Left) && (textEmphasisPosition & TextEmphasisPosition::Right)));
    ASSERT((textEmphasisPosition & TextEmphasisPosition::Over) || (textEmphasisPosition & TextEmphasisPosition::Under));

    bool listEmpty = true;
    auto appendOption = [&](TextEmphasisPosition test, CSSValueID value) {
        if (textEmphasisPosition &  test) {
            if (!listEmpty)
                builder.append(' ');
            builder.append(nameLiteralForSerialization(value));
            listEmpty = false;
        }
    };
    appendOption(TextEmphasisPosition::Over, CSSValueOver);
    appendOption(TextEmphasisPosition::Under, CSSValueUnder);
    appendOption(TextEmphasisPosition::Left, CSSValueLeft);
}

inline void ExtractorSerializer::serializeSpeakAs(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, OptionSet<SpeakAs> speakAs)
{
    bool listEmpty = true;
    auto appendOption = [&](SpeakAs test, CSSValueID value) {
        if (speakAs &  test) {
            if (!listEmpty)
                builder.append(' ');
            builder.append(nameLiteralForSerialization(value));
            listEmpty = false;
        }
    };
    appendOption(SpeakAs::SpellOut, CSSValueSpellOut);
    appendOption(SpeakAs::Digits, CSSValueDigits);
    appendOption(SpeakAs::LiteralPunctuation, CSSValueLiteralPunctuation);
    appendOption(SpeakAs::NoPunctuation, CSSValueNoPunctuation);

    if (listEmpty)
        serializationForCSS(builder, context, state.style, CSS::Keyword::Normal { });
}

inline void ExtractorSerializer::serializeHangingPunctuation(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, OptionSet<HangingPunctuation> hangingPunctuation)
{
    bool listEmpty = true;
    auto appendOption = [&](HangingPunctuation test, CSSValueID value) {
        if (hangingPunctuation &  test) {
            if (!listEmpty)
                builder.append(' ');
            builder.append(nameLiteralForSerialization(value));
            listEmpty = false;
        }
    };
    appendOption(HangingPunctuation::First, CSSValueFirst);
    appendOption(HangingPunctuation::AllowEnd, CSSValueAllowEnd);
    appendOption(HangingPunctuation::ForceEnd, CSSValueForceEnd);
    appendOption(HangingPunctuation::Last, CSSValueLast);

    if (listEmpty)
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
}

inline void ExtractorSerializer::serializePositionAnchor(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const std::optional<ScopedName>& positionAnchor)
{
    if (!positionAnchor) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Auto { });
        return;
    }

    serialize(state, builder, context, *positionAnchor);
}

inline void ExtractorSerializer::serializePositionArea(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const std::optional<PositionArea>& positionArea)
{
    if (!positionArea) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }

    // FIXME: Do this more efficiently without creating and destroying a CSSValue object.
    builder.append(ExtractorConverter::convertPositionArea(state, *positionArea)->cssText(context));
}

inline void ExtractorSerializer::serializeNameScope(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const NameScope& scope)
{
    switch (scope.type) {
    case NameScope::Type::None:
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    case NameScope::Type::All:
        serializationForCSS(builder, context, state.style, CSS::Keyword::All { });
        return;
    case NameScope::Type::Ident:
        if (scope.names.isEmpty()) {
            serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
            return;
        }

        builder.append(interleave(scope.names, [&](auto& builder, auto& name) {
            serializationForCSS(builder, context, state.style, CustomIdentifier { name });
        }, ", "_s));
        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

// MARK: - MaskLayer property serializations

inline void ExtractorSerializer::serializeSingleMaskComposite(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext&, CompositeOperator composite)
{
    builder.append(nameLiteralForSerialization(toCSSValueID(composite, CSSPropertyMaskComposite)));
}

inline void ExtractorSerializer::serializeSingleWebkitMaskComposite(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext&, CompositeOperator composite)
{
    builder.append(nameLiteralForSerialization(toCSSValueID(composite, CSSPropertyWebkitMaskComposite)));
}

inline void ExtractorSerializer::serializeSingleMaskMode(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, MaskMode maskMode)
{
    switch (maskMode) {
    case MaskMode::Alpha:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Alpha { });
        return;
    case MaskMode::Luminance:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Luminance { });
        return;
    case MaskMode::MatchSource:
        serializationForCSS(builder, context, state.style, CSS::Keyword::MatchSource { });
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline void ExtractorSerializer::serializeSingleWebkitMaskSourceType(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, MaskMode maskMode)
{
    switch (maskMode) {
    case MaskMode::Alpha:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Alpha { });
        return;
    case MaskMode::Luminance:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Luminance { });
        return;
    case MaskMode::MatchSource:
        // MatchSource is only available in the mask-mode property.
        serializationForCSS(builder, context, state.style, CSS::Keyword::Alpha { });
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace Style
} // namespace WebCore
