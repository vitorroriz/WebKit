/*
 * Copyright (C) 2007-2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ComplexTextController.h"

#include "FloatSize.h"
#include "FontCascade.h"
#include "FontCascadeInlines.h"
#include "GlyphBuffer.h"
#include "RenderBlock.h"
#include "RenderText.h"
#include "TextRun.h"
#include <algorithm>
#include <unicode/ubrk.h>
#include <unicode/utf16.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/CharacterProperties.h>
#include <wtf/text/TextBreakIterator.h>
#include <wtf/unicode/CharacterNames.h>

#if PLATFORM(IOS_FAMILY)
#include <CoreText/CoreText.h>
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ComplexTextController);

class TextLayout {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(TextLayout);
public:
    static bool isNeeded(RenderText& text, const FontCascade& fontCascade)
    {
        TextRun run = RenderBlock::constructTextRun(text, text.style());
        return fontCascade.codePath(run) == FontCascade::CodePath::Complex;
    }

    TextLayout(RenderText& text, const FontCascade& fontCascade, float xPos)
        : m_fontCascade(fontCascade)
        , m_run(constructTextRun(text, xPos))
        , m_controller(makeUniqueRef<ComplexTextController>(m_fontCascade, m_run, true))
    {
    }

    float width(unsigned from, unsigned len, SingleThreadWeakHashSet<const Font>* fallbackFonts)
    {
        m_controller->advance(from, 0, GlyphIterationStyle::ByWholeGlyphs, fallbackFonts);
        float beforeWidth = m_controller->runWidthSoFar();
        if (m_fontCascade.wordSpacing() && from && FontCascade::treatAsSpace(m_run[from]))
            beforeWidth += m_fontCascade.wordSpacing();
        m_controller->advance(from + len, 0, GlyphIterationStyle::ByWholeGlyphs, fallbackFonts);
        float afterWidth = m_controller->runWidthSoFar();
        return afterWidth - beforeWidth;
    }

private:
    static TextRun constructTextRun(RenderText& text, float xPos)
    {
        TextRun run = RenderBlock::constructTextRun(text, text.style());
        run.setXPos(xPos);
        return run;
    }

    // ComplexTextController has only references to its FontCascade and TextRun so they must be kept alive here.
    FontCascade m_fontCascade;
    TextRun m_run;
    const UniqueRef<ComplexTextController> m_controller;
};

void TextLayoutDeleter::operator()(TextLayout* layout) const
{
    delete layout;
}

std::unique_ptr<TextLayout, TextLayoutDeleter> FontCascade::createLayout(RenderText& text, float xPos, bool collapseWhiteSpace) const
{
    if (!collapseWhiteSpace || !TextLayout::isNeeded(text, *this))
        return nullptr;
    return std::unique_ptr<TextLayout, TextLayoutDeleter>(new TextLayout(text, *this, xPos));
}

void ComplexTextController::computeExpansionOpportunity()
{
    if (!m_expansion)
        m_expansionPerOpportunity = 0;
    else {
        unsigned expansionOpportunityCount = FontCascade::expansionOpportunityCount(m_run->text(), m_run->ltr() ? TextDirection::LTR : TextDirection::RTL, m_run->expansionBehavior()).first;

        if (!expansionOpportunityCount)
            m_expansionPerOpportunity = 0;
        else
            m_expansionPerOpportunity = m_expansion / expansionOpportunityCount;
    }
}

ComplexTextController::ComplexTextController(const FontCascade& fontCascade, const TextRun& run, bool mayUseNaturalWritingDirection, SingleThreadWeakHashSet<const Font>* fallbackFonts, bool forTextEmphasis)
    : m_fallbackFonts(fallbackFonts)
    , m_fontCascade(fontCascade)
    , m_run(run)
    , m_end(run.length())
    , m_expansion(run.expansion())
    , m_mayUseNaturalWritingDirection(mayUseNaturalWritingDirection)
    , m_forTextEmphasis(forTextEmphasis)
    , m_textSpacingState(run.textSpacingState())
{
    computeExpansionOpportunity();

    collectComplexTextRuns();

    finishConstruction();
}

ComplexTextController::ComplexTextController(const FontCascade& fontCascade, const TextRun& run, Vector<Ref<ComplexTextRun>>& runs)
    : m_fontCascade(fontCascade)
    , m_run(run)
    , m_end(run.length())
    , m_expansion(run.expansion())
{
    computeExpansionOpportunity();

    for (auto& run : runs)
        m_complexTextRuns.append(run.ptr());

    finishConstruction();
}

ComplexTextController::ComplexTextController(const TextRun& run, const FontCascade& fontCascade)
    : m_fontCascade(fontCascade)
    , m_run(run)
    , m_end(run.length())
{
}

std::pair<float, float> ComplexTextController::enclosingGlyphBoundsForTextRun(const FontCascade& fontCascade, const TextRun& textRun)
{
    auto textController = ComplexTextController { textRun, fontCascade };
    textController.collectComplexTextRuns();

    auto enclosingAscent = std::optional<float> { };
    auto enclosingDescent = std::optional<float> { };

    for (size_t runIndex = 0; runIndex < textController.m_complexTextRuns.size(); ++runIndex) {
        auto& complexTextRun = *textController.m_complexTextRuns[runIndex];
        auto& font = complexTextRun.font();
        auto glyphs = complexTextRun.glyphs();
        ASSERT(glyphs.size() == complexTextRun.glyphCount());

#if USE(CORE_TEXT)
        auto glyphBounds = font.boundsForGlyphs(glyphs);
        for (auto& bounds : glyphBounds) {
#else
        for (auto& glyph : glyphs) {
            auto bounds = font.boundsForGlyph(glyph);
#endif
            enclosingAscent = std::min(enclosingAscent.value_or(bounds.y()), bounds.y());
            enclosingDescent = std::max(enclosingDescent.value_or(bounds.maxY()), bounds.maxY());
        }
    }
    return { enclosingAscent.value_or(0.f), enclosingDescent.value_or(0.f) };
}

void ComplexTextController::finishConstruction()
{
    adjustGlyphsAndAdvances();

    if (!m_isLTROnly) {
        unsigned length = m_complexTextRuns.size();
        m_runIndices = Vector<unsigned, 16>(length, [&](size_t i) {
            return length - i - 1;
        });
        std::ranges::sort(m_runIndices.mutableSpan(),
            [this](auto a, auto b) {
                return stringBegin(*m_complexTextRuns[a]) < stringBegin(*m_complexTextRuns[b]);
            });

        unsigned glyphCountSoFar = 0;
        m_glyphCountFromStartToIndex = Vector<unsigned, 16>(length, [&](size_t i) {
            auto glyphCountThisTime = glyphCountSoFar;
            glyphCountSoFar += m_complexTextRuns[i]->glyphCount();
            return glyphCountThisTime;
        });
    }
}

unsigned ComplexTextController::offsetForPosition(float h, bool includePartialGlyphs)
{
    if (h >= m_totalAdvance.width())
        return m_run->ltr() ? m_end : 0;

    if (h < 0)
        return m_run->ltr() ? 0 : m_end;

    float x = h;

    size_t runCount = m_complexTextRuns.size();
    unsigned offsetIntoAdjustedGlyphs = 0;

    for (size_t r = 0; r < runCount; ++r) {
        const ComplexTextRun& complexTextRun = *m_complexTextRuns[r];
        for (unsigned j = 0; j < complexTextRun.glyphCount(); ++j) {
            unsigned index = offsetIntoAdjustedGlyphs + j;
            float adjustedAdvance = m_adjustedBaseAdvances[index].width();
            bool hit = m_run->ltr() ? x < adjustedAdvance : (x <= adjustedAdvance && adjustedAdvance);
            if (hit) {
                unsigned hitGlyphStart = complexTextRun.indexAt(j);
                unsigned hitGlyphEnd;
                if (m_run->ltr())
                    hitGlyphEnd = std::max(hitGlyphStart, j + 1 < complexTextRun.glyphCount() ? complexTextRun.indexAt(j + 1) : complexTextRun.indexEnd());
                else
                    hitGlyphEnd = std::max(hitGlyphStart, j > 0 ? complexTextRun.indexAt(j - 1) : complexTextRun.indexEnd());

                // FIXME: Instead of dividing the glyph's advance equally between the characters, this
                // could use the glyph's "ligature carets". This is available in CoreText via CTFontGetLigatureCaretPositions().
                unsigned hitIndex;
                if (m_run->ltr())
                    hitIndex = hitGlyphStart + (hitGlyphEnd - hitGlyphStart) * (x / adjustedAdvance);
                else {
                    if (hitGlyphStart == hitGlyphEnd)
                        hitIndex = hitGlyphStart;
                    else if (x)
                        hitIndex = hitGlyphEnd - (hitGlyphEnd - hitGlyphStart) * (x / adjustedAdvance);
                    else
                        hitIndex = hitGlyphEnd - 1;
                }

                unsigned stringLength = complexTextRun.stringLength();
                CachedTextBreakIterator cursorPositionIterator(complexTextRun.characters(), { }, TextBreakIterator::CaretMode { }, nullAtom());
                unsigned clusterStart;
                if (cursorPositionIterator.isBoundary(hitIndex))
                    clusterStart = hitIndex;
                else
                    clusterStart = cursorPositionIterator.preceding(hitIndex).value_or(0);

                if (!includePartialGlyphs)
                    return complexTextRun.stringLocation() + clusterStart;

                unsigned clusterEnd = cursorPositionIterator.following(hitIndex).value_or(stringLength);

                float clusterWidth;
                // FIXME: The search stops at the boundaries of complexTextRun. In theory, it should go on into neighboring ComplexTextRuns
                // derived from the same CTLine. In practice, we do not expect there to be more than one CTRun in a CTLine, as no
                // reordering and no font fallback should occur within a CTLine.
                if (clusterEnd - clusterStart > 1) {
                    clusterWidth = adjustedAdvance;
                    if (j) {
                        unsigned firstGlyphBeforeCluster = j - 1;
                        while (complexTextRun.indexAt(firstGlyphBeforeCluster) >= clusterStart && complexTextRun.indexAt(firstGlyphBeforeCluster) < clusterEnd) {
                            float width = m_adjustedBaseAdvances[offsetIntoAdjustedGlyphs + firstGlyphBeforeCluster].width();
                            clusterWidth += width;
                            x += width;
                            if (!firstGlyphBeforeCluster)
                                break;
                            firstGlyphBeforeCluster--;
                        }
                    }
                    unsigned firstGlyphAfterCluster = j + 1;
                    while (firstGlyphAfterCluster < complexTextRun.glyphCount() && complexTextRun.indexAt(firstGlyphAfterCluster) >= clusterStart && complexTextRun.indexAt(firstGlyphAfterCluster) < clusterEnd) {
                        clusterWidth += m_adjustedBaseAdvances[offsetIntoAdjustedGlyphs + firstGlyphAfterCluster].width();
                        firstGlyphAfterCluster++;
                    }
                } else {
                    clusterWidth = adjustedAdvance / (hitGlyphEnd - hitGlyphStart);
                    x -=  clusterWidth * (m_run->ltr() ? hitIndex - hitGlyphStart : hitGlyphEnd - hitIndex - 1);
                }
                if (x <= clusterWidth / 2)
                    return complexTextRun.stringLocation() + (m_run->ltr() ? clusterStart : clusterEnd);
                return complexTextRun.stringLocation() + (m_run->ltr() ? clusterEnd : clusterStart);
            }
            x -= adjustedAdvance;
        }
        offsetIntoAdjustedGlyphs += complexTextRun.glyphCount();
    }

    ASSERT_NOT_REACHED();
    return 0;
}

void ComplexTextController::advanceByCombiningCharacterSequence(const CachedTextBreakIterator& graphemeClusterIterator, unsigned& currentIndex, char32_t& baseCharacter)
{
    unsigned remainingCharacters = m_end - currentIndex;
    ASSERT(remainingCharacters);

    std::array<char16_t, 2> buffer;
    unsigned bufferLength = 1;
    buffer[0] = m_run.get()[currentIndex];
    buffer[1] = 0;
    if (remainingCharacters >= 2) {
        buffer[1] = m_run.get()[currentIndex + 1];
        bufferLength = 2;
    }

    unsigned i = 0;
    U16_NEXT(buffer, i, bufferLength, baseCharacter);
    if (U_IS_SURROGATE(baseCharacter)) {
        currentIndex += i;
        return;
    }

    int delta = remainingCharacters;
    if (auto following = graphemeClusterIterator.following(currentIndex))
        delta = *following - currentIndex;

    currentIndex += delta;
}

void ComplexTextController::collectComplexTextRuns()
{
    if (!m_end || !m_fontCascade->size())
        return;

    // We break up glyph run generation for the string by Font.

    std::span<const char16_t> baseOfString = [&] {
        // We need a 16-bit string to pass to Core Text.
        if (!m_run->is8Bit())
            return m_run->span16();
        String stringConvertedTo16Bit = m_run->textAsString();
        stringConvertedTo16Bit.convertTo16Bit();
        auto characters = stringConvertedTo16Bit.span16();
        m_stringsFor8BitRuns.append(WTFMove(stringConvertedTo16Bit));
        return characters;
    }();

    auto fontVariantCaps = m_fontCascade->fontDescription().variantCaps();
    bool dontSynthesizeSmallCaps = !m_fontCascade->fontDescription().hasAutoFontSynthesisSmallCaps();
    bool engageAllSmallCapsProcessing = fontVariantCaps == FontVariantCaps::AllSmall || fontVariantCaps == FontVariantCaps::AllPetite;
    bool engageSmallCapsProcessing = engageAllSmallCapsProcessing || fontVariantCaps == FontVariantCaps::Small || fontVariantCaps == FontVariantCaps::Petite;
    auto shouldProcessTextSpacingTrim = !m_fontCascade->textSpacingTrim().isSpaceAll();

    if (engageAllSmallCapsProcessing || engageSmallCapsProcessing)
        m_smallCapsBuffer.resize(m_end);

    unsigned currentIndex = 0;
    unsigned indexOfFontTransition = 0;

    RefPtr<const Font> font;
    RefPtr<const Font> nextFont;
    RefPtr<const Font> synthesizedFont;
    RefPtr<const Font> smallSynthesizedFont;
    RefPtr<const Font> halfWidthFont;

    CachedTextBreakIterator graphemeClusterIterator(m_run->text(), { }, TextBreakIterator::CharacterMode { }, m_fontCascade->fontDescription().computedLocale());

    char32_t baseCharacter;
    advanceByCombiningCharacterSequence(graphemeClusterIterator, currentIndex, baseCharacter);

    // We don't perform font fallback on the capitalized characters when small caps is synthesized.
    // We may want to change this code to do so in the future; if we do, then the logic in initiateFontLoadingByAccessingGlyphDataIfApplicable()
    // would need to be updated accordingly too.
    nextFont = m_fontCascade->fontForCombiningCharacterSequence(baseOfString.first(currentIndex));

    if (shouldProcessTextSpacingTrim && nextFont && !nextFont->isSystemFontFallbackPlaceholder()) {
        TextSpacing::CharactersData charactersData = { .currentCharacter = baseCharacter, .currentCharacterClass = TextSpacing::characterClass(baseCharacter) };
        halfWidthFont = TextSpacing::getHalfWidthFontIfNeeded(*nextFont, m_fontCascade->textSpacingTrim(), charactersData);
        nextFont = halfWidthFont ? halfWidthFont : nextFont;
    }

    bool isSmallCaps = false;
    bool nextIsSmallCaps = false;

    auto capitalizedBase = capitalized(baseCharacter);
    if (shouldSynthesizeSmallCaps(dontSynthesizeSmallCaps, nextFont.get(), baseCharacter, capitalizedBase, fontVariantCaps, engageAllSmallCapsProcessing)) {
        synthesizedFont = nextFont->noSynthesizableFeaturesFont();
        smallSynthesizedFont = synthesizedFont->smallCapsFont(m_fontCascade->fontDescription());
        char32_t characterToWrite = capitalizedBase ? capitalizedBase.value() : baseOfString[0];
        unsigned characterIndex = 0;
        U16_APPEND_UNSAFE(m_smallCapsBuffer, characterIndex, characterToWrite);
        for (unsigned i = characterIndex; i < currentIndex; ++i)
            m_smallCapsBuffer[i] = baseOfString[i];
        nextIsSmallCaps = true;
    }

    while (currentIndex < m_end) {
        font = nextFont.get();
        isSmallCaps = nextIsSmallCaps;
        auto previousIndex = currentIndex;
        halfWidthFont = nullptr;

        advanceByCombiningCharacterSequence(graphemeClusterIterator, currentIndex, baseCharacter);

        if (synthesizedFont) {
            if (auto capitalizedBase = capitalized(baseCharacter)) {
                unsigned characterIndex = previousIndex;
                U16_APPEND_UNSAFE(m_smallCapsBuffer, characterIndex, capitalizedBase.value());
                for (unsigned i = characterIndex; i < currentIndex; ++i)
                    m_smallCapsBuffer[i] = baseOfString[i];
                nextIsSmallCaps = true;
            } else {
                if (engageAllSmallCapsProcessing) {
                    for (unsigned i = previousIndex; i < currentIndex; ++i)
                        m_smallCapsBuffer[i] = baseOfString[i];
                }
                nextIsSmallCaps = engageAllSmallCapsProcessing;
            }
        }

        nextFont = m_fontCascade->fontForCombiningCharacterSequence(baseOfString.subspan(previousIndex, currentIndex - previousIndex));

        if (shouldProcessTextSpacingTrim && nextFont && !nextFont->isSystemFontFallbackPlaceholder()) {
            TextSpacing::CharactersData charactersData = { .currentCharacter = baseCharacter, .currentCharacterClass = TextSpacing::characterClass(baseCharacter) };
            halfWidthFont = TextSpacing::getHalfWidthFontIfNeeded(*nextFont, m_fontCascade->textSpacingTrim(), charactersData);
            nextFont = halfWidthFont ? halfWidthFont : nextFont;
        }

        capitalizedBase = capitalized(baseCharacter);
        if (!synthesizedFont && shouldSynthesizeSmallCaps(dontSynthesizeSmallCaps, nextFont.get(), baseCharacter, capitalizedBase, fontVariantCaps, engageAllSmallCapsProcessing)) {
            // Rather than synthesize each character individually, we should synthesize the entire "run" if any character requires synthesis.
            synthesizedFont = nextFont->noSynthesizableFeaturesFont();
            smallSynthesizedFont = synthesizedFont->smallCapsFont(m_fontCascade->fontDescription());
            nextIsSmallCaps = true;
            currentIndex = indexOfFontTransition;
            continue;
        }

        if (nextFont != font || nextIsSmallCaps != isSmallCaps) {
            unsigned itemLength = previousIndex - indexOfFontTransition;
            if (itemLength) {
                unsigned itemStart = indexOfFontTransition;
                if (synthesizedFont) {
                    if (isSmallCaps)
                        collectComplexTextRunsForCharacters(m_smallCapsBuffer.subspan(itemStart, itemLength), itemStart, smallSynthesizedFont.get());
                    else
                        collectComplexTextRunsForCharacters(baseOfString.subspan(itemStart, itemLength), itemStart, synthesizedFont.get());
                } else
                    collectComplexTextRunsForCharacters(baseOfString.subspan(itemStart, itemLength), itemStart, font.get());
                if (nextFont != font) {
                    synthesizedFont = nullptr;
                    smallSynthesizedFont = nullptr;
                    nextIsSmallCaps = false;
                }
            }
            indexOfFontTransition = previousIndex;
        }
    }

    ASSERT(m_end >= indexOfFontTransition);
    unsigned itemLength = m_end - indexOfFontTransition;
    if (itemLength) {
        unsigned itemStart = indexOfFontTransition;
        if (synthesizedFont) {
            if (nextIsSmallCaps)
                collectComplexTextRunsForCharacters(m_smallCapsBuffer.subspan(itemStart, itemLength), itemStart, smallSynthesizedFont.get());
            else
                collectComplexTextRunsForCharacters(baseOfString.subspan(itemStart, itemLength), itemStart, synthesizedFont.get());
        } else
            collectComplexTextRunsForCharacters(baseOfString.subspan(itemStart, itemLength), itemStart, nextFont.get());
    }

    if (!m_run->ltr())
        m_complexTextRuns.reverse();
}

unsigned ComplexTextController::ComplexTextRun::indexAt(unsigned i) const
{
    ASSERT(i < m_glyphCount);

    return m_coreTextIndices[i];
}

void ComplexTextController::ComplexTextRun::setIsNonMonotonic()
{
    ASSERT(m_isMonotonic);
    m_isMonotonic = false;

    Vector<bool, 64> mappedIndices(stringLength(), false);
    for (unsigned i = 0; i < m_glyphCount; ++i)
        mappedIndices[indexAt(i)] = true;

    m_glyphEndOffsets.grow(m_glyphCount);
    for (unsigned i = 0; i < m_glyphCount; ++i) {
        unsigned nextMappedIndex = m_indexEnd;
        for (unsigned j = indexAt(i) + 1; j < stringLength(); ++j) {
            if (mappedIndices[j]) {
                nextMappedIndex = j;
                break;
            }
        }
        m_glyphEndOffsets[i] = nextMappedIndex;
    }
}

unsigned ComplexTextController::indexOfCurrentRun(unsigned& leftmostGlyph)
{
    leftmostGlyph = 0;
    
    size_t runCount = m_complexTextRuns.size();
    if (m_currentRun >= runCount)
        return runCount;

    if (m_isLTROnly) {
        for (unsigned i = 0; i < m_currentRun; ++i)
            leftmostGlyph += m_complexTextRuns[i]->glyphCount();
        return m_currentRun;
    }

    unsigned currentRunIndex = m_runIndices[m_currentRun];
    leftmostGlyph = m_glyphCountFromStartToIndex[currentRunIndex];
    return currentRunIndex;
}

unsigned ComplexTextController::incrementCurrentRun(unsigned& leftmostGlyph)
{
    if (m_isLTROnly) {
        leftmostGlyph += m_complexTextRuns[m_currentRun++]->glyphCount();
        return m_currentRun;
    }

    m_currentRun++;
    leftmostGlyph = 0;
    return indexOfCurrentRun(leftmostGlyph);
}

float ComplexTextController::runWidthSoFarFraction(unsigned glyphStartOffset, unsigned glyphEndOffset, unsigned oldCharacterInCurrentGlyph, GlyphIterationStyle iterationStyle) const
{
    // FIXME: Instead of dividing the glyph's advance equally between the characters, this
    // could use the glyph's "ligature carets". This is available in CoreText via CTFontGetLigatureCaretPositions().
    if (glyphStartOffset == glyphEndOffset) {
        // When there are multiple glyphs per character we need to advance by the full width of the glyph.
        ASSERT(m_characterInCurrentGlyph == oldCharacterInCurrentGlyph);
        return 1;
    }

    if (iterationStyle == GlyphIterationStyle::ByWholeGlyphs) {
        if (!oldCharacterInCurrentGlyph)
            return 1;
        return 0;
    }

    return static_cast<float>(m_characterInCurrentGlyph - oldCharacterInCurrentGlyph) / (glyphEndOffset - glyphStartOffset);
}

void ComplexTextController::advance(unsigned offset, GlyphBuffer* glyphBuffer, GlyphIterationStyle iterationStyle, SingleThreadWeakHashSet<const Font>* fallbackFonts)
{
    if (offset > m_end)
        offset = m_end;

    if (offset < m_currentCharacter) {
        m_runWidthSoFar = 0;
        m_numGlyphsSoFar = 0;
        m_currentRun = 0;
        m_glyphInCurrentRun = 0;
        m_characterInCurrentGlyph = 0;
    }

    m_currentCharacter = offset;

    size_t runCount = m_complexTextRuns.size();

    unsigned indexOfLeftmostGlyphInCurrentRun = 0; // Relative to the beginning of ComplexTextController.
    unsigned currentRunIndex = indexOfCurrentRun(indexOfLeftmostGlyphInCurrentRun);
    while (m_currentRun < runCount) {
        const ComplexTextRun& complexTextRun = *m_complexTextRuns[currentRunIndex];
        bool ltr = complexTextRun.isLTR();
        unsigned glyphCount = complexTextRun.glyphCount();
        unsigned glyphIndexIntoCurrentRun = ltr ? m_glyphInCurrentRun : glyphCount - 1 - m_glyphInCurrentRun;
        unsigned glyphIndexIntoComplexTextController = indexOfLeftmostGlyphInCurrentRun + glyphIndexIntoCurrentRun;
        if (fallbackFonts && &complexTextRun.font() != m_fontCascade->primaryFont().ptr())
            fallbackFonts->add(complexTextRun.font());

        // We must store the initial advance for the first glyph we are going to draw.
        // When leftmostGlyph is 0, it represents the first glyph to draw, taking into
        // account the text direction.
        if (!indexOfLeftmostGlyphInCurrentRun && glyphBuffer)
            glyphBuffer->setInitialAdvance(makeGlyphBufferAdvance(complexTextRun.initialAdvance()));

        while (m_glyphInCurrentRun < glyphCount) {
            unsigned glyphStartOffset = complexTextRun.indexAt(glyphIndexIntoCurrentRun);
            unsigned glyphEndOffset;
            if (complexTextRun.isMonotonic()) {
                if (ltr)
                    glyphEndOffset = std::max(glyphStartOffset, glyphIndexIntoCurrentRun + 1 < glyphCount ? complexTextRun.indexAt(glyphIndexIntoCurrentRun + 1) : complexTextRun.indexEnd());
                else
                    glyphEndOffset = std::max(glyphStartOffset, glyphIndexIntoCurrentRun > 0 ? complexTextRun.indexAt(glyphIndexIntoCurrentRun - 1) : complexTextRun.indexEnd());
            } else
                glyphEndOffset = complexTextRun.endOffsetAt(glyphIndexIntoCurrentRun);

            FloatSize adjustedBaseAdvance = m_adjustedBaseAdvances[glyphIndexIntoComplexTextController];

            if (glyphStartOffset + complexTextRun.stringLocation() >= m_currentCharacter)
                return;

            if (glyphBuffer && !m_characterInCurrentGlyph) {
                ASSERT(m_textAutoSpaceSpacings.isEmpty() || m_textAutoSpaceSpacings.size() == m_adjustedBaseAdvances.size());
                auto textAutoSpaceSpacing = m_textAutoSpaceSpacings.isEmpty() ? 0.0 : m_textAutoSpaceSpacings[glyphIndexIntoComplexTextController];
                auto currentGlyphOrigin = glyphOrigin(glyphIndexIntoComplexTextController);
                currentGlyphOrigin.move(textAutoSpaceSpacing, 0);
                GlyphBufferAdvance paintAdvance = makeGlyphBufferAdvance(adjustedBaseAdvance);
                if (!glyphIndexIntoCurrentRun) {
                    // The first layout advance of every run includes the "initial layout advance." However, here, we need
                    // paint advances, so subtract it out before transforming the layout advance into a paint advance.
                    setWidth(paintAdvance, width(paintAdvance) - (complexTextRun.initialAdvance().width() - currentGlyphOrigin.x()));
                    setHeight(paintAdvance, height(paintAdvance) - (complexTextRun.initialAdvance().height() - currentGlyphOrigin.y()));
                }
                setWidth(paintAdvance, width(paintAdvance) + glyphOrigin(glyphIndexIntoComplexTextController + 1).x() - currentGlyphOrigin.x());
                setHeight(paintAdvance, height(paintAdvance) + glyphOrigin(glyphIndexIntoComplexTextController + 1).y() - currentGlyphOrigin.y());
                if (glyphIndexIntoCurrentRun == glyphCount - 1 && currentRunIndex + 1 < runCount) {
                    // Our paint advance points to the end of the run. However, the next run may have an
                    // initial advance, and our paint advance needs to point to the location of the next
                    // glyph. So, we need to add in the next run's initial advance.
                    setWidth(paintAdvance, width(paintAdvance) - glyphOrigin(glyphIndexIntoComplexTextController + 1).x() + m_complexTextRuns[currentRunIndex + 1]->initialAdvance().width());
                    setHeight(paintAdvance, height(paintAdvance) - glyphOrigin(glyphIndexIntoComplexTextController + 1).y() + m_complexTextRuns[currentRunIndex + 1]->initialAdvance().height());
                }
                setHeight(paintAdvance, -height(paintAdvance)); // Increasing y points down
                glyphBuffer->add(m_adjustedGlyphs[glyphIndexIntoComplexTextController], complexTextRun.font(), paintAdvance, complexTextRun.indexAt(m_glyphInCurrentRun) + complexTextRun.stringLocation(), FloatPoint(textAutoSpaceSpacing, 0));
            }

            unsigned oldCharacterInCurrentGlyph = m_characterInCurrentGlyph;
            m_characterInCurrentGlyph = std::min(m_currentCharacter - complexTextRun.stringLocation(), glyphEndOffset) - glyphStartOffset;
            m_runWidthSoFar += adjustedBaseAdvance.width() * runWidthSoFarFraction(glyphStartOffset, glyphEndOffset, oldCharacterInCurrentGlyph, iterationStyle);

            if (glyphEndOffset + complexTextRun.stringLocation() > m_currentCharacter)
                return;

            m_numGlyphsSoFar++;
            m_glyphInCurrentRun++;
            m_characterInCurrentGlyph = 0;
            if (ltr) {
                glyphIndexIntoCurrentRun++;
                glyphIndexIntoComplexTextController++;
            } else {
                glyphIndexIntoCurrentRun--;
                glyphIndexIntoComplexTextController--;
            }
        }
        currentRunIndex = incrementCurrentRun(indexOfLeftmostGlyphInCurrentRun);
        m_glyphInCurrentRun = 0;
    }
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
    if (isAfterExpansion)
        expandLeft = false;
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

void ComplexTextController::adjustGlyphsAndAdvances()
{
    bool afterExpansion = m_run->expansionBehavior().left == ExpansionBehavior::Behavior::Forbid;
    size_t runCount = m_complexTextRuns.size();
    bool hasExtraSpacing = (m_fontCascade->letterSpacing() || m_fontCascade->wordSpacing() || m_expansion) && !m_run->spacingDisabled();
    bool runForcesLeftExpansion = m_run->expansionBehavior().left == ExpansionBehavior::Behavior::Force;
    bool runForcesRightExpansion = m_run->expansionBehavior().right == ExpansionBehavior::Behavior::Force;
    bool runForbidsLeftExpansion = m_run->expansionBehavior().left == ExpansionBehavior::Behavior::Forbid;
    bool runForbidsRightExpansion = m_run->expansionBehavior().right == ExpansionBehavior::Behavior::Forbid;

    TextSpacing::CharacterClass previousCharacterClass = m_textSpacingState.lastCharacterClassFromPreviousRun;
    // We are iterating in glyph order, not string order. Compare this to WidthIterator::advanceInternal()
    for (size_t runIndex = 0; runIndex < runCount; ++runIndex) {
        ComplexTextRun& complexTextRun = *m_complexTextRuns[runIndex];
        unsigned glyphCount = complexTextRun.glyphCount();
        const Font& font = complexTextRun.font();

        if (!complexTextRun.isLTR())
            m_isLTROnly = false;

        auto glyphs = complexTextRun.glyphs();
        auto advances = complexTextRun.baseAdvances();

        // Lower in this function, synthetic bold is blanket-applied to everything, so no need to double-apply it here.
        float spaceWidth = font.spaceWidth(Font::SyntheticBoldInclusion::Exclude);
        auto charactersSpan = complexTextRun.characters();
        FloatPoint glyphOrigin;
        unsigned previousCharacterIndex = m_run->ltr() ? std::numeric_limits<unsigned>::min() : std::numeric_limits<unsigned>::max();
        bool isMonotonic = true;

#if USE(CORE_TEXT)
        auto boundsForGlyphs = font.boundsForGlyphs(glyphs);
#endif

        for (unsigned glyphIndex = 0; glyphIndex < glyphCount; glyphIndex++) {
            unsigned characterIndex = complexTextRun.indexAt(glyphIndex);
            if (m_run->ltr()) {
                if (characterIndex < previousCharacterIndex)
                    isMonotonic = false;
            } else {
                if (characterIndex > previousCharacterIndex)
                    isMonotonic = false;
            }
            char16_t character = charactersSpan[characterIndex];

            bool treatAsSpace = FontCascade::treatAsSpace(character);
            CGGlyph glyph = glyphs[glyphIndex];
            FloatSize advance = treatAsSpace ? FloatSize(spaceWidth, advances[glyphIndex].height()) : advances[glyphIndex];

            if (character == tabCharacter && m_run->allowTabs()) {
                advance.setWidth(m_fontCascade->tabWidth(font, m_run->tabSize(), m_run->xPos() + m_totalAdvance.width(), Font::SyntheticBoldInclusion::Exclude));
                // Like simple text path in WidthIterator::applyCSSVisibilityRules,
                // make tabCharacter glyph invisible after advancing.
                glyph = deletedGlyph;
#if USE(CORE_TEXT)
                boundsForGlyphs[glyphIndex] = font.boundsForGlyph(glyph);
#endif
            } else if (character == zeroWidthNonJoiner) {
                // zeroWidthNonJoiner is rendered as deletedGlyph for compatibility with other engines: https://bugs.webkit.org/show_bug.cgi?id=285959
                advance.setWidth(0);
                glyph = deletedGlyph;
#if USE(CORE_TEXT)
                boundsForGlyphs[glyphIndex] = font.boundsForGlyph(glyph);
#endif
            } else if (!treatAsSpace && FontCascade::treatAsZeroWidthSpace(character)) {
                advance.setWidth(0);
                glyph = font.spaceGlyph();
#if USE(CORE_TEXT)
                boundsForGlyphs[glyphIndex] = font.boundsForGlyph(glyph);
#endif
            }

            // https://www.w3.org/TR/css-text-3/#white-space-processing
            // "Control characters (Unicode category Cc)—other than tabs (U+0009), line feeds (U+000A), carriage returns (U+000D) and sequences that form a segment break—must be rendered as a visible glyph"
            // Also, we're omitting Null (U+0000) from this set because Chrome and Firefox do so and it's needed for compat. See https://github.com/w3c/csswg-drafts/pull/6983.
            if (character != newlineCharacter && character != carriageReturn && character != noBreakSpace && character != tabCharacter && character != nullCharacter && isControlCharacter(character)) {
                // Let's assume that .notdef is visible.
                glyph = 0;
#if USE(CORE_TEXT)
                boundsForGlyphs[glyphIndex] = font.boundsForGlyph(glyph);
#endif
                advance.setWidth(font.widthForGlyph(glyph));
            }

            if (!glyphIndex) {
                advance.expand(complexTextRun.initialAdvance().width(), complexTextRun.initialAdvance().height());
                if (auto origins = complexTextRun.glyphOrigins(); !origins.empty())
                    advance.expand(-origins[0].x(), -origins[0].y());
            }

            advance.expand(font.syntheticBoldOffset(), 0);

            if (hasExtraSpacing) {
                // If we're a glyph with an advance, add in letter-spacing.
                // That way we weed out zero width lurkers. This behavior matches the fast text code path.
                if (advance.width())
                    advance.expand(m_fontCascade->letterSpacing(), 0);

                unsigned characterIndexInRun = characterIndex + complexTextRun.stringLocation();
                bool isFirstCharacter = !(characterIndex + complexTextRun.stringLocation());
                bool isLastCharacter = characterIndexInRun + 1 == m_run->length()
                    || (U16_IS_LEAD(character) && characterIndexInRun + 2 == m_run->length() && characterIndex + 2 == charactersSpan.size() && U16_IS_TRAIL(charactersSpan[characterIndex + 1]));

                bool forceLeftExpansion = false; // On the left, regardless of m_run->ltr()
                bool forceRightExpansion = false; // On the right, regardless of m_run->ltr()
                bool forbidLeftExpansion = false;
                bool forbidRightExpansion = false;
                if (runForcesLeftExpansion)
                    forceLeftExpansion = m_run->ltr() ? isFirstCharacter : isLastCharacter;
                if (runForcesRightExpansion)
                    forceRightExpansion = m_run->ltr() ? isLastCharacter : isFirstCharacter;
                if (runForbidsLeftExpansion)
                    forbidLeftExpansion = m_run->ltr() ? isFirstCharacter : isLastCharacter;
                if (runForbidsRightExpansion)
                    forbidRightExpansion = m_run->ltr() ? isLastCharacter : isFirstCharacter;
                // Handle justification and word-spacing.
                bool ideograph = FontCascade::canExpandAroundIdeographsInComplexText() && FontCascade::isCJKIdeographOrSymbol(character);
                if (treatAsSpace || ideograph || forceLeftExpansion || forceRightExpansion) {
                    // Distribute the run's total expansion evenly over all expansion opportunities in the run.
                    if (m_expansion) {
                        auto [expandLeft, expandRight] = expansionLocation(ideograph, treatAsSpace, m_run->ltr(), afterExpansion, forbidLeftExpansion, forbidRightExpansion, forceLeftExpansion, forceRightExpansion);
                        if (expandLeft) {
                            m_expansion -= m_expansionPerOpportunity;
                            // Increase previous width
                            if (m_adjustedBaseAdvances.isEmpty()) {
                                advance.expand(m_expansionPerOpportunity, 0);
                                complexTextRun.growInitialAdvanceHorizontally(m_expansionPerOpportunity);
                            } else {
                                m_adjustedBaseAdvances.last().expand(m_expansionPerOpportunity, 0);
                                m_totalAdvance.expand(m_expansionPerOpportunity, 0);
                            }
                        }
                        if (expandRight) {
                            m_expansion -= m_expansionPerOpportunity;
                            advance.expand(m_expansionPerOpportunity, 0);
                            afterExpansion = true;
                        }
                    } else
                        afterExpansion = false;

                    // Account for word-spacing.
                    if (treatAsSpace && (character != '\t' || !m_run->allowTabs()) && (characterIndex > 0 || runIndex > 0 || character == noBreakSpace) && m_fontCascade->wordSpacing())
                        advance.expand(m_fontCascade->wordSpacing(), 0);
                } else
                    afterExpansion = false;
            }

            const auto& textAutoSpace =  m_fontCascade->textAutospace();
            float textAutoSpaceSpacing = 0;
            auto characterClass = TextSpacing::CharacterClass::Undefined;
            // Since we are iterating through glyphs here we skip combining marks, since we just care about the grapheme cluster base for text-autospace.
            if (!textAutoSpace.isNoAutospace() && !isCombiningMark(character)) {
                characterClass = TextSpacing::characterClass(character);
                if (textAutoSpace.shouldApplySpacing(previousCharacterClass, characterClass)) {
                    textAutoSpaceSpacing = complexTextRun.textAutospaceSize();
                    advance.expand(textAutoSpaceSpacing, 0);
                }

                previousCharacterClass = characterClass;
            }

            if (!textAutoSpace.isNoAutospace())
                m_textAutoSpaceSpacings.append(textAutoSpaceSpacing);

            m_totalAdvance += advance;

            if (m_forTextEmphasis) {
                char32_t ch32 = character;
                if (U16_IS_SURROGATE(character))
                    U16_GET(charactersSpan, 0, characterIndex, complexTextRun.stringLength(), ch32);
                // FIXME: Combining marks should receive a text emphasis mark if they are combine with a space.
                if (!FontCascade::canReceiveTextEmphasis(ch32) || (U_GET_GC_MASK(character) & U_GC_M_MASK)) {
                    glyph = deletedGlyph;
#if USE(CORE_TEXT)
                    boundsForGlyphs[glyphIndex] = font.boundsForGlyph(glyph);
#endif
                }
            }

            m_adjustedBaseAdvances.append(advance);
            if (auto origins = complexTextRun.glyphOrigins(); !origins.empty()) {
                ASSERT(m_glyphOrigins.size() < m_adjustedBaseAdvances.size());
                m_glyphOrigins.grow(m_adjustedBaseAdvances.size());
                m_glyphOrigins[m_glyphOrigins.size() - 1] = origins[glyphIndex] + FloatSize(textAutoSpaceSpacing, 0);
                ASSERT(m_glyphOrigins.size() == m_adjustedBaseAdvances.size());
            }
            m_adjustedGlyphs.append(glyph);

#if USE(CORE_TEXT)
            auto& glyphBounds = boundsForGlyphs[glyphIndex];
#else
            auto glyphBounds = font.boundsForGlyph(glyph);
#endif
            glyphBounds.move(glyphOrigin.x(), glyphOrigin.y());
            m_minGlyphBoundingBoxX = std::min(m_minGlyphBoundingBoxX, glyphBounds.x());
            m_maxGlyphBoundingBoxX = std::max(m_maxGlyphBoundingBoxX, glyphBounds.maxX());
            m_minGlyphBoundingBoxY = std::min(m_minGlyphBoundingBoxY, glyphBounds.y());
            m_maxGlyphBoundingBoxY = std::max(m_maxGlyphBoundingBoxY, glyphBounds.maxY());
            glyphOrigin.move(advance);

            previousCharacterIndex = characterIndex;
        }
        if (!isMonotonic)
            complexTextRun.setIsNonMonotonic();
    }
}

// Missing glyphs run constructor. Core Text will not generate a run of missing glyphs, instead falling back on
// glyphs from LastResort. We want to use the primary font's missing glyph in order to match the fast text code path.
ComplexTextController::ComplexTextRun::ComplexTextRun(const Font& font, std::span<const char16_t> characters, unsigned stringLocation, unsigned indexBegin, unsigned indexEnd, bool ltr)
    : m_font(font)
    , m_characters(characters)
    , m_indexBegin(indexBegin)
    , m_indexEnd(indexEnd)
    , m_stringLocation(stringLocation)
    , m_isLTR(ltr)
    , m_textAutospaceSize(TextAutospace::textAutospaceSize(font))
{
    auto runLengthInCodeUnits = m_indexEnd - m_indexBegin;
    m_coreTextIndices.reserveInitialCapacity(runLengthInCodeUnits);
    unsigned r = m_indexBegin;
    while (r < m_indexEnd) {
        auto currentIndex = r;
        char32_t character;
        U16_NEXT(m_characters, r, stringLength(), character);
        // https://drafts.csswg.org/css-text-3/#white-space-processing
        // "Unsupported Default_ignorable characters must be ignored for text rendering."
        if (!FontCascade::isCharacterWhoseGlyphsShouldBeDeletedForTextRendering(character))
            m_coreTextIndices.append(currentIndex);
    }
    m_glyphCount = m_coreTextIndices.size();
    if (!ltr) {
        for (unsigned r = 0, end = m_glyphCount - 1; r < m_glyphCount / 2; ++r, --end)
            std::swap(m_coreTextIndices[r], m_coreTextIndices[end]);
    }

    // Synthesize a run of missing glyphs.
    m_glyphs.fill(0, m_glyphCount);
    // Synthetic bold will be handled later in adjustGlyphsAndAdvances().
    m_baseAdvances.fill(FloatSize(m_font->widthForGlyph(0, Font::SyntheticBoldInclusion::Exclude), 0), m_glyphCount);
}

ComplexTextController::ComplexTextRun::ComplexTextRun(const Vector<FloatSize>& advances, const Vector<FloatPoint>& origins, const Vector<Glyph>& glyphs, const Vector<unsigned>& stringIndices, FloatSize initialAdvance, const Font& font, std::span<const char16_t> characters, unsigned stringLocation, unsigned indexBegin, unsigned indexEnd, bool ltr)
    : m_baseAdvances(advances)
    , m_glyphOrigins(origins)
    , m_glyphs(glyphs)
    , m_coreTextIndices(stringIndices)
    , m_initialAdvance(initialAdvance)
    , m_font(font)
    , m_characters(characters)
    , m_indexBegin(indexBegin)
    , m_indexEnd(indexEnd)
    , m_glyphCount(glyphs.size())
    , m_stringLocation(stringLocation)
    , m_isLTR(ltr)
    , m_textAutospaceSize(TextAutospace::textAutospaceSize(font))
{
}

} // namespace WebCore
