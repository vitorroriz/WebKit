/*
 * Copyright (C) 2003-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Holger Hans Peter Freyther
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

#include "config.h"
#include "WidthIterator.h"

#include "ComposedCharacterClusterTextIterator.h"
#include "Font.h"
#include "FontCascade.h"
#include "FontCascadeInlines.h"
#include "GlyphBuffer.h"
#include "Latin1TextIterator.h"
#include "SurrogatePairAwareTextIterator.h"
#include "TextSpacing.h"
#include <algorithm>
#include <wtf/MathExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/CharacterProperties.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WidthIterator);

using namespace WTF::Unicode;

WidthIterator::WidthIterator(const FontCascade& fontCascade, const TextRun& run, SingleThreadWeakHashSet<const Font>* fallbackFonts, bool accountForGlyphBounds, bool forTextEmphasis)
    : m_fontCascade(fontCascade)
    , m_run(run)
    , m_fallbackFonts(fallbackFonts)
    , m_expansion(run.expansion())
    , m_direction(m_run->direction())
    , m_isAfterExpansion(run.expansionBehavior().left == ExpansionBehavior::Behavior::Forbid)
    , m_accountForGlyphBounds(accountForGlyphBounds)
    , m_enableKerning(fontCascade.enableKerning())
    , m_requiresShaping(fontCascade.requiresShaping())
    , m_forTextEmphasis(forTextEmphasis)
{
    // FIXME: Should we clamp m_expansion so it can never be negative?

    if (!m_expansion)
        m_expansionPerOpportunity = 0;
    else {
        unsigned expansionOpportunityCount = FontCascade::expansionOpportunityCount(m_run->text(), direction(), run.expansionBehavior()).first;

        if (!expansionOpportunityCount)
            m_expansionPerOpportunity = 0;
        else
            m_expansionPerOpportunity = m_expansion / expansionOpportunityCount;
    }
}

struct OriginalAdvancesForCharacterTreatedAsSpace {
    explicit OriginalAdvancesForCharacterTreatedAsSpace(GlyphBufferStringOffset stringOffset, bool isSpace, float advance)
        : stringOffset(stringOffset)
        , characterIsSpace(isSpace)
        , advance(advance)
    {
    }

    GlyphBufferStringOffset stringOffset { 0 };
    bool characterIsSpace { false };
    float advance { 0 };
};

inline auto WidthIterator::applyFontTransforms(GlyphBuffer& glyphBuffer, unsigned lastGlyphCount, const Font& font, CharactersTreatedAsSpace& charactersTreatedAsSpace) -> ApplyFontTransformsResult
{
    auto glyphBufferSize = glyphBuffer.size();
    ASSERT(lastGlyphCount <= glyphBufferSize);
    if (lastGlyphCount >= glyphBufferSize)
        return { 0, makeGlyphBufferAdvance() };

    auto advances = glyphBuffer.advances();
    float beforeWidth = 0;
    for (unsigned i = lastGlyphCount; i < glyphBufferSize; ++i)
        beforeWidth += width(advances[i]);

    auto initialAdvance = font.applyTransforms(glyphBuffer, lastGlyphCount, m_currentCharacterIndex, m_enableKerning, m_requiresShaping, m_fontCascade->fontDescription().computedLocale(), m_run->text(), direction());

    glyphBufferSize = glyphBuffer.size();
    advances = glyphBuffer.advances();

    auto origins = glyphBuffer.origins();
    for (unsigned i = lastGlyphCount; i < glyphBufferSize; ++i) {
        setHeight(advances[i], -height(advances[i]));
        setY(origins[i], -y(origins[i]));
    }

    for (unsigned i = lastGlyphCount; i < glyphBufferSize; ++i) {
        auto characterIndex = glyphBuffer.uncheckedStringOffsetAt(i);
        auto iterator = std::lower_bound(charactersTreatedAsSpace.begin(), charactersTreatedAsSpace.end(), characterIndex, [](const OriginalAdvancesForCharacterTreatedAsSpace& l, GlyphBufferStringOffset r) -> bool {
            return l.stringOffset < r;
        });
        if (iterator == charactersTreatedAsSpace.end() || iterator->stringOffset != characterIndex)
            continue;
        auto& originalAdvances = *iterator;
        setWidth(glyphBuffer.advanceAt(i), originalAdvances.advance);
    }
    charactersTreatedAsSpace.clear();

    float afterWidth = 0;
    for (unsigned i = lastGlyphCount; i < glyphBufferSize; ++i)
        afterWidth += width(advances[i]);

    return { afterWidth - beforeWidth, initialAdvance };
}

static inline std::pair<bool, bool> expansionLocation(bool ideograph, bool treatAsSpace, bool ltr, bool isAfterExpansion, bool forbidLeftExpansion, bool forbidRightExpansion, bool forceLeftExpansion, bool forceRightExpansion)
{
    bool expandLeft = ideograph;
    bool expandRight = ideograph;
    if (treatAsSpace) {
        if (ltr)
            expandRight = true;
        else
            expandLeft = true;
    }
    if (isAfterExpansion) {
        if (ltr)
            expandLeft = false;
        else
            expandRight = false;
    }
    ASSERT(!forbidLeftExpansion || !forceLeftExpansion);
    ASSERT(!forbidRightExpansion || !forceRightExpansion);
    if (forbidLeftExpansion)
        expandLeft = false;
    if (forbidRightExpansion)
        expandRight = false;
    if (forceLeftExpansion)
        expandLeft = true;
    if (forceRightExpansion)
        expandRight = true;
    return std::make_pair(expandLeft, expandRight);
}

static void expandWithInitialAdvance(GlyphBufferAdvance& advanceToExpand, const GlyphBufferAdvance& initialAdvance)
{
    setWidth(advanceToExpand, width(advanceToExpand) + width(initialAdvance));
    setHeight(advanceToExpand, height(advanceToExpand) + height(initialAdvance));
}

void WidthIterator::applyInitialAdvance(GlyphBuffer& glyphBuffer, GlyphBufferAdvance initialAdvance, unsigned lastGlyphCount)
{
    ASSERT(glyphBuffer.size() >= lastGlyphCount);

    if (glyphBuffer.size() <= lastGlyphCount)
        return;

    ASSERT(lastGlyphCount || (!width(m_leftoverInitialAdvance) && !height(m_leftoverInitialAdvance)));

    if (rtl() && lastGlyphCount) {
        auto& visuallyLastAdvance = glyphBuffer.advanceAt(lastGlyphCount);
        expandWithInitialAdvance(visuallyLastAdvance, m_leftoverInitialAdvance);
        m_runWidthSoFar += width(m_leftoverInitialAdvance);
        m_leftoverInitialAdvance = makeGlyphBufferAdvance();
    }

    if (rtl())
        m_leftoverInitialAdvance = initialAdvance;
    else {
        if (lastGlyphCount) {
            auto& visuallyPreviousAdvance = glyphBuffer.advanceAt(lastGlyphCount - 1);
            expandWithInitialAdvance(visuallyPreviousAdvance, initialAdvance);
            m_runWidthSoFar += width(initialAdvance);
        } else
            glyphBuffer.expandInitialAdvance(initialAdvance);
    }
}

bool WidthIterator::hasExtraSpacing() const
{
    return (m_fontCascade->letterSpacing() || m_fontCascade->wordSpacing() || m_expansion) && !m_run->spacingDisabled();
}

static void resetGlyphBuffer(GlyphBuffer& glyphBuffer, GlyphBufferStringOffset index)
{
#if USE(CG)
    ASSERT(index >= 0);
#endif
    auto unsignedIndex = static_cast<unsigned>(index);
    ASSERT(unsignedIndex < glyphBuffer.size());
    glyphBuffer.shrink(unsignedIndex);
}

static void addToGlyphBuffer(GlyphBuffer& glyphBuffer, Glyph glyph, const Font& font, float width, GlyphBufferStringOffset currentCharacterIndex, char32_t character)
{
    glyphBuffer.add(glyph, font, width, currentCharacterIndex);

    // These 0 glyphs are needed by shapers if the source text has surrogate pairs.
    // However, CTFontTransformGlyphs() can't delete these 0 glyphs from the shaped text,
    // so we shouldn't add them in the first place if we're using that shaping routine.
    // Any other shaping routine should delete these glyphs from the shaped text.
    if (!U_IS_BMP(character))
        glyphBuffer.add(0, font, 0, currentCharacterIndex + 1);
}

struct SmallCapsState {
    RefPtr<const Font> font;
    RefPtr<const Font> synthesizedFont;
    RefPtr<const Font> smallSynthesizedFont;
    bool isSmallCaps { false };
    bool isLastSmallCaps { false };
    bool shouldSynthesizeCharacter { false };

    const FontVariantCaps fontVariantCaps { FontVariantCaps::Normal };
    const bool dontSynthesizeSmallCaps { false };
    const bool engageAllSmallCapsProcessing { false };

    SmallCapsState(const FontCascadeDescription& fontDescription)
        : fontVariantCaps { fontDescription.variantCaps() }
        , dontSynthesizeSmallCaps { !fontDescription.hasAutoFontSynthesisSmallCaps() }
        , engageAllSmallCapsProcessing { fontVariantCaps == FontVariantCaps::AllSmall || fontVariantCaps == FontVariantCaps::AllPetite }
    {
    }

    void setSmallCapsData(const Font* font, const FontDescription& fontDescription)
    {
        ASSERT(font);
        this->font = font;
        synthesizedFont = font->noSynthesizableFeaturesFont();
        smallSynthesizedFont = Ref { *synthesizedFont }->smallCapsFont(fontDescription);
        isLastSmallCaps = isSmallCaps;
        isSmallCaps = true;
    }

    void clear()
    {
        synthesizedFont = nullptr;
        smallSynthesizedFont = nullptr;
        isSmallCaps = false;
    }

    void setIsSmallCaps(bool isSmallCaps)
    {
        isLastSmallCaps = this->isSmallCaps;
        this->isSmallCaps = isSmallCaps;
    }

    bool skipSmallCapsProcessing() const
    {
        return fontVariantCaps == FontVariantCaps::Normal;
    }
};

struct AdvanceInternalState {
    RefPtr<const Font> font;
    RefPtr<const Font> lastFont;
    // rangeFont and font are not necessarily the same, since small-caps might change the range fot for a synthesized font, or a small-caps-synthesized font.
    RefPtr<const Font> rangeFont;
    RefPtr<const Font> nextRangeFont;
    GlyphBuffer& glyphBuffer;
    unsigned lastGlyphCount { 0 };
    Ref<const Font> primaryFont;
    float widthOfCurrentFontRange { 0 };
    CharactersTreatedAsSpace charactersTreatedAsSpace;
    unsigned currentCharacterIndex { 0 };
    unsigned indexOfFontTransition { 0 };

    AdvanceInternalState(GlyphBuffer& glyphBuffer, const Font& primaryFont, unsigned currentCharacterIndex)
        : font { &primaryFont }
        , lastFont { &primaryFont }
        , rangeFont { &primaryFont }
        , nextRangeFont { &primaryFont }
        , glyphBuffer { glyphBuffer }
        , lastGlyphCount { glyphBuffer.size() }
        , primaryFont { primaryFont }
        , currentCharacterIndex { currentCharacterIndex }
        , indexOfFontTransition { currentCharacterIndex }
    {
    }

    bool fontChanged() const
    {
        return font != lastFont;
    }

    void updateFont(const Font* newFont)
    {
        lastFont = std::exchange(font, newFont);
    }
};

void WidthIterator::commitCurrentFontRange(AdvanceInternalState& advanceInternalState)
{
#if ASSERT_ENABLED
    ASSERT(advanceInternalState.rangeFont);
    for (unsigned i = advanceInternalState.lastGlyphCount; i < advanceInternalState.glyphBuffer.size(); ++i)
        ASSERT(&advanceInternalState.glyphBuffer.fontAt(i) == advanceInternalState.rangeFont);
#endif

    auto applyFontTransformsResult = applyFontTransforms(advanceInternalState.glyphBuffer, advanceInternalState.lastGlyphCount, Ref { *advanceInternalState.rangeFont }, advanceInternalState.charactersTreatedAsSpace);
    m_runWidthSoFar += applyFontTransformsResult.additionalAdvance;
    applyInitialAdvance(advanceInternalState.glyphBuffer, applyFontTransformsResult.initialAdvance, advanceInternalState.lastGlyphCount);
    m_currentCharacterIndex = advanceInternalState.currentCharacterIndex;

    if (advanceInternalState.widthOfCurrentFontRange && m_fallbackFonts && advanceInternalState.rangeFont != advanceInternalState.primaryFont.ptr())
        m_fallbackFonts->add(Ref { *advanceInternalState.rangeFont });

    advanceInternalState.widthOfCurrentFontRange = 0;
    advanceInternalState.lastGlyphCount = advanceInternalState.glyphBuffer.size();
}

static const Font* fontForRange(const Font* font, const SmallCapsState& smallCapsData, bool isSmallCaps)
{
    if (!smallCapsData.synthesizedFont)
        return font;
    return isSmallCaps ? smallCapsData.smallSynthesizedFont.get() : smallCapsData.synthesizedFont.get();
}

void WidthIterator::startNewFontRangeIfNeeded(AdvanceInternalState& advanceInternalState, SmallCapsState& smallCapsState, const FontCascadeDescription& fontDescription)
{
    // New range is needed if there is a font change or "small caps" status change.
    if (advanceInternalState.fontChanged() || smallCapsState.isSmallCaps != smallCapsState.isLastSmallCaps) {
        commitCurrentFontRange(advanceInternalState);

        if (!smallCapsState.skipSmallCapsProcessing()) {
            if (advanceInternalState.fontChanged())
                smallCapsState.clear();
            if (smallCapsState.shouldSynthesizeCharacter)
                smallCapsState.setSmallCapsData(advanceInternalState.font.get(), fontDescription);
        }

        advanceInternalState.nextRangeFont = fontForRange(advanceInternalState.font.get(), smallCapsState, smallCapsState.isSmallCaps);
        advanceInternalState.indexOfFontTransition = advanceInternalState.currentCharacterIndex;
    }
}

template <typename TextIterator>
static bool resetFontRangeIfNeeded(AdvanceInternalState& advanceInternalState, SmallCapsState& smallCapsState, const FontCascadeDescription& fontDescription, TextIterator& textIterator)
{
    if (smallCapsState.skipSmallCapsProcessing())
        return false;

    if (!smallCapsState.synthesizedFont && smallCapsState.shouldSynthesizeCharacter) {
        // Rather than synthesize each character individually, we should synthesize the entire "run" if any character requires synthesis.
        smallCapsState.setSmallCapsData(advanceInternalState.font.get(), fontDescription);
        textIterator.reset(advanceInternalState.indexOfFontTransition);
        resetGlyphBuffer(advanceInternalState.glyphBuffer, advanceInternalState.lastGlyphCount);
        return true;
    }
    return false;
}

static void updateCharacterAndSmallCapsIfNeeded(SmallCapsState& smallCapsState, std::optional<char32_t> capitalizedCharacter, char32_t& characterToWrite)
{
    if (smallCapsState.skipSmallCapsProcessing())
        return;

    if (smallCapsState.synthesizedFont || smallCapsState.shouldSynthesizeCharacter) {
        if (capitalizedCharacter) {
            smallCapsState.setIsSmallCaps(true);
            characterToWrite = capitalizedCharacter.value();
        } else
            smallCapsState.setIsSmallCaps(smallCapsState.engageAllSmallCapsProcessing);
    }
}

template <typename TextIterator>
inline void WidthIterator::advanceInternal(TextIterator& textIterator, GlyphBuffer& glyphBuffer)
{
    // The core logic here needs to match FontCascade::widthForTextUsingSimplifiedMeasuring()
    FloatRect bounds;
    auto& fontDescription = m_fontCascade->fontDescription();
    Ref primaryFont = m_fontCascade->primaryFont();
    AdvanceInternalState advanceInternalState(glyphBuffer, primaryFont, textIterator.currentIndex());
    SmallCapsState smallCapsState(fontDescription);

    char32_t character = 0;
    float width = 0;
    unsigned clusterLength = 0;
    // We are iterating in string order, not glyph order. Compare this to ComplexTextController::adjustGlyphsAndAdvances()
    if (!textIterator.consume(character, clusterLength))
        return;

    auto glyphData = m_fontCascade->glyphDataForCharacter(character, false, FontVariant::NormalVariant);

    RefPtr<Font> halfWidthFont;
    auto shouldProcessTextSpacingTrim = !fontDescription.textSpacingTrim().isSpaceAll();
    if (shouldProcessTextSpacingTrim) {
        TextSpacing::CharactersData charactersData = { .currentCharacter = character, .currentCharacterClass = TextSpacing::characterClass(character) };
        halfWidthFont = TextSpacing::getHalfWidthFontIfNeeded(*glyphData.protectedFont(), fontDescription.textSpacingTrim(), charactersData);
        glyphData.font = halfWidthFont ? halfWidthFont.get() : glyphData.font;
    }

    advanceInternalState.updateFont(glyphData.font ? glyphData.protectedFont().get() : primaryFont.ptr());
    auto capitalizedCharacter = capitalized(character);
    if (shouldSynthesizeSmallCaps(smallCapsState.dontSynthesizeSmallCaps, advanceInternalState.font.get(), character, capitalizedCharacter, smallCapsState.fontVariantCaps, smallCapsState.engageAllSmallCapsProcessing))
        smallCapsState.setSmallCapsData(advanceInternalState.font.get(), fontDescription);
    advanceInternalState.rangeFont = fontForRange(advanceInternalState.font.get(), smallCapsState, smallCapsState.isSmallCaps);
    advanceInternalState.nextRangeFont = advanceInternalState.rangeFont;

    while (textIterator.consume(character, clusterLength)) {
        // FIXME: Should we replace unpaired surrogates with the object replacement character?
        // Should we do this before or after shaping? What does a shaper do with an unpaired surrogate?
        m_containsTabs |= character == tabCharacter;
        advanceInternalState.currentCharacterIndex = textIterator.currentIndex();
        unsigned advanceLength = clusterLength;
        if (advanceInternalState.currentCharacterIndex + advanceLength == m_run->length())
            m_lastCharacterIndex = advanceInternalState.currentCharacterIndex;
        bool characterMustDrawSomething = !isDefaultIgnorableCodePoint(character);

        capitalizedCharacter = capitalized(character);
        char32_t characterToWrite = character;

#if USE(FREETYPE)
        // Freetype based ports only override the characters with Default_Ignorable unicode property when the font
        // doesn't support the code point. We should ignore them at this point to ensure they are not displayed.
        if (!characterMustDrawSomething) {
            textIterator.advance(advanceLength);
            continue;
        }
#endif
        auto glyphData = m_fontCascade->glyphDataForCharacter(character, false, FontVariant::NormalVariant);

        if (shouldProcessTextSpacingTrim) {
            TextSpacing::CharactersData charactersData = { .currentCharacter = character, .currentCharacterClass = TextSpacing::characterClass(character) };
            halfWidthFont = TextSpacing::getHalfWidthFontIfNeeded(*glyphData.protectedFont(), fontDescription.textSpacingTrim(), charactersData);
            glyphData.font = halfWidthFont ? halfWidthFont.get() : glyphData.font;
        }

        advanceInternalState.updateFont(glyphData.font ? glyphData.protectedFont().get() : primaryFont.ptr());
        smallCapsState.shouldSynthesizeCharacter = shouldSynthesizeSmallCaps(smallCapsState.dontSynthesizeSmallCaps, advanceInternalState.font.get(), character, capitalizedCharacter, smallCapsState.fontVariantCaps, smallCapsState.engageAllSmallCapsProcessing);
        updateCharacterAndSmallCapsIfNeeded(smallCapsState, capitalizedCharacter, characterToWrite);

        advanceInternalState.rangeFont = fontForRange(advanceInternalState.lastFont.get(), smallCapsState, smallCapsState.isLastSmallCaps);
        startNewFontRangeIfNeeded(advanceInternalState, smallCapsState, fontDescription);
        if (resetFontRangeIfNeeded(advanceInternalState, smallCapsState, fontDescription, textIterator))
            continue;

        if (rtl())
            characterToWrite = u_charMirror(characterToWrite);

        Glyph glyph = glyphData.glyph;
        if (glyphData.font.get() != advanceInternalState.nextRangeFont || character != characterToWrite)
            glyph = Ref { *advanceInternalState.nextRangeFont }->glyphForCharacter(characterToWrite);

        if (!glyph && !characterMustDrawSomething) {
            commitCurrentFontRange(advanceInternalState);

            addToGlyphBuffer(advanceInternalState.glyphBuffer, deletedGlyph, primaryFont, 0, advanceInternalState.currentCharacterIndex, characterToWrite);

            textIterator.advance(advanceLength);
            advanceInternalState.currentCharacterIndex = textIterator.currentIndex();
            advanceInternalState.updateFont(primaryFont.ptr());
            continue;
        }

        width = Ref { * advanceInternalState.nextRangeFont }->widthForGlyph(glyph, Font::SyntheticBoldInclusion::Exclude); // We apply synthetic bold after shaping, in applyCSSVisibilityRules().
        advanceInternalState.widthOfCurrentFontRange += width;

        if (FontCascade::treatAsSpace(characterToWrite))
            advanceInternalState.charactersTreatedAsSpace.constructAndAppend(advanceInternalState.currentCharacterIndex, characterToWrite == space, characterToWrite == tabCharacter ? width : advanceInternalState.nextRangeFont->spaceWidth(Font::SyntheticBoldInclusion::Exclude));

        if (m_accountForGlyphBounds) {
            bounds = Ref { *advanceInternalState.nextRangeFont }->boundsForGlyph(glyph);
            if (!advanceInternalState.currentCharacterIndex)
                m_firstGlyphOverflow = std::max<float>(0, -bounds.x());
        }

        if (m_forTextEmphasis && !FontCascade::canReceiveTextEmphasis(characterToWrite))
            glyph = deletedGlyph;

        addToGlyphBuffer(glyphBuffer, glyph,  Ref { *advanceInternalState.nextRangeFont }, width, advanceInternalState.currentCharacterIndex, characterToWrite);

        // Advance past the character we just dealt with.
        textIterator.advance(advanceLength);
        advanceInternalState.currentCharacterIndex = textIterator.currentIndex();

        m_runWidthSoFar += width;

        if (m_accountForGlyphBounds) {
            m_maxGlyphBoundingBoxY = std::max(m_maxGlyphBoundingBoxY, bounds.maxY());
            m_minGlyphBoundingBoxY = std::min(m_minGlyphBoundingBoxY, bounds.y());
            m_lastGlyphOverflow = std::max<float>(0, bounds.maxX() - width);
        }
    }
    advanceInternalState.rangeFont = advanceInternalState.nextRangeFont;
    commitCurrentFontRange(advanceInternalState);
}

auto WidthIterator::calculateAdditionalWidth(GlyphBuffer& glyphBuffer, GlyphBufferStringOffset currentCharacterIndex, unsigned leadingGlyphIndex, unsigned trailingGlyphIndex, float position) const -> AdditionalWidth
{
    float leftAdditionalWidth = 0;
    float rightAdditionalWidth = 0;
    float leftExpansionAdditionalWidth = 0;
    float rightExpansionAdditionalWidth = 0;

    auto character = m_run.get()[currentCharacterIndex];

    if (character == tabCharacter && m_run->allowTabs()) {
        Ref font = glyphBuffer.fontAt(trailingGlyphIndex);
        // Synthetic bold will be handled in applyCSSVisibilityRules() later.
        auto newWidth = m_fontCascade->tabWidth(font, m_run->tabSize(), position, Font::SyntheticBoldInclusion::Exclude);
        auto currentWidth = width(glyphBuffer.advanceAt(trailingGlyphIndex));
        rightAdditionalWidth += newWidth - currentWidth;
    }

    if (hasExtraSpacing()) {
        bool treatAsSpace = FontCascade::treatAsSpace(character);

        // This is a heuristic to determine if the character is non-visible. Non-visible characters don't get letter-spacing.
        float baseWidth = 0;
        for (unsigned i = leadingGlyphIndex; i <= trailingGlyphIndex; ++i)
            baseWidth += width(glyphBuffer.advanceAt(i));
        if (baseWidth)
            rightAdditionalWidth += m_fontCascade->letterSpacing();

        if (treatAsSpace && (character != tabCharacter || !m_run->allowTabs()) && (currentCharacterIndex || character == noBreakSpace) && m_fontCascade->wordSpacing())
            rightAdditionalWidth += m_fontCascade->wordSpacing();

        if (m_expansion > 0) {
            bool currentIsLastCharacter = m_lastCharacterIndex && currentCharacterIndex == static_cast<GlyphBufferStringOffset>(*m_lastCharacterIndex);

            bool isLeftmostCharacter = !currentCharacterIndex;
            bool isRightmostCharacter = currentIsLastCharacter;
            if (rtl())
                std::swap(isLeftmostCharacter, isRightmostCharacter);

            bool forceLeftExpansion = isLeftmostCharacter && m_run->expansionBehavior().left == ExpansionBehavior::Behavior::Force;
            bool forceRightExpansion = isRightmostCharacter && m_run->expansionBehavior().right == ExpansionBehavior::Behavior::Force;
            bool forbidLeftExpansion = isLeftmostCharacter && m_run->expansionBehavior().left == ExpansionBehavior::Behavior::Forbid;
            bool forbidRightExpansion = isRightmostCharacter && m_run->expansionBehavior().right == ExpansionBehavior::Behavior::Forbid;

            bool isIdeograph = FontCascade::canExpandAroundIdeographsInComplexText() && FontCascade::isCJKIdeographOrSymbol(character);

            if (treatAsSpace || isIdeograph || forceLeftExpansion || forceRightExpansion) {
                auto [expandLeft, expandRight] = expansionLocation(isIdeograph, treatAsSpace, ltr(), m_isAfterExpansion, forbidLeftExpansion, forbidRightExpansion, forceLeftExpansion, forceRightExpansion);

                if (expandLeft)
                    leftExpansionAdditionalWidth += m_expansionPerOpportunity;
                if (expandRight)
                    rightExpansionAdditionalWidth += m_expansionPerOpportunity;
            }
        }
    }

    return { leftAdditionalWidth, rightAdditionalWidth, leftExpansionAdditionalWidth, rightExpansionAdditionalWidth };
}

struct GlyphIndexRange {
    // This means the character got expanded to glyphs inside the GlyphBuffer at indices [leadingGlyphIndex, trailingGlyphIndex].
    unsigned leadingGlyphIndex;
    unsigned trailingGlyphIndex;
};

void WidthIterator::applyAdditionalWidth(GlyphBuffer& glyphBuffer, GlyphIndexRange glyphIndexRange, float leftAdditionalWidth, float rightAdditionalWidth, float leftExpansionAdditionalWidth, float rightExpansionAdditionalWidth)
{
    m_expansion -= leftExpansionAdditionalWidth + rightExpansionAdditionalWidth;

    leftAdditionalWidth += leftExpansionAdditionalWidth;
    rightAdditionalWidth += rightExpansionAdditionalWidth;

    m_runWidthSoFar += leftAdditionalWidth;
    m_runWidthSoFar += rightAdditionalWidth;

    if (leftAdditionalWidth) {
        if (ltr()) {
            // Left additional width in LTR means the previous (leading) glyph's right side gets expanded.
            auto leadingGlyphIndex = glyphIndexRange.leadingGlyphIndex;
            if (leadingGlyphIndex)
                glyphBuffer.expandAdvance(leadingGlyphIndex - 1, leftAdditionalWidth);
            else
                glyphBuffer.expandInitialAdvance(leftAdditionalWidth);
        } else {
            // Left additional width in RTL means the next (trailing) glyph's right side gets expanded.
            auto trailingGlyphIndex = glyphIndexRange.trailingGlyphIndex;
            if (trailingGlyphIndex + 1 < glyphBuffer.size())
                glyphBuffer.expandAdvance(trailingGlyphIndex + 1, leftAdditionalWidth);
            else {
                m_leftoverJustificationWidth = leftAdditionalWidth;
                // We can't actually add in this width just yet.
                // Add it in when the client calls advance() again or finalize().
                m_runWidthSoFar -= m_leftoverJustificationWidth;
            }
        }
    }

    if (rightAdditionalWidth) {
        // Right additional width means the current glyph's right side gets expanded. This is true for both LTR and RTL.
        glyphBuffer.expandAdvance(glyphIndexRange.trailingGlyphIndex, rightAdditionalWidth);
    }
}

void WidthIterator::applyExtraSpacingAfterShaping(GlyphBuffer& glyphBuffer, unsigned characterStartIndex, unsigned glyphBufferStartIndex, unsigned characterDestinationIndex, float startingRunWidth)
{
    Vector<std::optional<GlyphIndexRange>> characterIndexToGlyphIndexRange(m_run->length(), std::nullopt);
    Vector<float> advanceWidths(m_run->length(), 0);
    for (unsigned i = glyphBufferStartIndex; i < glyphBuffer.size(); ++i) {
        auto stringOffset = glyphBuffer.checkedStringOffsetAt(i, m_run->length());
        if (!stringOffset)
            continue;
        advanceWidths[stringOffset.value()] += width(glyphBuffer.advanceAt(i));
        auto& glyphIndexRange = characterIndexToGlyphIndexRange[stringOffset.value()];
        if (glyphIndexRange)
            glyphIndexRange->trailingGlyphIndex = i;
        else
            glyphIndexRange = {{i, i}};
    }

    // SVG can stretch advances
    if (m_run->horizontalGlyphStretch() != 1) {
        for (unsigned i = glyphBufferStartIndex; i < glyphBuffer.size(); ++i) {
            // All characters' advances get stretched, except apparently tab characters...
            // This doesn't make much sense, because even tab characters get letter-spacing...
            auto stringOffset = glyphBuffer.checkedStringOffsetAt(i, m_run->length());
            if (stringOffset && m_run.get()[stringOffset.value()] == tabCharacter)
                continue;

            auto currentAdvance = width(glyphBuffer.advanceAt(i));
            auto newAdvance = currentAdvance * m_run->horizontalGlyphStretch();
            glyphBuffer.expandAdvance(i, newAdvance - currentAdvance);
        }
    }

    auto previousCharacterClass = m_run->textSpacingState().lastCharacterClassFromPreviousRun;
    float position = m_run->xPos() + startingRunWidth;
    auto textAutospace = m_fontCascade->textAutospace();
    for (auto i = characterStartIndex; i < characterDestinationIndex; ++i) {
        auto& glyphIndexRange = characterIndexToGlyphIndexRange[i];
        if (!glyphIndexRange)
            continue;

        auto width = calculateAdditionalWidth(glyphBuffer, i, glyphIndexRange->leadingGlyphIndex, glyphIndexRange->trailingGlyphIndex, position);
        applyAdditionalWidth(glyphBuffer, glyphIndexRange.value(), width.left, width.right, width.leftExpansion, width.rightExpansion);

        auto textAutospaceSpacing = 0.f;
        auto characterClass = TextSpacing::CharacterClass::Undefined;
        if (!textAutospace.isNoAutospace()) {
            characterClass = TextSpacing::characterClass(m_run.get()[i]);
            if (textAutospace.shouldApplySpacing(characterClass, previousCharacterClass)) {
                textAutospaceSpacing = TextAutospace::textAutospaceSize(glyphBuffer.protectedFontAt(glyphIndexRange->leadingGlyphIndex));
                glyphBuffer.expandAdvanceToLogicalRight(glyphIndexRange->leadingGlyphIndex, textAutospaceSpacing);
                m_runWidthSoFar += textAutospaceSpacing;
            }
        }
        previousCharacterClass = characterClass;

        m_isAfterExpansion = (ltr() && width.rightExpansion) || (rtl() && width.leftExpansion);

        // This isn't quite perfect, because we may come across a tab character in between two glyphs which both report to correspond to a previous character.
        // But, the fundamental concept of tabs isn't really compatible with complex text shaping, so this is probably okay.
        // We can probably just do the best we can here.
        // The only alternative, to calculate this position in glyph-space rather than character-space,
        // is O(n^2) because we're iterating across the string here, rather than glyphs, so we can't keep the calculation up-to-date,
        // which means calculateAdditionalWidth() would have to calculate the result from scratch whenever it's needed.
        // And we can't do some sort of prefix-sum thing because applyAdditionalWidth() would modify the values,
        // so updating the data structure each turn of this loop would also end up being O(n^2).
        // Unfortunately, strings with tabs are more likely to be long data-table kind of strings, which means O(n^2) is not acceptable.
        // Also, even if we did the O(n^2) thing, there would still be cases that wouldn't be perfect
        // (because the fundamental concept of tabs isn't really compatible with complex text shaping),
        // so let's choose the fast-wrong approach here instead of the slow-wrong approach.
        position += advanceWidths[i]
            + width.left
            + width.right
            + width.leftExpansion
            + width.rightExpansion;
    }
}

bool WidthIterator::characterCanUseSimplifiedTextMeasuring(char32_t codePoint, bool whitespaceIsCollapsed)
{
    // This function needs to be kept in sync with applyCSSVisibilityRules().

    switch (codePoint) {
    case newlineCharacter:
    case carriageReturn:
        return true;
    case tabCharacter:
        if (!whitespaceIsCollapsed)
            return false;
        break;
    case noBreakSpace:
    case softHyphen:
    case leftToRightMark:
    case rightToLeftMark:
    case leftToRightEmbed:
    case rightToLeftEmbed:
    case leftToRightOverride:
    case rightToLeftOverride:
    case leftToRightIsolate:
    case rightToLeftIsolate:
    case popDirectionalFormatting:
    case popDirectionalIsolate:
    case firstStrongIsolate:
    case objectReplacementCharacter:
    case zeroWidthNoBreakSpace:
    case zeroWidthNonJoiner:
    case zeroWidthJoiner:
    case wordJoiner:
    case zeroWidthSpace:
    case functionApplication:
    case invisibleTimes:
    case invisibleSeparator:
    case inhibitSymmetricSwapping:
    case activateSymmetricSwapping:
    case inhibitArabicFormShaping:
    case activateArabicFormShaping:
    case nationalDigitShapes:
    case nominalDigitShapes:
    case mediumShade:
        return false;
    }

    if (codePoint >= HiraganaLetterSmallA || isControlCharacter(codePoint))
        return false;

    return true;
}

void WidthIterator::applyCSSVisibilityRules(GlyphBuffer& glyphBuffer, unsigned glyphBufferStartIndex)
{
    // This function needs to be kept in sync with characterCanUseSimplifiedTextMeasuring().

    Vector<unsigned> glyphsIndicesToBeDeleted;

    float yPosition = height(glyphBuffer.initialAdvance());

    auto adjustForSyntheticBold = [&](auto index) {
        auto glyph = glyphBuffer.glyphAt(index);
        auto syntheticBoldOffset = glyph == deletedGlyph ? 0 : glyphBuffer.fontAt(index).syntheticBoldOffset();
        m_runWidthSoFar += syntheticBoldOffset;
        auto& advance = glyphBuffer.advances(index)[0];
        setWidth(advance, width(advance) + syntheticBoldOffset);
    };

    auto clobberGlyph = [&](auto index, auto newGlyph) {
        glyphBuffer.glyphs(index)[0] = newGlyph;
    };

    // FIXME: It's technically wrong to call clobberAdvance or deleteGlyph here, because this is after initialAdvances have been
    // applied. If the last glyph in a run needs to have its advance clobbered, but the next run has an initial advance, we need
    // to apply the initial advance on the new clobbered advance, rather than clobbering the initial advance entirely.

    auto clobberAdvance = [&](auto index, auto newAdvance) {
        auto advanceBeforeClobbering = glyphBuffer.advanceAt(index);
        glyphBuffer.advances(index)[0] = makeGlyphBufferAdvance(newAdvance, height(advanceBeforeClobbering));
        m_runWidthSoFar += width(glyphBuffer.advanceAt(index)) - width(advanceBeforeClobbering);
        glyphBuffer.origins(index)[0] = makeGlyphBufferOrigin(0, -yPosition);
    };

    auto deleteGlyph = [&](auto index) {
        m_runWidthSoFar -= width(glyphBuffer.advanceAt(index));
        glyphBuffer.deleteGlyphWithoutAffectingSize(index);
    };

    auto makeGlyphInvisible = [&](auto index) {
        glyphBuffer.makeGlyphInvisible(index);
    };

    for (unsigned i = glyphBufferStartIndex; i < glyphBuffer.size(); yPosition += height(glyphBuffer.advanceAt(i)), ++i) {
        auto stringOffset = glyphBuffer.checkedStringOffsetAt(i, m_run->length());
        if (!stringOffset)
            continue;
        auto characterResponsibleForThisGlyph = m_run.get()[stringOffset.value()];

        switch (characterResponsibleForThisGlyph) {
        case newlineCharacter:
        case carriageReturn:
            ASSERT(glyphBuffer.fonts(i)[0]);
            // FIXME: It isn't quite right to use the space glyph here, because the space character may be supposed to render with a totally unrelated font (because of fallback).
            // Instead, we should probably somehow have the caller pass in a Font/glyph pair to use in this situation.
            if (auto spaceGlyph = glyphBuffer.fontAt(i).spaceGlyph())
                clobberGlyph(i, spaceGlyph);
            adjustForSyntheticBold(i);
            continue;
        case noBreakSpace:
            adjustForSyntheticBold(i);
            continue;
        case tabCharacter:
            makeGlyphInvisible(i);
            adjustForSyntheticBold(i);
            continue;
        }

        // https://www.w3.org/TR/css-text-3/#white-space-processing
        // "Unsupported Default_ignorable characters must be ignored for text rendering."
        if (FontCascade::isCharacterWhoseGlyphsShouldBeDeletedForTextRendering(characterResponsibleForThisGlyph)) {
            deleteGlyph(i);
            continue;
        }
        // "Control characters (Unicode category Cc)—other than tabs (U+0009), line feeds (U+000A), carriage returns (U+000D) and sequences that form a segment break—must be rendered as a visible glyph"
        if (isControlCharacter(characterResponsibleForThisGlyph)) {
            // Let's assume that .notdef is visible.
            GlyphBufferGlyph visibleGlyph = 0;
            clobberGlyph(i, visibleGlyph);
            clobberAdvance(i, glyphBuffer.protectedFontAt(i)->widthForGlyph(visibleGlyph));
            continue;
        }

        adjustForSyntheticBold(i);
    }
}

void WidthIterator::finalize(GlyphBuffer& buffer)
{
    ASSERT(rtl() || !m_leftoverJustificationWidth);
    // In LTR these do nothing. In RTL, these add left width by moving the whole run to the right.
    buffer.expandInitialAdvance(m_leftoverInitialAdvance);
    m_runWidthSoFar += width(m_leftoverInitialAdvance);
    buffer.expandInitialAdvance(m_leftoverJustificationWidth);
    m_runWidthSoFar += m_leftoverJustificationWidth;
    m_leftoverJustificationWidth = 0;
}

void WidthIterator::advance(unsigned offset, GlyphBuffer& glyphBuffer)
{
    m_containsTabs = false;
    unsigned length = m_run->length();

    if (offset > length)
        offset = length;

    if (m_currentCharacterIndex >= offset)
        return;

    unsigned characterStartIndex = m_currentCharacterIndex;
    unsigned glyphBufferStartIndex = glyphBuffer.size();
    float startingRunWidth = m_runWidthSoFar;

    if (m_run->is8Bit()) {
        Latin1TextIterator textIterator(m_run->subspan8(m_currentCharacterIndex), m_currentCharacterIndex, offset);
        advanceInternal(textIterator, glyphBuffer);
    } else {
#if USE(CLUSTER_AWARE_WIDTH_ITERATOR)
        ComposedCharacterClusterTextIterator textIterator(m_run->subspan16(m_currentCharacterIndex), m_currentCharacterIndex, offset);
#else
        SurrogatePairAwareTextIterator textIterator(m_run->subspan16(m_currentCharacterIndex), m_currentCharacterIndex, offset);
#endif
        advanceInternal(textIterator, glyphBuffer);
    }

    // In general, we have to apply spacing after shaping, because shaping requires its input to be unperturbed (see https://bugs.webkit.org/show_bug.cgi?id=215052).
    // So, if there's extra spacing to add, do it here after shaping occurs.
    if (glyphBufferStartIndex < glyphBuffer.size()) {
        glyphBuffer.expandAdvance(glyphBufferStartIndex, m_leftoverJustificationWidth);
        m_runWidthSoFar += m_leftoverJustificationWidth;
        m_leftoverJustificationWidth = 0;
    }

    if (hasExtraSpacing() || m_containsTabs || m_run->horizontalGlyphStretch() != 1 || !m_fontCascade->textAutospace().isNoAutospace())
        applyExtraSpacingAfterShaping(glyphBuffer, characterStartIndex, glyphBufferStartIndex, offset, startingRunWidth);

    applyCSSVisibilityRules(glyphBuffer, glyphBufferStartIndex);
}

// FIXME: It's pretty much never right to advance just one character.
bool WidthIterator::advanceOneCharacter(float& width, GlyphBuffer& glyphBuffer)
{
    unsigned oldSize = glyphBuffer.size();
    advance(m_currentCharacterIndex + 1, glyphBuffer);
    float w = 0;
    for (unsigned i = oldSize; i < glyphBuffer.size(); ++i)
        w += WebCore::width(glyphBuffer.advanceAt(i));
    width = w;
    return glyphBuffer.size() > oldSize;
}

} // namespace WebCore
