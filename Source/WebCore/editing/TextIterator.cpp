/*
 * Copyright (C) 2004-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2015-2018 Google Inc. All rights reserved.
 * Copyright (C) 2005 Alexey Proskuryakov.
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
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "TextIterator.h"

#include "BoundaryPointInlines.h"
#include "ComposedTreeIterator.h"
#include "Document.h"
#include "Editing.h"
#include "ElementInlines.h"
#include "ElementRareData.h"
#include "FontCascade.h"
#include "HTMLAttachmentElement.h"
#include "HTMLBodyElement.h"
#include "HTMLElement.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLLegendElement.h"
#include "HTMLMeterElement.h"
#include "HTMLNames.h"
#include "HTMLParagraphElement.h"
#include "HTMLProgressElement.h"
#include "HTMLSlotElement.h"
#include "HTMLTextAreaElement.h"
#include "HTMLTextFormControlElement.h"
#include "ImageOverlay.h"
#include "LocalFrame.h"
#include "NodeTraversal.h"
#include "Range.h"
#include "RenderBoxInlines.h"
#include "RenderElementInlines.h"
#include "RenderImage.h"
#include "RenderIterator.h"
#include "RenderObjectInlines.h"
#include "RenderTableCell.h"
#include "RenderTableRow.h"
#include "RenderTextControl.h"
#include "RenderTextFragment.h"
#include "ShadowRoot.h"
#include "TextBoundaries.h"
#include "TextControlInnerElements.h"
#include "TextPlaceholderElement.h"
#include "VisiblePosition.h"
#include "VisibleUnits.h"
#include <unicode/unorm2.h>
#include <wtf/Function.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/ParsingUtilities.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/TextBreakIterator.h>
#include <wtf/unicode/CharacterNames.h>
#include <wtf/unicode/icu/ICUHelpers.h>

#if !UCONFIG_NO_COLLATION
#include <unicode/usearch.h>
#include <wtf/text/TextBreakIteratorInternalICU.h>
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(TextIterator);

using namespace WTF::Unicode;
using namespace HTMLNames;

// Buffer that knows how to compare with a search target.
// Keeps enough of the previous text to be able to search in the future, but no more.
// Non-breaking spaces are always equal to normal spaces.
// Case folding is also done if the CaseInsensitive option is specified.
// Matches are further filtered if the AtWordStarts option is specified, although some
// matches inside a word are permitted if TreatMedialCapitalAsWordStart is specified as well.
class SearchBuffer {
    WTF_MAKE_NONCOPYABLE(SearchBuffer);
public:
    SearchBuffer(const String& target, FindOptions);
    ~SearchBuffer();

    // Returns number of characters appended; guaranteed to be in the range [1, length].
    size_t append(StringView);
    bool needsMoreContext() const;
    void prependContext(StringView);
    void reachedBreak();

    // Result is the size in characters of what was found.
    // And <startOffset> is the number of characters back to the start of what was found.
    size_t search(size_t& startOffset);
    bool atBreak() const;

#if !UCONFIG_NO_COLLATION

private:
    bool isBadMatch(const char16_t*, size_t length) const;
    bool isWordStartMatch(size_t start, size_t length) const;
    bool isWordEndMatch(size_t start, size_t length) const;

    const String m_target;
    const StringView::UpconvertedCharacters m_targetCharacters;
    FindOptions m_options;

    Vector<char16_t> m_buffer;
    size_t m_overlap;
    size_t m_prefixLength;
    bool m_atBreak;
    bool m_needsMoreContext;

    const bool m_targetRequiresKanaWorkaround;
    Vector<char16_t> m_normalizedTarget;
    mutable Vector<char16_t> m_normalizedMatch;

#else

private:
    void append(char16_t, bool isCharacterStart);
    size_t length() const;

    String m_target;
    FindOptions m_options;

    Vector<char16_t> m_buffer;
    Vector<bool> m_isCharacterStartBuffer;
    bool m_isBufferFull;
    size_t m_cursor;

#endif
};

// --------

constexpr unsigned bitsInWord = sizeof(unsigned) * 8;
constexpr unsigned bitInWordMask = bitsInWord - 1;

void BitStack::push(bool bit)
{
    unsigned index = m_size / bitsInWord;
    unsigned shift = m_size & bitInWordMask;
    if (!shift && index == m_words.size()) {
        m_words.grow(index + 1);
        m_words[index] = 0;
    }
    unsigned& word = m_words[index];
    unsigned mask = 1U << shift;
    if (bit)
        word |= mask;
    else
        word &= ~mask;
    ++m_size;
}

void BitStack::pop()
{
    if (m_size)
        --m_size;
}

bool BitStack::top() const
{
    if (!m_size)
        return false;
    unsigned shift = (m_size - 1) & bitInWordMask;
    unsigned index = (m_size - 1) / bitsInWord;
    return m_words[index] & (1U << shift);
}

// --------

// This function is like Range::pastLastNode, except for the fact that it can climb up out of shadow trees.
static RefPtr<Node> nextInPreOrderCrossingShadowBoundaries(Node& rangeEndContainer, int rangeEndOffset)
{
    if (rangeEndOffset >= 0 && !rangeEndContainer.isCharacterDataNode()) {
        if (RefPtr next = rangeEndContainer.traverseToChildAt(rangeEndOffset))
            return next;
    }
    for (RefPtr node = rangeEndContainer; node; node = node->parentOrShadowHostNode()) {
        if (RefPtr next = node->nextSibling())
            return next;
    }
    return nullptr;
}

static inline bool fullyClipsContents(const Node& node, TextIteratorBehaviors behaviors)
{
    CheckedPtr renderer = node.renderer();
    if (!renderer) {
        RefPtr element = dynamicDowncast<Element>(node);
        return element && !element->hasDisplayContents();
    }
    CheckedPtr box = dynamicDowncast<RenderBox>(*renderer);
    if (!box || !box->hasNonVisibleOverflow())
        return false;

    // Quirk to keep copy/paste in the CodeMirror editor version used in Jenkins working.
    if (is<HTMLTextAreaElement>(node))
        return box->size().isEmpty();

    if (behaviors.contains(TextIteratorBehavior::EntersSkippedContentRelevantToUser) && isSkippedContentRoot(*box)) {
        // This may reveal collapsed content to find-in-page, but it's uncommon (and highly redundant) to have computed block height 0px while applying c-v: hidden.
        return false;
    }

    return box->contentBoxSize().isEmpty();
}

static inline bool ignoresContainerClip(const Node& node)
{
    CheckedPtr renderer = node.renderer();
    if (!renderer || renderer->isRenderTextOrLineBreak())
        return false;
    return renderer->isOutOfFlowPositioned();
}

static void pushFullyClippedState(BitStack& stack, Node& node, TextIteratorBehaviors behaviors)
{
    // Push true if this node full clips its contents, or if a parent already has fully
    // clipped and this is not a node that ignores its container's clip.
    stack.push(fullyClipsContents(node, behaviors) || (stack.top() && !ignoresContainerClip(node)));
}

static void setUpFullyClippedStack(BitStack& stack, Node& node, TextIteratorBehaviors behaviors)
{
    // Put the nodes in a vector so we can iterate in reverse order.
    // FIXME: This (and TextIterator in general) should use ComposedTreeIterator.
    Vector<Ref<Node>, 100> ancestry;
    for (RefPtr parent = node.parentOrShadowHostNode(); parent; parent = parent->parentOrShadowHostNode())
        ancestry.append(*parent);

    // Call pushFullyClippedState on each node starting with the earliest ancestor.
    size_t size = ancestry.size();
    for (size_t i = 0; i < size; ++i)
        pushFullyClippedState(stack, ancestry[size - i - 1], behaviors);
    pushFullyClippedState(stack, node, behaviors);
}

static bool isClippedByFrameAncestor(const Document& document, TextIteratorBehaviors behaviors)
{
    if (!behaviors.contains(TextIteratorBehavior::ClipsToFrameAncestors))
        return false;

    for (RefPtr owner = document.ownerElement(); owner; owner = owner->document().ownerElement()) {
        BitStack ownerClipStack;
        setUpFullyClippedStack(ownerClipStack, *owner, behaviors);
        if (ownerClipStack.top())
            return true;
    }
    return false;
}

// FIXME: editingIgnoresContent and isRendererReplacedElement try to do the same job.
// It's not good to have both of them.
bool isRendererReplacedElement(RenderObject* renderer, TextIteratorBehaviors behaviors)
{
    if (!renderer)
        return false;
    
    bool isAttachment = false;
#if ENABLE(ATTACHMENT_ELEMENT)
    isAttachment = renderer->isRenderAttachment();
#endif
    if (renderer->isImage() || renderer->isRenderWidget() || renderer->isRenderMedia() || isAttachment)
        return true;

    if (RefPtr element = dynamicDowncast<Element>(renderer->node())) {
        if (is<HTMLFormControlElement>(*element) || is<HTMLLegendElement>(*element) || is<HTMLProgressElement>(*element) || element->hasTagName(meterTag))
            return true;
        if (equalLettersIgnoringASCIICase(element->attributeWithoutSynchronization(roleAttr), "img"_s))
            return true;
#if USE(ATSPI)
        // Links are also replaced with object replacement character in ATSPI.
        if (behaviors.contains(TextIteratorBehavior::EmitsObjectReplacementCharacters) && element->isLink())
            return true;
#else
        UNUSED_PARAM(behaviors);
#endif
    }

    return false;
}

// --------

inline void TextIteratorCopyableText::reset()
{
    m_singleCharacter = 0;
    m_string = String();
    m_offset = 0;
    m_length = 0;
}

inline void TextIteratorCopyableText::set(String&& string)
{
    m_singleCharacter = 0;
    m_string = WTFMove(string);
    m_offset = 0;
    m_length = m_string.length();
}

inline void TextIteratorCopyableText::set(String&& string, unsigned offset, unsigned length)
{
    ASSERT(offset < string.length());
    ASSERT(length);
    ASSERT(length <= string.length() - offset);

    m_singleCharacter = 0;
    m_string = WTFMove(string);
    m_offset = offset;
    m_length = length;
}

inline void TextIteratorCopyableText::set(char16_t singleCharacter)
{
    m_singleCharacter = singleCharacter;
    m_string = String();
    m_offset = 0;
    m_length = 0;
}

void TextIteratorCopyableText::appendToStringBuilder(StringBuilder& builder) const
{
    if (m_singleCharacter)
        builder.append(m_singleCharacter);
    else
        builder.appendSubstring(m_string, m_offset, m_length);
}

// --------

static Node* firstNode(const BoundaryPoint& point)
{
    if (point.container->isCharacterDataNode())
        return point.container.ptr();
    if (RefPtr child = point.container->traverseToChildAt(point.offset))
        return child.get();
    if (!point.offset)
        return point.container.ptr();
    return NodeTraversal::nextSkippingChildren(point.container);
}

TextIterator::TextIterator(const SimpleRange& range, TextIteratorBehaviors behaviors)
    : m_behaviors(behaviors)
{
    ASSERT(!m_behaviors.contains(TextIteratorBehavior::EmitsObjectReplacementCharacters) || !m_behaviors.contains(TextIteratorBehavior::EmitsObjectReplacementCharactersForImages));

    OptionSet<LayoutOptions> findInPageLayoutOptions;
    if (m_behaviors.contains(TextIteratorBehavior::EntersSkippedContentRelevantToUser))
        findInPageLayoutOptions.add({ LayoutOptions::TreatContentVisibilityAutoAsVisible, LayoutOptions::TreatRevealedWhenFoundAsVisible });
    range.start.protectedDocument()->updateLayoutIgnorePendingStylesheets(findInPageLayoutOptions);

    m_startContainer = range.start.container.ptr();
    m_startOffset = range.start.offset;
    m_endContainer = range.end.container.ptr();
    m_endOffset = range.end.offset;

    m_currentNode = firstNode(range.start);
    if (!m_currentNode)
        return;

    init();
}

void TextIterator::init()
{
    RefPtr currentNode = m_currentNode;
    if (isClippedByFrameAncestor(currentNode->protectedDocument(), m_behaviors))
        return;

    setUpFullyClippedStack(m_fullyClippedStack, *currentNode, m_behaviors);

    m_offset = currentNode == m_startContainer.get() ? m_startOffset : 0;

    m_pastEndNode = nextInPreOrderCrossingShadowBoundaries(*m_endContainer, m_endOffset);

    m_positionNode = currentNode.get();

    advance();
}

TextIterator::~TextIterator() = default;

// FIXME: Use ComposedTreeIterator instead. These functions are more expensive because they might do O(n) work.
static inline Node* firstChild(TextIteratorBehaviors options, Node& node)
{
    if (options.contains(TextIteratorBehavior::TraversesFlatTree)) [[unlikely]]
        return firstChildInComposedTreeIgnoringUserAgentShadow(node);
    return node.firstChild();
}

static inline Node* nextSibling(TextIteratorBehaviors options, Node& node)
{
    if (options.contains(TextIteratorBehavior::TraversesFlatTree)) [[unlikely]]
        return nextSiblingInComposedTreeIgnoringUserAgentShadow(node);
    return node.nextSibling();
}

static inline Node* nextNode(TextIteratorBehaviors options, Node& node)
{
    if (options.contains(TextIteratorBehavior::TraversesFlatTree)) [[unlikely]]
        return nextInComposedTreeIgnoringUserAgentShadow(node);
    return NodeTraversal::next(node);
}

static inline bool isDescendantOf(TextIteratorBehaviors options, Node& node, Node& possibleAncestor)
{
    if (options.contains(TextIteratorBehavior::TraversesFlatTree)) [[unlikely]]
        return node.isShadowIncludingDescendantOf(&possibleAncestor);
    return node.isDescendantOf(&possibleAncestor);
}

static inline Node* parentNodeOrShadowHost(TextIteratorBehaviors options, Node& node)
{
    if (options.contains(TextIteratorBehavior::TraversesFlatTree)) [[unlikely]]
        return node.parentInComposedTree();
    return node.parentOrShadowHostNode();
}

static inline bool hasDisplayContents(Node& node)
{
    RefPtr element = dynamicDowncast<Element>(node);
    return element && element->hasDisplayContents();
}

static bool isRendererAccessible(const RenderObject* renderer, TextIteratorBehaviors behaviors)
{
    if (!renderer)
        return false;

    auto& style = renderer->style();
    if (style.usedUserSelect() == UserSelect::None && behaviors.contains(TextIteratorBehavior::IgnoresUserSelectNone))
        return false;

    if (renderer->isSkippedContent()) {
        if (!behaviors.contains(TextIteratorBehavior::EntersSkippedContentRelevantToUser))
            return false;
        return style.usedContentVisibility() == ContentVisibility::Auto || style.autoRevealsWhenFound();
    }

    return true;
}

static bool isConsideredSkippedContent(const RenderBox* renderBox, TextIteratorBehaviors behaviors)
{
    if (!renderBox || !isSkippedContentRoot(*renderBox))
        return false;

    if (behaviors.contains(TextIteratorBehavior::EntersSkippedContentRelevantToUser))
        return renderBox->style().usedContentVisibility() == ContentVisibility::Hidden && !renderBox->style().autoRevealsWhenFound();

    return true;
}

void TextIterator::advance()
{
    ASSERT(!atEnd());

    // reset the run information
    m_positionNode = nullptr;
    m_copyableText.reset();
    m_text = StringView();

    // handle remembered node that needed a newline after the text node's newline
    if (RefPtr nodeForAdditionalNewline = std::exchange(m_nodeForAdditionalNewline, nullptr).get()) {
        // Emit the extra newline, and position it *inside* m_node, after m_node's 
        // contents, in case it's a block, in the same way that we position the first 
        // newline. The range for the emitted newline should start where the line
        // break begins.
        // FIXME: It would be cleaner if we emitted two newlines during the last 
        // iteration, instead of using m_needsAnotherNewline.
        RefPtr parentNode = nodeForAdditionalNewline->parentNode();
        emitCharacter('\n', WTFMove(parentNode), WTFMove(nodeForAdditionalNewline), 1, 1);
        return;
    }

    if (!m_textRun && m_remainingTextRun)
        revertToRemainingTextRun();

    // handle remembered text box
    if (m_textRun) {
        handleTextRun();
        if (m_positionNode)
            return;
    }

    while (m_currentNode && m_currentNode != m_pastEndNode) {
        // if the range ends at offset 0 of an element, represent the
        // position, but not the content, of that element e.g. if the
        // node is a blockflow element, emit a newline that
        // precedes the element
        if (m_currentNode == m_endContainer && !m_endOffset) {
            representNodeOffsetZero();
            m_currentNode = nullptr;
            return;
        }
        
        CheckedPtr renderer = m_currentNode->renderer();
        if (!m_handledNode) {
            if (!isRendererAccessible(renderer.get(), m_behaviors)) {
                m_handledNode = true;
                m_handledChildren = !hasDisplayContents(*protectedCurrentNode()) && !renderer;
            } else {
                if (isConsideredSkippedContent(dynamicDowncast<RenderBox>(renderer.get()), m_behaviors))
                    m_handledChildren = true;
                else if (renderer->isRenderText() && m_currentNode->isTextNode())
                    m_handledNode = handleTextNode();
                else if (isRendererReplacedElement(renderer.get(), m_behaviors))
                    m_handledNode = handleReplacedElement();
                else
                    m_handledNode = handleNonTextNode();
                if (m_positionNode)
                    return;
            }
        }

        // find a new current node to handle in depth-first manner,
        // calling exitNode() as we come back thru a parent node

        RefPtr next = m_handledChildren ? nullptr : firstChild(m_behaviors, *protectedCurrentNode());
        m_offset = 0;
        if (!next) {
            RefPtr currentNode = m_currentNode;
            next = nextSibling(m_behaviors, *currentNode);
            if (!next) {
                bool pastEnd = nextNode(m_behaviors, *currentNode) == m_pastEndNode;
                RefPtr parentNode = parentNodeOrShadowHost(m_behaviors, *currentNode);
                while (!next && parentNode) {
                    if ((pastEnd && parentNode == m_endContainer.get()) || isDescendantOf(m_behaviors, *m_endContainer, *parentNode))
                        return;
                    bool haveRenderer = isRendererAccessible(currentNode->renderer(), m_behaviors);
                    RefPtr exitedNode = WTFMove(currentNode);
                    m_currentNode = WTFMove(parentNode);
                    currentNode = m_currentNode;
                    m_fullyClippedStack.pop();
                    parentNode = parentNodeOrShadowHost(m_behaviors, *currentNode);
                    if (haveRenderer)
                        exitNode(exitedNode.get());
                    if (m_positionNode) {
                        m_handledNode = true;
                        m_handledChildren = true;
                        return;
                    }
                    next = nextSibling(m_behaviors, *currentNode);
                    if (next && isRendererAccessible(currentNode->renderer(), m_behaviors))
                        exitNode(currentNode.get());
                }
            }
            m_fullyClippedStack.pop();            
        }

        // set the new current node
        m_currentNode = WTFMove(next);
        if (RefPtr currentNode = m_currentNode)
            pushFullyClippedState(m_fullyClippedStack, *currentNode, m_behaviors);
        m_handledNode = false;
        m_handledChildren = false;
        m_handledFirstLetter = false;
        m_firstLetterText = nullptr;

        // how would this ever be?
        if (m_positionNode)
            return;
    }
}

static bool hasVisibleTextNode(RenderText& renderer)
{
    if (renderer.style().visibility() == Visibility::Visible)
        return true;
    if (CheckedPtr renderTextFragment = dynamicDowncast<RenderTextFragment>(renderer)) {
        if (auto firstLetter = renderTextFragment->firstLetter()) {
            if (firstLetter->style().visibility() == Visibility::Visible)
                return true;
        }
    }
    return false;
}

bool TextIterator::handleTextNode()
{
    Ref textNode = downcast<Text>(protectedCurrentNode().releaseNonNull());

    if (m_fullyClippedStack.top() && !m_behaviors.contains(TextIteratorBehavior::IgnoresStyleVisibility))
        return false;

    CheckedRef renderer = *textNode->renderer();
    m_lastTextNode = textNode.ptr();
    auto rendererText = rendererTextForBehavior(renderer.get());

    // handle pre-formatted text
    if (!renderer->style().collapseWhiteSpace()) {
        int runStart = m_offset;
        if (m_lastTextNodeEndedWithCollapsedSpace && hasVisibleTextNode(renderer)) {
            emitCharacter(' ', WTFMove(textNode), nullptr, runStart, runStart);
            return false;
        }
        if (CheckedPtr renderTextFragment = dynamicDowncast<RenderTextFragment>(renderer.get()); renderTextFragment && !m_handledFirstLetter && !m_offset) {
            handleTextNodeFirstLetter(*renderTextFragment);
            if (m_firstLetterText) {
                String firstLetter = m_firstLetterText->text();
                emitText(textNode, *m_firstLetterText, m_offset, m_offset + firstLetter.length());
                m_firstLetterText = nullptr;
                m_textRun = { };
                return false;
            }
        }
        if (renderer->style().visibility() != Visibility::Visible && !m_behaviors.contains(TextIteratorBehavior::IgnoresStyleVisibility))
            return false;
        int rendererTextLength = rendererText.length();
        int end = (textNode.ptr() == m_endContainer) ? m_endOffset : INT_MAX;
        int runEnd = std::min(rendererTextLength, end);

        if (runStart >= runEnd)
            return true;

        emitText(textNode, renderer, runStart, runEnd);
        return true;
    }

    std::tie(m_textRun, m_textRunLogicalOrderCache) = InlineIterator::firstTextBoxInLogicalOrderFor(renderer.get());

    if (CheckedPtr renderTextFragment = dynamicDowncast<RenderTextFragment>(renderer.get()); renderTextFragment && !m_handledFirstLetter && !m_offset)
        handleTextNodeFirstLetter(*renderTextFragment);
    else if (!m_textRun && rendererText.length()) {
        if (renderer->style().visibility() != Visibility::Visible && !m_behaviors.contains(TextIteratorBehavior::IgnoresStyleVisibility))
            return false;
        m_lastTextNodeEndedWithCollapsedSpace = true; // entire block is collapsed space
        return true;
    }

    handleTextRun();
    return true;
}

void TextIterator::handleTextRun()
{
    Ref textNode = downcast<Text>(protectedCurrentNode().releaseNonNull());

    CheckedRef renderer = m_firstLetterText ? *m_firstLetterText : *textNode->renderer();
    if (renderer->style().visibility() != Visibility::Visible && !m_behaviors.contains(TextIteratorBehavior::IgnoresStyleVisibility)) {
        m_textRun = { };
        return;
    }

    auto [firstTextRun, orderCache] = InlineIterator::firstTextBoxInLogicalOrderFor(renderer);

    auto rendererText = rendererTextForBehavior(renderer.get());
    unsigned rangeStart = m_offset;
    auto rangeEnd = std::optional<unsigned> { };
    if (textNode.ptr() == m_endContainer)
        rangeEnd = m_endOffset;

    while (m_textRun) {
        auto textRunStart = m_textRun->start();
        auto textRunEnd = textRunStart + m_textRun->length();

        auto runStart = std::max(textRunStart, rangeStart);
        auto runEnd = std::min(textRunEnd, rangeEnd.value_or(textRunEnd));

        // Check if we need to emit (previously) collapsed whitespace at the start of this run.
        auto isAfterRangeEnd = rangeEnd ? runStart > *rangeEnd : false;
        auto hasPrecedingCollapsedWhitespace = m_lastTextNodeEndedWithCollapsedSpace || (m_textRun == firstTextRun && textRunStart == runStart && runStart);
        auto shouldEmitWhitespace = !isAfterRangeEnd && hasPrecedingCollapsedWhitespace && m_lastCharacter && !renderer->style().isCollapsibleWhiteSpace(m_lastCharacter);
        if (shouldEmitWhitespace) {
            if (m_lastTextNode == textNode.ptr() && runStart && renderer->style().isCollapsibleWhiteSpace(rendererText[runStart - 1])) {
                unsigned spaceRunStart = runStart - 1;
                while (spaceRunStart && renderer->style().isCollapsibleWhiteSpace(rendererText[spaceRunStart - 1]))
                    --spaceRunStart;
                emitCharacter(' ', WTFMove(textNode), nullptr, spaceRunStart, spaceRunStart + 1);
            } else
                emitCharacter(' ', WTFMove(textNode), nullptr, runStart, runStart);
            return;
        }

        // Determine what the next text run will be, but don't advance yet
        auto nextTextRun = InlineIterator::nextTextBoxInLogicalOrder(m_textRun, m_textRunLogicalOrderCache);
        if (runStart < runEnd) {
            auto isNewlineOrTab = [&](char16_t character) {
                return character == '\n' || character == '\t';
            };
            // Handle either a single newline or tab character (which becomes a space),
            // or a run of characters that does not include newlines or tabs.
            // This effectively translates newlines and tabs to spaces without copying the text.
            if (isNewlineOrTab(rendererText[runStart])) {
                emitCharacter(' ', textNode.copyRef(), nullptr, runStart, runStart + 1);
                m_offset = runStart + 1;
            } else {
                auto subrunEnd = runStart + 1;
                for (; subrunEnd < runEnd; ++subrunEnd) {
                    if (isNewlineOrTab(rendererText[subrunEnd]))
                        break;
                }
                if (subrunEnd == runEnd && m_behaviors.contains(TextIteratorBehavior::BehavesAsIfNodesFollowing)) {
                    bool lastSpaceCollapsedByNextNonTextRun = !nextTextRun && rendererText.length() > subrunEnd && rendererText[subrunEnd] == ' ';
                    if (lastSpaceCollapsedByNextNonTextRun)
                        ++subrunEnd; // runEnd stopped before last space. Increment by one to restore the space.
                }
                m_offset = subrunEnd;
                emitText(textNode, renderer, runStart, subrunEnd);
            }

            // If we are doing a subrun that doesn't go to the end of the text box,
            // come back again to finish handling this text box; don't advance to the next one.
            if (static_cast<unsigned>(m_positionEndOffset) < textRunEnd)
                return;

            // Advance and return
            unsigned nextRunStart = nextTextRun ? nextTextRun->start() : rendererText.length();
            if (nextRunStart > runEnd)
                m_lastTextNodeEndedWithCollapsedSpace = true; // collapsed space between runs or at the end
            m_textRun = nextTextRun;
            return;
        }
        // Advance and continue
        m_textRun = nextTextRun;
    }
    if (!m_textRun && m_remainingTextRun) {
        revertToRemainingTextRun();
        handleTextRun();
    }
}

void TextIterator::revertToRemainingTextRun()
{
    ASSERT(!m_textRun && m_remainingTextRun);

    m_textRun = m_remainingTextRun;
    m_textRunLogicalOrderCache = std::exchange(m_remainingTextRunLogicalOrderCache, { });
    m_remainingTextRun = { };
    m_firstLetterText = { };
    m_offset = 0;
}

static inline RenderText* firstRenderTextInFirstLetter(RenderBoxModelObject* firstLetter)
{
    if (!firstLetter)
        return nullptr;

    // FIXME: Should this check descendent objects?
    return childrenOfType<RenderText>(*firstLetter).first();
}

void TextIterator::handleTextNodeFirstLetter(RenderTextFragment& renderer)
{
    if (CheckedPtr firstLetter = renderer.firstLetter()) {
        if (firstLetter->style().visibility() != Visibility::Visible && !m_behaviors.contains(TextIteratorBehavior::IgnoresStyleVisibility))
            return;
        if (CheckedPtr firstLetterText = firstRenderTextInFirstLetter(firstLetter.get())) {
            m_handledFirstLetter = true;
            m_remainingTextRun = m_textRun;
            m_remainingTextRunLogicalOrderCache = std::exchange(m_textRunLogicalOrderCache, { });
            std::tie(m_textRun, m_textRunLogicalOrderCache) = InlineIterator::firstTextBoxInLogicalOrderFor(*firstLetterText);
            m_firstLetterText = firstLetterText.get();
        }
    }
    m_handledFirstLetter = true;
}

bool TextIterator::handleReplacedElement()
{
    if (m_fullyClippedStack.top())
        return false;

    // Note that RenderInlines can get passed in as replaced elements.
    CheckedPtr renderer = dynamicDowncast<RenderElement>(m_currentNode->renderer());
    if (!renderer) {
        ASSERT_NOT_REACHED();
        return false;
    }

    if (renderer->style().visibility() != Visibility::Visible && !m_behaviors.contains(TextIteratorBehavior::IgnoresStyleVisibility))
        return false;

    if (m_lastTextNodeEndedWithCollapsedSpace) {
        emitCharacter(' ', m_lastTextNode->protectedParentNode(), m_lastTextNode.copyRef(), 1, 1);
        return false;
    }

    if (CheckedPtr renderTextControl = dynamicDowncast<RenderTextControl>(renderer.get()); renderTextControl && m_behaviors.contains(TextIteratorBehavior::EntersTextControls)) {
        if (auto innerTextElement = renderTextControl->textFormControlElement().innerTextElement()) {
            m_currentNode = innerTextElement->containingShadowRoot();
            pushFullyClippedState(m_fullyClippedStack, *protectedCurrentNode(), m_behaviors);
            m_offset = 0;
            return false;
        }
    }

    RefPtr currentElement = dynamicDowncast<HTMLElement>(m_currentNode.get());
    if (m_behaviors.contains(TextIteratorBehavior::EntersImageOverlays) && currentElement && ImageOverlay::hasOverlay(*currentElement)) {
        if (RefPtr shadowRoot = m_currentNode->shadowRoot()) {
            m_currentNode = WTFMove(shadowRoot);
            pushFullyClippedState(m_fullyClippedStack, *protectedCurrentNode(), m_behaviors);
            m_offset = 0;
            return false;
        }
        ASSERT_NOT_REACHED();
    }

    m_hasEmitted = true;

    auto shouldEmitObjectReplacementCharacter = [&] {
        if (m_behaviors.contains(TextIteratorBehavior::EmitsObjectReplacementCharacters))
            return true;

        if (m_behaviors.contains(TextIteratorBehavior::EmitsObjectReplacementCharactersForImages) && is<HTMLImageElement>(m_currentNode.get()))
            return true;

#if ENABLE(ATTACHMENT_ELEMENT)
        if (m_behaviors.contains(TextIteratorBehavior::EmitsObjectReplacementCharactersForAttachments) && is<HTMLAttachmentElement>(m_currentNode.get()))
            return true;
#endif

        return false;
    }();

    if (shouldEmitObjectReplacementCharacter) {
        emitCharacter(objectReplacementCharacter, m_currentNode->protectedParentNode(), protectedCurrentNode(), 0, 1);
        // Don't process subtrees for embedded objects. If the text there is required,
        // it must be explicitly asked by specifying a range falling inside its boundaries.
        m_handledChildren = true;
        return true;
    }

    if (m_behaviors.contains(TextIteratorBehavior::EmitsCharactersBetweenAllVisiblePositions)) {
        // We want replaced elements to behave like punctuation for boundary 
        // finding, and to simply take up space for the selection preservation 
        // code in moveParagraphs, so we use a comma.
        emitCharacter(',', m_currentNode->protectedParentNode(), protectedCurrentNode(), 0, 1);
        return true;
    }

    m_positionNode = m_currentNode->parentNode();
    m_positionOffsetBaseNode = m_currentNode;
    m_positionStartOffset = 0;
    m_positionEndOffset = 1;

    if (CheckedPtr renderImage = dynamicDowncast<RenderImage>(renderer.get()); renderImage && m_behaviors.contains(TextIteratorBehavior::EmitsImageAltText)) {
        auto altText = renderImage->altText();
        if (unsigned length = altText.length()) {
            m_lastCharacter = altText[length - 1];
            m_copyableText.set(WTFMove(altText));
            m_text = m_copyableText.text();
            return true;
        }
    }

    m_copyableText.reset();
    m_text = StringView();
    m_lastCharacter = 0;
    return true;
}

static bool shouldEmitTabBeforeNode(Node& node)
{
    CheckedPtr cell = dynamicDowncast<RenderTableCell>(node.renderer());
    
    // Table cells are delimited by tabs.
    if (!cell)
        return false;
    
    // Want a tab before every cell other than the first one.
    CheckedPtr table = cell->table();
    return table && (table->cellBefore(cell.get()) || table->cellAbove(cell.get()));
}

static bool shouldEmitNewlineForNode(Node* node, bool emitsOriginalText)
{
    CheckedPtr renderer = node->renderer();
    if (!(renderer ? renderer->isBR() : node->hasTagName(brTag)))
        return false;
    return emitsOriginalText || !(node->isInShadowTree() && is<HTMLInputElement>(*node->shadowHost()));
}

static bool hasHeaderTag(HTMLElement& element)
{
    return element.hasTagName(h1Tag)
        || element.hasTagName(h2Tag)
        || element.hasTagName(h3Tag)
        || element.hasTagName(h4Tag)
        || element.hasTagName(h5Tag)
        || element.hasTagName(h6Tag);
}

static bool shouldEmitReplacementInsteadOfNode(const Node& node)
{
    // Placeholders should eventually disappear, so treating them as a line break doesn't make sense
    // as when they are removed the text after it is combined with the text before it.
    return is<TextPlaceholderElement>(node);
}

bool shouldEmitNewlinesBeforeAndAfterNode(Node& node)
{
    // Block flow (versus inline flow) is represented by having
    // a newline both before and after the element.
    CheckedPtr renderer = node.renderer();
    if (!renderer) {
        if (hasDisplayContents(node))
            return false;
        RefPtr element = dynamicDowncast<HTMLElement>(node);
        return element && (hasHeaderTag(*element)
            || element->hasTagName(blockquoteTag)
            || element->hasTagName(ddTag)
            || element->hasTagName(divTag)
            || element->hasTagName(dlTag)
            || element->hasTagName(dtTag)
            || element->hasTagName(hrTag)
            || element->hasTagName(liTag)
            || element->hasTagName(listingTag)
            || element->hasTagName(olTag)
            || element->hasTagName(pTag)
            || element->hasTagName(preTag)
            || element->hasTagName(trTag)
            || element->hasTagName(ulTag));
    }
    
    // Need to make an exception for table cells, because they are blocks, but we
    // want them tab-delimited rather than having newlines before and after.
    if (isTableCell(node))
        return false;
    
    // Need to make an exception for table row elements, because they are neither
    // "inline" or "RenderBlock", but we want newlines for them.
    if (CheckedPtr tableRow = dynamicDowncast<RenderTableRow>(*renderer)) {
        CheckedPtr table = tableRow->table();
        if (table && !table->isInline())
            return true;
    }

    if (shouldEmitReplacementInsteadOfNode(node))
        return false;

    return !renderer->isInline()
        && is<RenderBlock>(*renderer)
        && !renderer->isFloatingOrOutOfFlowPositioned()
        && !renderer->isBody();
}

static bool shouldEmitNewlineAfterNode(Node& node, bool emitsCharactersBetweenAllVisiblePositions = false)
{
    // FIXME: It should be better but slower to create a VisiblePosition here.
    if (!shouldEmitNewlinesBeforeAndAfterNode(node))
        return false;

    // Don't emit a new line at the end of the document unless we're matching the behavior of VisiblePosition.
    if (emitsCharactersBetweenAllVisiblePositions)
        return true;
    RefPtr subsequentNode = node;
    while ((subsequentNode = NodeTraversal::nextSkippingChildren(*subsequentNode))) {
        if (subsequentNode->renderer())
            return true;
    }
    return false;
}

static bool shouldEmitNewlineBeforeNode(Node& node)
{
    return shouldEmitNewlinesBeforeAndAfterNode(node); 
}

static bool shouldEmitExtraNewlineForNode(Node& node)
{
    // When there is a significant collapsed bottom margin, emit an extra
    // newline for a more realistic result. We end up getting the right
    // result even without margin collapsing. For example: <div><p>text</p></div>
    // will work right even if both the <div> and the <p> have bottom margins.

    CheckedPtr renderBox = dynamicDowncast<RenderBox>(node.renderer());
    if (!renderBox || !renderBox->height())
        return false;

    // NOTE: We only do this for a select set of nodes, and WinIE appears not to do this at all.
    RefPtr element = dynamicDowncast<HTMLElement>(node);
    if (!element || (!hasHeaderTag(*element) && !is<HTMLParagraphElement>(*element)))
        return false;

    auto bottomMargin = renderBox->collapsedMarginAfter();
    auto fontSize = renderBox->style().fontDescription().computedSize();
    return bottomMargin * 2 >= fontSize;
}

static int collapsedSpaceLength(RenderText& renderer, int textEnd)
{
    auto text = renderer.text();
    unsigned length = text.length();
    for (unsigned i = textEnd; i < length; ++i) {
        if (!renderer.style().isCollapsibleWhiteSpace(text[i]))
            return i - textEnd;
    }
    return length - textEnd;
}

static int maxOffsetIncludingCollapsedSpaces(Node& node)
{
    int offset = caretMaxOffset(node);
    if (CheckedPtr renderText = dynamicDowncast<RenderText>(node.renderer()))
        offset += collapsedSpaceLength(*renderText, offset);
    return offset;
}

// Whether or not we should emit a character as we enter m_currentNode (if it's a container) or as we hit it (if it's atomic).
bool TextIterator::shouldRepresentNodeOffsetZero()
{
    if (m_behaviors.contains(TextIteratorBehavior::EmitsCharactersBetweenAllVisiblePositions)) {
        if (CheckedPtr renderer = m_currentNode->renderer(); renderer && renderer->isRenderTable())
            return true;
    }

    // Leave element positioned flush with start of a paragraph
    // (e.g. do not insert tab before a table cell at the start of a paragraph)
    if (m_lastCharacter == '\n')
        return false;
    
    // Otherwise, show the position if we have emitted any characters
    if (m_hasEmitted)
        return true;
    
    // We've not emitted anything yet. Generally, there is no need for any positioning then.
    // The only exception is when the element is visually not in the same line as
    // the start of the range (e.g. the range starts at the end of the previous paragraph).
    // NOTE: Creating VisiblePositions and comparing them is relatively expensive, so we
    // make quicker checks to possibly avoid that. Another check that we could make is
    // is whether the inline vs block flow changed since the previous visible element.
    // I think we're already in a special enough case that that won't be needed, tho.

    // No character needed if this is the first node in the range.
    if (m_currentNode == m_startContainer)
        return false;
    
    // If we are outside the start container's subtree, assume we need to emit.
    // FIXME: m_startContainer could be an inline block
    Ref currentNode = *m_currentNode;
    if (!currentNode->isDescendantOf(m_startContainer.get()))
        return true;

    // If we started as m_startContainer offset 0 and the current node is a descendant of
    // the start container, we already had enough context to correctly decide whether to
    // emit after a preceding block. We chose not to emit (m_hasEmitted is false),
    // so don't second guess that now.
    // NOTE: Is this really correct when m_currentNode is not a leftmost descendant? Probably
    // immaterial since we likely would have already emitted something by now.
    if (m_startOffset == 0)
        return false;
        
    // If this node is unrendered or invisible the VisiblePosition checks below won't have much meaning.
    // Additionally, if the range we are iterating over contains huge sections of unrendered content, 
    // we would create VisiblePositions on every call to this function without this check.
    if (!currentNode->renderer() || currentNode->renderer()->style().visibility() != Visibility::Visible)
        return false;

    if (CheckedPtr renderBlockFlow = dynamicDowncast<RenderBlockFlow>(*currentNode->renderer())) {
        if (!renderBlockFlow->height() && !is<HTMLBodyElement>(currentNode))
            return false;
    }

    // The startPos.isNotNull() check is needed because the start could be before the body,
    // and in that case we'll get null. We don't want to put in newlines at the start in that case.
    // The currPos.isNotNull() check is needed because positions in non-HTML content
    // (like SVG) do not have visible positions, and we don't want to emit for them either.
    VisiblePosition startPos = VisiblePosition(Position(protectedStartContainer(), m_startOffset, Position::PositionIsOffsetInAnchor));
    VisiblePosition currPos = VisiblePosition(positionBeforeNode(currentNode.ptr()));
    return startPos.isNotNull() && currPos.isNotNull() && !inSameLine(startPos, currPos);
}

bool TextIterator::shouldEmitSpaceBeforeAndAfterNode(Node& node)
{
    return node.renderer() && node.renderer()->isRenderTable() && (node.renderer()->isInline() || m_behaviors.contains(TextIteratorBehavior::EmitsCharactersBetweenAllVisiblePositions));
}

void TextIterator::representNodeOffsetZero()
{
    // Emit a character to show the positioning of m_currentNode.
    
    // When we haven't been emitting any characters, shouldRepresentNodeOffsetZero() can 
    // create VisiblePositions, which is expensive. So, we perform the inexpensive checks
    // on m_currentNode to see if it necessitates emitting a character first and will early return
    // before encountering shouldRepresentNodeOffsetZero()s worse case behavior.
    RefPtr currentNode = m_currentNode;
    if (shouldEmitTabBeforeNode(*currentNode)) {
        if (shouldRepresentNodeOffsetZero()) {
            RefPtr parentNode = currentNode->parentNode();
            emitCharacter('\t', WTFMove(parentNode), WTFMove(currentNode), 0, 0);
        }
    } else if (shouldEmitNewlineBeforeNode(*currentNode)) {
        if (shouldRepresentNodeOffsetZero()) {
            RefPtr parentNode = currentNode->parentNode();
            emitCharacter('\n', WTFMove(parentNode), WTFMove(currentNode), 0, 0);
        }
    } else if (shouldEmitSpaceBeforeAndAfterNode(*currentNode)) {
        if (shouldRepresentNodeOffsetZero()) {
            RefPtr parentNode = currentNode->parentNode();
            emitCharacter(' ', WTFMove(parentNode), WTFMove(currentNode), 0, 0);
        }
    } else if (shouldEmitReplacementInsteadOfNode(*currentNode)) {
        if (shouldRepresentNodeOffsetZero()) {
            RefPtr parentNode = currentNode->parentNode();
            emitCharacter(objectReplacementCharacter, WTFMove(parentNode), WTFMove(currentNode), 0, 0);
        }
    }
}

bool TextIterator::handleNonTextNode()
{
    RefPtr currentNode = m_currentNode;
    if (shouldEmitNewlineForNode(currentNode.get(), m_behaviors.contains(TextIteratorBehavior::EmitsOriginalText))) {
        RefPtr parentNode = currentNode->parentNode();
        emitCharacter('\n', WTFMove(parentNode), WTFMove(currentNode), 0, 1);
    } else if (m_behaviors.contains(TextIteratorBehavior::EmitsCharactersBetweenAllVisiblePositions) && currentNode->renderer() && currentNode->renderer()->isHR()) {
        RefPtr parentNode = currentNode->parentNode();
        emitCharacter(' ', WTFMove(parentNode), WTFMove(currentNode), 0, 1);
    } else
        representNodeOffsetZero();

    return true;
}

void TextIterator::exitNode(Node* exitedNode)
{
    // prevent emitting a newline when exiting a collapsed block at beginning of the range
    // FIXME: !m_hasEmitted does not necessarily mean there was a collapsed block... it could
    // have been an hr (e.g.). Also, a collapsed block could have height (e.g. a table) and
    // therefore look like a blank line.
    if (!m_hasEmitted)
        return;
        
    // Emit with a position *inside* m_currentNode, after m_currentNode's contents, in
    // case it is a block, because the run should start where the 
    // emitted character is positioned visually.
    RefPtr baseNode = exitedNode;
    // FIXME: This shouldn't require the m_lastTextNode to be true, but we can't change that without making
    // the logic in _web_attributedStringFromRange match. We'll get that for free when we switch to use
    // TextIterator in _web_attributedStringFromRange.
    // See <rdar://problem/5428427> for an example of how this mismatch will cause problems.
    if (m_lastTextNode && shouldEmitNewlineAfterNode(*protectedCurrentNode(), m_behaviors.contains(TextIteratorBehavior::EmitsCharactersBetweenAllVisiblePositions))) {
        // use extra newline to represent margin bottom, as needed
        bool addNewline = shouldEmitExtraNewlineForNode(*protectedCurrentNode());
        
        // FIXME: We need to emit a '\n' as we leave an empty block(s) that
        // contain a VisiblePosition when doing selection preservation.
        if (m_lastCharacter != '\n') {
            // insert a newline with a position following this block's contents.
            emitCharacter('\n', baseNode->protectedParentNode(), baseNode.copyRef(), 1, 1);
            // remember whether to later add a newline for the current node
            ASSERT(!m_nodeForAdditionalNewline);
            if (addNewline)
                m_nodeForAdditionalNewline = baseNode.get();
        } else if (addNewline)
            // insert a newline with a position following this block's contents.
            emitCharacter('\n', baseNode->protectedParentNode(), baseNode.copyRef(), 1, 1);
    }
    
    // If nothing was emitted, see if we need to emit a space.
    if (!m_positionNode && shouldEmitSpaceBeforeAndAfterNode(*protectedCurrentNode())) {
        RefPtr parentNode = baseNode->parentNode();
        emitCharacter(' ', WTFMove(parentNode), WTFMove(baseNode), 1, 1);
    }
}

void TextIterator::emitCharacter(char16_t character, RefPtr<Node>&& characterNode, RefPtr<Node>&& offsetBaseNode, int textStartOffset, int textEndOffset)
{
    ASSERT(characterNode);
    m_hasEmitted = true;
    
    // remember information with which to construct the TextIterator::range()
    m_positionNode = WTFMove(characterNode);
    m_positionOffsetBaseNode = WTFMove(offsetBaseNode);
    m_positionStartOffset = textStartOffset;
    m_positionEndOffset = textEndOffset;

    m_copyableText.set(character);
    m_text = m_copyableText.text();
    m_lastCharacter = character;
    m_lastTextNodeEndedWithCollapsedSpace = false;
}

void TextIterator::emitText(Text& textNode, RenderText& renderer, int textStartOffset, int textEndOffset)
{
    ASSERT(textStartOffset >= 0);
    ASSERT(textEndOffset >= 0);
    ASSERT(textStartOffset <= textEndOffset);

    bool shouldIgnoreFullSizeKana = m_behaviors.contains(TextIteratorBehavior::IgnoresFullSizeKana) && renderer.style().textTransform().contains(TextTransform::FullSizeKana);

    // FIXME: This probably yields the wrong offsets when text-transform: lowercase turns a single character into two characters.
    String string = m_behaviors.contains(TextIteratorBehavior::EmitsOriginalText) || shouldIgnoreFullSizeKana ? renderer.originalText()
        : (m_behaviors.contains(TextIteratorBehavior::EmitsTextsWithoutTranscoding) ? renderer.textWithoutConvertingBackslashToYenSymbol() : renderer.text());

    ASSERT(m_behaviors.contains(TextIteratorBehavior::EmitsOriginalText) || string.length() >= static_cast<unsigned>(textEndOffset));

    textEndOffset = std::min(string.length(), static_cast<unsigned>(textEndOffset));

    m_positionNode = textNode;
    m_positionOffsetBaseNode = nullptr;
    m_positionStartOffset = textStartOffset;
    m_positionEndOffset = textEndOffset;

    m_lastCharacter = string[textEndOffset - 1];
    m_copyableText.set(WTFMove(string), textStartOffset, textEndOffset - textStartOffset);
    m_text = m_copyableText.text();

    m_lastTextNodeEndedWithCollapsedSpace = false;
    m_hasEmitted = true;
}

SimpleRange TextIterator::range() const
{
    ASSERT(!atEnd());
    // Use the current run information, if we have it.
    if (m_positionOffsetBaseNode) {
        unsigned index = m_positionOffsetBaseNode->computeNodeIndex();
        m_positionStartOffset += index;
        m_positionEndOffset += index;
        m_positionOffsetBaseNode = nullptr;
    }
    return { { *m_positionNode, static_cast<unsigned>(m_positionStartOffset) }, { *m_positionNode, static_cast<unsigned>(m_positionEndOffset) } };
}

Node* TextIterator::node() const
{
    auto start = this->range().start;
    if (start.container->isCharacterDataNode())
        return start.container.ptr();
    return start.container->traverseToChildAt(start.offset);
}

RefPtr<Node> TextIterator::protectedCurrentNode() const
{
    return m_currentNode;
}

#if ENABLE(TREE_DEBUGGING)
void TextIterator::showTreeForThis() const
{
    if (m_currentNode)
        m_currentNode->showTreeForThis();
    fprintf(stderr, "offset: %d\n", m_offset);
}
#endif

// --------

SimplifiedBackwardsTextIterator::SimplifiedBackwardsTextIterator(const SimpleRange& range)
{
    range.start.protectedDocument()->updateLayoutIgnorePendingStylesheets();

    RefPtr startNode = range.start.container.ptr();
    RefPtr endNode = range.end.container.ptr();
    unsigned startOffset = range.start.offset;
    unsigned endOffset = range.end.offset;

    if (!startNode->isCharacterDataNode()) {
        if (startOffset < startNode->countChildNodes()) {
            startNode = startNode->traverseToChildAt(startOffset);
            startOffset = 0;
        }
    }
    if (!endNode->isCharacterDataNode()) {
        if (endOffset > 0 && endOffset <= endNode->countChildNodes()) {
            endNode = endNode->traverseToChildAt(endOffset - 1);
            endOffset = endNode->length();
        }
    }

    m_node = endNode;
    setUpFullyClippedStack(m_fullyClippedStack, *m_node, m_behaviors);
    m_offset = endOffset;
    m_handledNode = false;
    m_handledChildren = endOffset == 0;

    m_startContainer = WTFMove(startNode);
    m_startOffset = startOffset;
    m_endContainer = endNode;
    m_endOffset = endOffset;
    
    m_positionNode = WTFMove(endNode);

    m_lastTextNode = nullptr;
    m_lastCharacter = '\n';

    m_havePassedStartContainer = false;

    advance();
}

void SimplifiedBackwardsTextIterator::advance()
{
    ASSERT(!atEnd());

    m_positionNode = nullptr;
    m_copyableText.reset();
    m_text = StringView();

    while (m_node && !m_havePassedStartContainer) {
        // Don't handle node if we start iterating at [node, 0].
        if (!m_handledNode && !(m_node == m_endContainer && !m_endOffset)) {
            CheckedPtr renderer = m_node->renderer();
            if (auto* renderText = dynamicDowncast<RenderText>(renderer.get())) {
                if (renderText->style().visibility() == Visibility::Visible && m_offset > 0)
                    m_handledNode = handleTextNode();
            } else if (isRendererReplacedElement(renderer.get(), m_behaviors)) {
                if (downcast<RenderElement>(*renderer).style().visibility() == Visibility::Visible && m_offset > 0)
                    m_handledNode = handleReplacedElement();
            } else
                m_handledNode = handleNonTextNode();
            if (m_positionNode)
                return;
        }

        if (!m_handledChildren && m_node->hasChildNodes()) {
            m_node = m_node->lastChild();
            pushFullyClippedState(m_fullyClippedStack, *protectedNode(), m_behaviors);
        } else {
            // Exit empty containers as we pass over them or containers
            // where [container, 0] is where we started iterating.
            if (!m_handledNode && canHaveChildrenForEditing(*protectedNode()) && m_node->parentNode() && (!m_node->lastChild() || (m_node == m_endContainer && !m_endOffset))) {
                exitNode();
                if (m_positionNode) {
                    m_handledNode = true;
                    m_handledChildren = true;
                    return;
                }
            }

            // Exit all other containers.
            while (!m_node->previousSibling()) {
                if (!advanceRespectingRange(m_node->protectedParentOrShadowHostNode().get()))
                    break;
                m_fullyClippedStack.pop();
                exitNode();
                if (m_positionNode) {
                    m_handledNode = true;
                    m_handledChildren = true;
                    return;
                }
            }

            m_fullyClippedStack.pop();
            if (advanceRespectingRange(m_node->protectedPreviousSibling().get()))
                pushFullyClippedState(m_fullyClippedStack, *protectedNode(), m_behaviors);
            else
                m_node = nullptr;
        }

        // For the purpose of word boundary detection,
        // we should iterate all visible text and trailing (collapsed) whitespaces. 
        m_offset = m_node ? maxOffsetIncludingCollapsedSpaces(*protectedNode()) : 0;
        m_handledNode = false;
        m_handledChildren = false;
        
        if (m_positionNode)
            return;
    }
}

bool SimplifiedBackwardsTextIterator::handleTextNode()
{
    m_lastTextNode = downcast<Text>(m_node.get());

    int startOffset;
    int offsetInNode;
    CheckedPtr renderer = handleFirstLetter(startOffset, offsetInNode);
    if (!renderer)
        return true;

    String text = renderer->text();
    if (!renderer->hasRenderedText() && text.length())
        return true;

    if (startOffset + offsetInNode == m_offset) {
        ASSERT(!m_shouldHandleFirstLetter);
        return true;
    }

    m_positionEndOffset = m_offset;
    m_offset = startOffset + offsetInNode;
    m_positionNode = m_node;
    m_positionStartOffset = m_offset;

    ASSERT(m_positionStartOffset < m_positionEndOffset);
    ASSERT(m_positionStartOffset - offsetInNode >= 0);
    ASSERT(m_positionEndOffset - offsetInNode > 0);
    ASSERT(m_positionEndOffset - offsetInNode <= static_cast<int>(text.length()));

    m_lastCharacter = text[m_positionEndOffset - offsetInNode - 1];
    m_copyableText.set(WTFMove(text), m_positionStartOffset - offsetInNode, m_positionEndOffset - m_positionStartOffset);
    m_text = m_copyableText.text();

    return !m_shouldHandleFirstLetter;
}

RenderText* SimplifiedBackwardsTextIterator::handleFirstLetter(int& startOffset, int& offsetInNode)
{
    CheckedRef renderer = downcast<RenderText>(*m_node->renderer());
    startOffset = (m_node == m_startContainer) ? m_startOffset : 0;

    CheckedPtr fragment = dynamicDowncast<RenderTextFragment>(renderer.get());
    if (!fragment) {
        offsetInNode = 0;
        return renderer.ptr();
    }

    int offsetAfterFirstLetter = fragment->start();
    if (startOffset >= offsetAfterFirstLetter) {
        ASSERT(!m_shouldHandleFirstLetter);
        offsetInNode = offsetAfterFirstLetter;
        return renderer.ptr();
    }

    if (!m_shouldHandleFirstLetter && startOffset + offsetAfterFirstLetter < m_offset) {
        m_shouldHandleFirstLetter = true;
        offsetInNode = offsetAfterFirstLetter;
        return renderer.ptr();
    }

    m_shouldHandleFirstLetter = false;
    offsetInNode = 0;
    CheckedPtr firstLetterRenderer = firstRenderTextInFirstLetter(fragment->firstLetter());

    m_offset = firstLetterRenderer->caretMaxOffset();
    m_offset += collapsedSpaceLength(*firstLetterRenderer, m_offset);

    return firstLetterRenderer.get();
}

bool SimplifiedBackwardsTextIterator::handleReplacedElement()
{
    unsigned index = m_node->computeNodeIndex();
    // We want replaced elements to behave like punctuation for boundary 
    // finding, and to simply take up space for the selection preservation 
    // code in moveParagraphs, so we use a comma. Unconditionally emit
    // here because this iterator is only used for boundary finding.
    emitCharacter(',', m_node->protectedParentNode(), index, index + 1);
    return true;
}

bool SimplifiedBackwardsTextIterator::handleNonTextNode()
{
    RefPtr currentNode = m_node;
    if (shouldEmitTabBeforeNode(*currentNode)) {
        unsigned index = currentNode->computeNodeIndex();
        emitCharacter('\t', currentNode->protectedParentNode(), index + 1, index + 1);
    } else if (shouldEmitNewlineForNode(currentNode.get(), m_behaviors.contains(TextIteratorBehavior::EmitsOriginalText)) || shouldEmitNewlineAfterNode(*m_node)) {
        if (m_lastCharacter != '\n') {
            // Corresponds to the same check in TextIterator::exitNode.
            unsigned index = currentNode->computeNodeIndex();
            // The start of this emitted range is wrong. Ensuring correctness would require
            // VisiblePositions and so would be slow. previousBoundary expects this.
            emitCharacter('\n', currentNode->protectedParentNode(), index + 1, index + 1);
        }
    }
    return true;
}

void SimplifiedBackwardsTextIterator::exitNode()
{
    RefPtr node = m_node;
    if (shouldEmitTabBeforeNode(*node))
        emitCharacter('\t', WTFMove(node), 0, 0);
    else if (shouldEmitNewlineForNode(node.get(), m_behaviors.contains(TextIteratorBehavior::EmitsOriginalText)) || shouldEmitNewlineBeforeNode(*m_node)) {
        // The start of this emitted range is wrong. Ensuring correctness would require
        // VisiblePositions and so would be slow. previousBoundary expects this.
        emitCharacter('\n', WTFMove(node), 0, 0);
    }
}

void SimplifiedBackwardsTextIterator::emitCharacter(char16_t c, RefPtr<Node>&& node, int startOffset, int endOffset)
{
    ASSERT(node);
    m_positionNode = WTFMove(node);
    m_positionStartOffset = startOffset;
    m_positionEndOffset = endOffset;
    m_copyableText.set(c);
    m_text = m_copyableText.text();
    m_lastCharacter = c;
}

bool SimplifiedBackwardsTextIterator::advanceRespectingRange(Node* next)
{
    if (!next)
        return false;
    m_havePassedStartContainer |= m_node == m_startContainer;
    if (m_havePassedStartContainer)
        return false;
    m_node = next;
    return true;
}

SimpleRange SimplifiedBackwardsTextIterator::range() const
{
    ASSERT(!atEnd());

    Ref positionNode = *m_positionNode;
    return { { positionNode.copyRef(), static_cast<unsigned>(m_positionStartOffset) }, { positionNode.copyRef(), static_cast<unsigned>(m_positionEndOffset) } };
}

// --------

CharacterIterator::CharacterIterator(const SimpleRange& range, TextIteratorBehaviors behaviors)
    : m_underlyingIterator(range, behaviors)
{
    while (!atEnd() && !m_underlyingIterator.text().length())
        m_underlyingIterator.advance();
}

SimpleRange CharacterIterator::range() const
{
    SimpleRange range = m_underlyingIterator.range();
    if (!m_underlyingIterator.atEnd()) {
        if (m_underlyingIterator.text().length() <= 1)
            ASSERT(m_runOffset == 0);
        else {
            unsigned offset = range.startOffset() + m_runOffset;
            range = { { range.start.container.copyRef(), offset }, { range.start.container.copyRef(), offset + 1 } };
        }
    }
    return range;
}

void CharacterIterator::advance(int count)
{
    if (count <= 0) {
        ASSERT(count == 0);
        return;
    }
    
    m_atBreak = false;

    // easy if there is enough left in the current m_underlyingIterator run
    int remaining = m_underlyingIterator.text().length() - m_runOffset;
    if (count < remaining) {
        m_runOffset += count;
        m_offset += count;
        return;
    }

    // exhaust the current m_underlyingIterator run
    count -= remaining;
    m_offset += remaining;
    
    // move to a subsequent m_underlyingIterator run
    for (m_underlyingIterator.advance(); !atEnd(); m_underlyingIterator.advance()) {
        int runLength = m_underlyingIterator.text().length();
        if (!runLength)
            m_atBreak = true;
        else {
            // see whether this is m_underlyingIterator to use
            if (count < runLength) {
                m_runOffset = count;
                m_offset += count;
                return;
            }
            
            // exhaust this m_underlyingIterator run
            count -= runLength;
            m_offset += runLength;
        }
    }

    // ran to the end of the m_underlyingIterator... no more runs left
    m_atBreak = true;
    m_runOffset = 0;
}

BackwardsCharacterIterator::BackwardsCharacterIterator(const SimpleRange& range)
    : m_underlyingIterator(range)
{
    while (!atEnd() && !m_underlyingIterator.text().length())
        m_underlyingIterator.advance();
}

SimpleRange BackwardsCharacterIterator::range() const
{
    auto range = m_underlyingIterator.range();
    if (!m_underlyingIterator.atEnd()) {
        if (m_underlyingIterator.text().length() <= 1)
            ASSERT(m_runOffset == 0);
        else {
            unsigned offset = range.end.offset - m_runOffset;
            range = { { range.start.container.copyRef(), offset - 1 }, { range.start.container.copyRef(), offset } };
        }
    }
    return range;
}

void BackwardsCharacterIterator::advance(int count)
{
    if (count <= 0) {
        ASSERT(!count);
        return;
    }

    m_atBreak = false;

    int remaining = m_underlyingIterator.text().length() - m_runOffset;
    if (count < remaining) {
        m_runOffset += count;
        m_offset += count;
        return;
    }

    count -= remaining;
    m_offset += remaining;

    for (m_underlyingIterator.advance(); !atEnd(); m_underlyingIterator.advance()) {
        int runLength = m_underlyingIterator.text().length();
        if (runLength == 0)
            m_atBreak = true;
        else {
            if (count < runLength) {
                m_runOffset = count;
                m_offset += count;
                return;
            }
            
            count -= runLength;
            m_offset += runLength;
        }
    }

    m_atBreak = true;
    m_runOffset = 0;
}

// --------

WordAwareIterator::WordAwareIterator(const SimpleRange& range)
    : m_underlyingIterator(range)
{
    advance(); // get in position over the first chunk of text
}

// We're always in one of these modes:
// - The current chunk in the text iterator is our current chunk
//      (typically its a piece of whitespace, or text that ended with whitespace)
// - The previous chunk in the text iterator is our current chunk
//      (we looked ahead to the next chunk and found a word boundary)
// - We built up our own chunk of text from many chunks from the text iterator

// FIXME: Performance could be bad for huge spans next to each other that don't fall on word boundaries.

void WordAwareIterator::advance()
{
    m_previousText.reset();
    m_buffer.clear();

    // If last time we did a look-ahead, start with that looked-ahead chunk now
    if (!m_didLookAhead) {
        ASSERT(!m_underlyingIterator.atEnd());
        m_underlyingIterator.advance();
    }
    m_didLookAhead = false;

    // Go to next non-empty chunk 
    while (!m_underlyingIterator.atEnd() && !m_underlyingIterator.text().length())
        m_underlyingIterator.advance();
    if (m_underlyingIterator.atEnd())
        return;

    while (1) {
        // If this chunk ends in whitespace we can just use it as our chunk.
        if (deprecatedIsSpaceOrNewline(m_underlyingIterator.text()[m_underlyingIterator.text().length() - 1]))
            return;

        // If this is the first chunk that failed, save it in previousText before look ahead
        if (m_buffer.isEmpty())
            m_previousText = m_underlyingIterator.copyableText();

        // Look ahead to next chunk. If it is whitespace or a break, we can use the previous stuff
        m_underlyingIterator.advance();
        if (m_underlyingIterator.atEnd() || !m_underlyingIterator.text().length() || deprecatedIsSpaceOrNewline(m_underlyingIterator.text()[0])) {
            m_didLookAhead = true;
            return;
        }

        if (m_buffer.isEmpty()) {
            // Start gobbling chunks until we get to a suitable stopping point.
            append(m_buffer, m_previousText.text());
            m_previousText.reset();
        }
        append(m_buffer, m_underlyingIterator.text());
    }
}

StringView WordAwareIterator::text() const
{
    if (!m_buffer.isEmpty())
        return m_buffer.span();
    if (m_previousText.text().length())
        return m_previousText.text();
    return m_underlyingIterator.text();
}

// --------

static inline char16_t foldQuoteMarkAndReplaceNoBreakSpace(char16_t c)
{
    switch (c) {
    case hebrewPunctuationGershayim:
    case leftDoubleQuotationMark:
    case leftLowDoubleQuotationMark:
    case rightDoubleQuotationMark:
    case leftPointingDoubleAngleQuotationMark:
    case rightPointingDoubleAngleQuotationMark:
    case doubleHighReversed9QuotationMark:
    case doubleLowReversed9QuotationMark:
    case reversedDoublePrimeQuotationMark:
    case doublePrimeQuotationMark:
    case lowDoublePrimeQuotationMark:
    case fullwidthQuotationMark:
        return '"';
    case hebrewPunctuationGeresh:
    case leftSingleQuotationMark:
    case leftLowSingleQuotationMark:
    case rightSingleQuotationMark:
    case singleLow9QuotationMark:
    case singleLeftPointingAngleQuotationMark:
    case singleRightPointingAngleQuotationMark:
    case leftCornerBracket:
    case rightCornerBracket:
    case leftWhiteCornerBracket:
    case rightWhiteCornerBracket:
    case presentationFormForVerticalLeftCornerBracket:
    case presentationFormForVerticalRightCornerBracket:
    case presentationFormForVerticalLeftWhiteCornerBracket:
    case presentationFormForVerticalRightWhiteCornerBracket:
    case fullwidthApostrophe:
    case halfwidthLeftCornerBracket:
    case halfwidthRightCornerBracket:
        return '\'';
    case noBreakSpace:
        return ' ';
    default:
        return c;
    }
}

// FIXME: We'd like to tailor the searcher to fold quote marks for us instead
// of doing it in a separate replacement pass here, but ICU doesn't offer a way
// to add tailoring on top of the locale-specific tailoring as of this writing.
String foldQuoteMarks(const String& stringToFold)
{
    String result = makeStringByReplacingAll(stringToFold, hebrewPunctuationGeresh, '\'');
    result = makeStringByReplacingAll(result, hebrewPunctuationGershayim, '"');
    result = makeStringByReplacingAll(result, leftDoubleQuotationMark, '"');
    result = makeStringByReplacingAll(result, leftLowDoubleQuotationMark, '"');
    result = makeStringByReplacingAll(result, leftSingleQuotationMark, '\'');
    result = makeStringByReplacingAll(result, leftLowSingleQuotationMark, '\'');
    result = makeStringByReplacingAll(result, rightDoubleQuotationMark, '"');
    result = makeStringByReplacingAll(result, singleLow9QuotationMark, '\'');
    result = makeStringByReplacingAll(result, singleLeftPointingAngleQuotationMark, '\'');
    result = makeStringByReplacingAll(result, singleRightPointingAngleQuotationMark, '\'');
    result = makeStringByReplacingAll(result, leftCornerBracket, '\'');
    result = makeStringByReplacingAll(result, rightCornerBracket, '\'');
    result = makeStringByReplacingAll(result, leftWhiteCornerBracket, '\'');
    result = makeStringByReplacingAll(result, rightWhiteCornerBracket, '\'');
    result = makeStringByReplacingAll(result, presentationFormForVerticalLeftCornerBracket, '\'');
    result = makeStringByReplacingAll(result, presentationFormForVerticalRightCornerBracket, '\'');
    result = makeStringByReplacingAll(result, presentationFormForVerticalLeftWhiteCornerBracket, '\'');
    result = makeStringByReplacingAll(result, presentationFormForVerticalRightWhiteCornerBracket, '\'');
    result = makeStringByReplacingAll(result, fullwidthApostrophe, '\'');
    result = makeStringByReplacingAll(result, halfwidthLeftCornerBracket, '\'');
    result = makeStringByReplacingAll(result, halfwidthRightCornerBracket, '\'');
    result = makeStringByReplacingAll(result, leftPointingDoubleAngleQuotationMark, '"');
    result = makeStringByReplacingAll(result, rightPointingDoubleAngleQuotationMark, '"');
    result = makeStringByReplacingAll(result, doubleHighReversed9QuotationMark, '"');
    result = makeStringByReplacingAll(result, doubleLowReversed9QuotationMark, '"');
    result = makeStringByReplacingAll(result, reversedDoublePrimeQuotationMark, '"');
    result = makeStringByReplacingAll(result, doublePrimeQuotationMark, '"');
    result = makeStringByReplacingAll(result, lowDoublePrimeQuotationMark, '"');
    result = makeStringByReplacingAll(result, fullwidthQuotationMark, '"');
    return makeStringByReplacingAll(result, rightSingleQuotationMark, '\'');
}

#if !UCONFIG_NO_COLLATION

constexpr size_t minimumSearchBufferSize = 8192;

#ifndef NDEBUG
static bool searcherInUse;
#endif

static UStringSearch* createSearcher()
{
    // Provide a non-empty pattern and non-empty text so usearch_open will not fail,
    // but it doesn't matter exactly what it is, since we don't perform any searches
    // without setting both the pattern and the text.
    UErrorCode status = U_ZERO_ERROR;
    auto searchCollatorName = makeString(unsafeSpan(currentSearchLocaleID()), "@collation=search"_s);
    UStringSearch* searcher = usearch_open(&newlineCharacter, 1, &newlineCharacter, 1, searchCollatorName.utf8().data(), 0, &status);
    ASSERT(U_SUCCESS(status) || status == U_USING_FALLBACK_WARNING || status == U_USING_DEFAULT_WARNING);
    return searcher;
}

static UStringSearch* searcher()
{
    static UStringSearch* searcher = createSearcher();
    return searcher;
}

static inline void lockSearcher()
{
#ifndef NDEBUG
    ASSERT(!searcherInUse);
    searcherInUse = true;
#endif
}

static inline void unlockSearcher()
{
#ifndef NDEBUG
    ASSERT(searcherInUse);
    searcherInUse = false;
#endif
}

// ICU's search ignores the distinction between small kana letters and ones
// that are not small, and also characters that differ only in the voicing
// marks when considering only primary collation strength differences.
// This is not helpful for end users, since these differences make words
// distinct, so for our purposes we need these to be considered.
// The Unicode folks do not think the collation algorithm should be
// changed. To work around this, we would like to tailor the ICU searcher,
// but we can't get that to work yet. So instead, we check for cases where
// these differences occur, and skip those matches.

// We refer to the above technique as the "kana workaround". The next few
// functions are helper functinos for the kana workaround.

static inline bool isKanaLetter(char16_t character)
{
    // Hiragana letters.
    if (character >= 0x3041 && character <= 0x3096)
        return true;

    // Katakana letters.
    if (character >= 0x30A1 && character <= 0x30FA)
        return true;
    if (character >= 0x31F0 && character <= 0x31FF)
        return true;

    // Halfwidth katakana letters.
    if (character >= 0xFF66 && character <= 0xFF9D && character != 0xFF70)
        return true;

    return false;
}

static inline bool isSmallKanaLetter(char16_t character)
{
    ASSERT(isKanaLetter(character));

    switch (character) {
    case 0x3041: // HIRAGANA LETTER SMALL A
    case 0x3043: // HIRAGANA LETTER SMALL I
    case 0x3045: // HIRAGANA LETTER SMALL U
    case 0x3047: // HIRAGANA LETTER SMALL E
    case 0x3049: // HIRAGANA LETTER SMALL O
    case 0x3063: // HIRAGANA LETTER SMALL TU
    case 0x3083: // HIRAGANA LETTER SMALL YA
    case 0x3085: // HIRAGANA LETTER SMALL YU
    case 0x3087: // HIRAGANA LETTER SMALL YO
    case 0x308E: // HIRAGANA LETTER SMALL WA
    case 0x3095: // HIRAGANA LETTER SMALL KA
    case 0x3096: // HIRAGANA LETTER SMALL KE
    case 0x30A1: // KATAKANA LETTER SMALL A
    case 0x30A3: // KATAKANA LETTER SMALL I
    case 0x30A5: // KATAKANA LETTER SMALL U
    case 0x30A7: // KATAKANA LETTER SMALL E
    case 0x30A9: // KATAKANA LETTER SMALL O
    case 0x30C3: // KATAKANA LETTER SMALL TU
    case 0x30E3: // KATAKANA LETTER SMALL YA
    case 0x30E5: // KATAKANA LETTER SMALL YU
    case 0x30E7: // KATAKANA LETTER SMALL YO
    case 0x30EE: // KATAKANA LETTER SMALL WA
    case 0x30F5: // KATAKANA LETTER SMALL KA
    case 0x30F6: // KATAKANA LETTER SMALL KE
    case 0x31F0: // KATAKANA LETTER SMALL KU
    case 0x31F1: // KATAKANA LETTER SMALL SI
    case 0x31F2: // KATAKANA LETTER SMALL SU
    case 0x31F3: // KATAKANA LETTER SMALL TO
    case 0x31F4: // KATAKANA LETTER SMALL NU
    case 0x31F5: // KATAKANA LETTER SMALL HA
    case 0x31F6: // KATAKANA LETTER SMALL HI
    case 0x31F7: // KATAKANA LETTER SMALL HU
    case 0x31F8: // KATAKANA LETTER SMALL HE
    case 0x31F9: // KATAKANA LETTER SMALL HO
    case 0x31FA: // KATAKANA LETTER SMALL MU
    case 0x31FB: // KATAKANA LETTER SMALL RA
    case 0x31FC: // KATAKANA LETTER SMALL RI
    case 0x31FD: // KATAKANA LETTER SMALL RU
    case 0x31FE: // KATAKANA LETTER SMALL RE
    case 0x31FF: // KATAKANA LETTER SMALL RO
    case 0xFF67: // HALFWIDTH KATAKANA LETTER SMALL A
    case 0xFF68: // HALFWIDTH KATAKANA LETTER SMALL I
    case 0xFF69: // HALFWIDTH KATAKANA LETTER SMALL U
    case 0xFF6A: // HALFWIDTH KATAKANA LETTER SMALL E
    case 0xFF6B: // HALFWIDTH KATAKANA LETTER SMALL O
    case 0xFF6C: // HALFWIDTH KATAKANA LETTER SMALL YA
    case 0xFF6D: // HALFWIDTH KATAKANA LETTER SMALL YU
    case 0xFF6E: // HALFWIDTH KATAKANA LETTER SMALL YO
    case 0xFF6F: // HALFWIDTH KATAKANA LETTER SMALL TU
        return true;
    }
    return false;
}

enum VoicedSoundMarkType { NoVoicedSoundMark, VoicedSoundMark, SemiVoicedSoundMark };

static inline VoicedSoundMarkType composedVoicedSoundMark(char16_t character)
{
    ASSERT(isKanaLetter(character));

    switch (character) {
    case 0x304C: // HIRAGANA LETTER GA
    case 0x304E: // HIRAGANA LETTER GI
    case 0x3050: // HIRAGANA LETTER GU
    case 0x3052: // HIRAGANA LETTER GE
    case 0x3054: // HIRAGANA LETTER GO
    case 0x3056: // HIRAGANA LETTER ZA
    case 0x3058: // HIRAGANA LETTER ZI
    case 0x305A: // HIRAGANA LETTER ZU
    case 0x305C: // HIRAGANA LETTER ZE
    case 0x305E: // HIRAGANA LETTER ZO
    case 0x3060: // HIRAGANA LETTER DA
    case 0x3062: // HIRAGANA LETTER DI
    case 0x3065: // HIRAGANA LETTER DU
    case 0x3067: // HIRAGANA LETTER DE
    case 0x3069: // HIRAGANA LETTER DO
    case 0x3070: // HIRAGANA LETTER BA
    case 0x3073: // HIRAGANA LETTER BI
    case 0x3076: // HIRAGANA LETTER BU
    case 0x3079: // HIRAGANA LETTER BE
    case 0x307C: // HIRAGANA LETTER BO
    case 0x3094: // HIRAGANA LETTER VU
    case 0x30AC: // KATAKANA LETTER GA
    case 0x30AE: // KATAKANA LETTER GI
    case 0x30B0: // KATAKANA LETTER GU
    case 0x30B2: // KATAKANA LETTER GE
    case 0x30B4: // KATAKANA LETTER GO
    case 0x30B6: // KATAKANA LETTER ZA
    case 0x30B8: // KATAKANA LETTER ZI
    case 0x30BA: // KATAKANA LETTER ZU
    case 0x30BC: // KATAKANA LETTER ZE
    case 0x30BE: // KATAKANA LETTER ZO
    case 0x30C0: // KATAKANA LETTER DA
    case 0x30C2: // KATAKANA LETTER DI
    case 0x30C5: // KATAKANA LETTER DU
    case 0x30C7: // KATAKANA LETTER DE
    case 0x30C9: // KATAKANA LETTER DO
    case 0x30D0: // KATAKANA LETTER BA
    case 0x30D3: // KATAKANA LETTER BI
    case 0x30D6: // KATAKANA LETTER BU
    case 0x30D9: // KATAKANA LETTER BE
    case 0x30DC: // KATAKANA LETTER BO
    case 0x30F4: // KATAKANA LETTER VU
    case 0x30F7: // KATAKANA LETTER VA
    case 0x30F8: // KATAKANA LETTER VI
    case 0x30F9: // KATAKANA LETTER VE
    case 0x30FA: // KATAKANA LETTER VO
        return VoicedSoundMark;
    case 0x3071: // HIRAGANA LETTER PA
    case 0x3074: // HIRAGANA LETTER PI
    case 0x3077: // HIRAGANA LETTER PU
    case 0x307A: // HIRAGANA LETTER PE
    case 0x307D: // HIRAGANA LETTER PO
    case 0x30D1: // KATAKANA LETTER PA
    case 0x30D4: // KATAKANA LETTER PI
    case 0x30D7: // KATAKANA LETTER PU
    case 0x30DA: // KATAKANA LETTER PE
    case 0x30DD: // KATAKANA LETTER PO
        return SemiVoicedSoundMark;
    }
    return NoVoicedSoundMark;
}

static inline bool isCombiningVoicedSoundMark(char16_t character)
{
    switch (character) {
    case 0x3099: // COMBINING KATAKANA-HIRAGANA VOICED SOUND MARK
    case 0x309A: // COMBINING KATAKANA-HIRAGANA SEMI-VOICED SOUND MARK
        return true;
    }
    return false;
}

static inline bool containsKanaLetters(const String& pattern)
{
    if (pattern.is8Bit())
        return false;
    for (auto character : pattern.span16()) {
        if (isKanaLetter(character))
            return true;
    }
    return false;
}

static void normalizeCharacters(const char16_t* characters, unsigned length, Vector<char16_t>& buffer)
{
    UErrorCode status = U_ZERO_ERROR;
    auto* normalizer = unorm2_getNFCInstance(&status);
    ASSERT(U_SUCCESS(status));

    buffer.reserveCapacity(length);

    status = callBufferProducingFunction(unorm2_normalize, normalizer, characters, length, buffer);
    ASSERT(U_SUCCESS(status));
}

static bool isNonLatin1Separator(char32_t character)
{
    ASSERT_ARG(character, !isLatin1(character));

    return U_GET_GC_MASK(character) & (U_GC_S_MASK | U_GC_P_MASK | U_GC_Z_MASK | U_GC_CF_MASK);
}

static inline bool isSeparator(char32_t character)
{
    static constexpr std::array<bool, 256> latin1SeparatorTable {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // space ! " # $ % & ' ( ) * + , - . /
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, //                         : ; < = > ?
        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //   @
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, //                         [ \ ] ^ _
        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //   `
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, //                           { | } ~
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0
    };

    if (isLatin1(character))
        return latin1SeparatorTable[character];

    return isNonLatin1Separator(character);
}

inline SearchBuffer::SearchBuffer(const String& target, FindOptions options)
    : m_target(foldQuoteMarks(target))
    , m_targetCharacters(StringView(m_target).upconvertedCharacters())
    , m_options(options)
    , m_prefixLength(0)
    , m_atBreak(true)
    , m_needsMoreContext(options.contains(FindOption::AtWordStarts))
    , m_targetRequiresKanaWorkaround(containsKanaLetters(m_target))
{
    ASSERT(!m_target.isEmpty());

    size_t targetLength = m_target.length();
    m_buffer.reserveInitialCapacity(std::max(targetLength * 8, minimumSearchBufferSize));
    m_overlap = m_buffer.capacity() / 4;

    if (m_options.contains(FindOption::AtWordStarts) && targetLength) {
        char32_t targetFirstCharacter;
        U16_GET(m_target, 0, 0u, targetLength, targetFirstCharacter);
        // Characters in the separator category never really occur at the beginning of a word,
        // so if the target begins with such a character, we just ignore the AtWordStart option.
        if (isSeparator(targetFirstCharacter)) {
            m_options.remove(FindOption::AtWordStarts);
            m_needsMoreContext = false;
        }
    }

    // Grab the single global searcher.
    // If we ever have a reason to do more than once search buffer at once, we'll have
    // to move to multiple searchers.
    lockSearcher();

    UStringSearch* searcher = WebCore::searcher();
    UCollator* collator = usearch_getCollator(searcher);

    UCollationStrength strength;
    USearchAttributeValue comparator;
    if (m_options.contains(FindOption::CaseInsensitive)) {
        // Without loss of generality, have 'e' match {'e', 'E', 'é', 'É'} and 'é' match {'é', 'É'}.
        strength = UCOL_SECONDARY;
        comparator = USEARCH_PATTERN_BASE_WEIGHT_IS_WILDCARD;
    } else {
        // Without loss of generality, have 'e' match {'e'} and 'é' match {'é'}.
        strength = UCOL_TERTIARY;
        comparator = USEARCH_STANDARD_ELEMENT_COMPARISON;
    }
    if (ucol_getStrength(collator) != strength) {
        ucol_setStrength(collator, strength);
        usearch_reset(searcher);
    }

    UErrorCode status = U_ZERO_ERROR;
    usearch_setAttribute(searcher, USEARCH_ELEMENT_COMPARISON, comparator, &status);
    ASSERT(U_SUCCESS(status));

    usearch_setPattern(searcher, m_targetCharacters, targetLength, &status);
    ASSERT(U_SUCCESS(status));

    // The kana workaround requires a normalized copy of the target string.
    if (m_targetRequiresKanaWorkaround)
        normalizeCharacters(m_targetCharacters, targetLength, m_normalizedTarget);
}

inline SearchBuffer::~SearchBuffer()
{
    // Leave the static object pointing to a valid string.
    UErrorCode status = U_ZERO_ERROR;
    usearch_setPattern(WebCore::searcher(), &newlineCharacter, 1, &status);
    ASSERT(U_SUCCESS(status));
    usearch_setText(WebCore::searcher(), &newlineCharacter, 1, &status);
    ASSERT(U_SUCCESS(status));

    unlockSearcher();
}

inline size_t SearchBuffer::append(StringView text)
{
    ASSERT(text.length());

    if (m_atBreak) {
        m_buffer.shrink(0);
        m_prefixLength = 0;
        m_atBreak = false;
    } else if (m_buffer.size() == m_buffer.capacity()) {
        memcpySpan(m_buffer.mutableSpan(), m_buffer.span().last(m_overlap));
        m_prefixLength -= std::min(m_prefixLength, m_buffer.size() - m_overlap);
        m_buffer.shrink(m_overlap);
    }

    size_t oldLength = m_buffer.size();
    size_t usableLength = std::min<size_t>(m_buffer.capacity() - oldLength, text.length());
    ASSERT(usableLength);
    m_buffer.grow(oldLength + usableLength);
    for (unsigned i = 0; i < usableLength; ++i)
        m_buffer[oldLength + i] = foldQuoteMarkAndReplaceNoBreakSpace(text[i]);
    return usableLength;
}

inline bool SearchBuffer::needsMoreContext() const
{
    return m_needsMoreContext;
}

inline void SearchBuffer::prependContext(StringView text)
{
    ASSERT(m_needsMoreContext);
    ASSERT(m_prefixLength == m_buffer.size());

    if (!text.length())
        return;

    m_atBreak = false;

    size_t wordBoundaryContextStart = text.length();
    if (wordBoundaryContextStart) {
        U16_BACK_1(text, 0, wordBoundaryContextStart);
        wordBoundaryContextStart = startOfLastWordBoundaryContext(text.left(wordBoundaryContextStart));
    }

    size_t usableLength = std::min(m_buffer.capacity() - m_prefixLength, text.length() - wordBoundaryContextStart);
    WTF::append(m_buffer, text.substring(text.length() - usableLength, usableLength));
    m_prefixLength += usableLength;

    if (wordBoundaryContextStart || m_prefixLength == m_buffer.capacity())
        m_needsMoreContext = false;
}

inline bool SearchBuffer::atBreak() const
{
    return m_atBreak;
}

inline void SearchBuffer::reachedBreak()
{
    m_atBreak = true;
}

inline bool SearchBuffer::isBadMatch(const char16_t* match, size_t matchLength) const
{
    // This function implements the kana workaround. If usearch treats
    // it as a match, but we do not want to, then it's a "bad match".
    if (!m_targetRequiresKanaWorkaround)
        return false;

    // Normalize into a match buffer. We reuse a single buffer rather than
    // creating a new one each time.
    normalizeCharacters(match, matchLength, m_normalizedMatch);

    auto a = m_normalizedTarget.span();
    auto b = m_normalizedMatch.span();

    while (true) {
        // Skip runs of non-kana-letter characters. This is necessary so we can
        // correctly handle strings where the target and match have different-length
        // runs of characters that match, while still double checking the correctness
        // of matches of kana letters with other kana letters.
        skipUntil<isKanaLetter>(a);
        skipUntil<isKanaLetter>(b);

        // If we reached the end of either the target or the match, we should have
        // reached the end of both; both should have the same number of kana letters.
        if (a.empty() || b.empty()) {
            ASSERT(a.empty());
            ASSERT(b.empty());
            return false;
        }

        // Check for differences in the kana letter character itself.
        if (isSmallKanaLetter(a.front()) != isSmallKanaLetter(b.front()))
            return true;
        if (composedVoicedSoundMark(a.front()) != composedVoicedSoundMark(b.front()))
            return true;
        skip(a, 1);
        skip(b, 1);

        // Check for differences in combining voiced sound marks found after the letter.
        while (1) {
            if (!(!a.empty() && isCombiningVoicedSoundMark(a.front()))) {
                if (!b.empty() && isCombiningVoicedSoundMark(b.front()))
                    return true;
                break;
            }
            if (!(!b.empty() && isCombiningVoicedSoundMark(b.front())))
                return true;
            if (a.front() != b.front())
                return true;
            skip(a, 1);
            skip(b, 1);
        }
    }
}
    
inline bool SearchBuffer::isWordEndMatch(size_t start, size_t length) const
{
    ASSERT(length);
    ASSERT(m_options.contains(FindOption::AtWordEnds));

    // Start searching at the end of matched search, so that multiple word matches succeed.
    int endWord;
    findEndWordBoundary(m_buffer.span(), start + length - 1, &endWord);
    return static_cast<size_t>(endWord) == start + length;
}

inline bool SearchBuffer::isWordStartMatch(size_t start, size_t length) const
{
    ASSERT(m_options.contains(FindOption::AtWordStarts));

    if (!start)
        return true;

    int size = m_buffer.size();
    int offset = start;
    char32_t firstCharacter;
    auto buffer = m_buffer.span();
    U16_GET(buffer, 0, offset, size, firstCharacter);

    if (m_options.contains(FindOption::TreatMedialCapitalAsWordStart)) {
        char32_t previousCharacter;
        U16_PREV(buffer, 0, offset, previousCharacter);

        if (isSeparator(firstCharacter)) {
            // The start of a separator run is a word start (".org" in "webkit.org").
            if (!isSeparator(previousCharacter))
                return true;
        } else if (isASCIIUpper(firstCharacter)) {
            // The start of an uppercase run is a word start ("Kit" in "WebKit").
            if (!isASCIIUpper(previousCharacter))
                return true;
            // The last character of an uppercase run followed by a non-separator, non-digit
            // is a word start ("Request" in "XMLHTTPRequest").
            offset = start;
            U16_FWD_1(buffer, offset, size);
            char32_t nextCharacter = 0;
            if (offset < size)
                U16_GET(buffer, 0, offset, size, nextCharacter);
            if (!isASCIIUpper(nextCharacter) && !isASCIIDigit(nextCharacter) && !isSeparator(nextCharacter))
                return true;
        } else if (isASCIIDigit(firstCharacter)) {
            // The start of a digit run is a word start ("2" in "WebKit2").
            if (!isASCIIDigit(previousCharacter))
                return true;
        } else if (isSeparator(previousCharacter) || isASCIIDigit(previousCharacter)) {
            // The start of a non-separator, non-uppercase, non-digit run is a word start,
            // except after an uppercase. ("org" in "webkit.org", but not "ore" in "WebCore").
            return true;
        }
    }

    // Chinese and Japanese lack word boundary marks, and there is no clear agreement on what constitutes
    // a word, so treat the position before any CJK character as a word start.
    if (FontCascade::isCJKIdeographOrSymbol(firstCharacter))
        return true;

    size_t wordBreakSearchStart = start + length;
    while (wordBreakSearchStart > start)
        wordBreakSearchStart = findNextWordFromIndex(buffer, wordBreakSearchStart, false /* backwards */);
    return wordBreakSearchStart == start;
}

inline size_t SearchBuffer::search(size_t& start)
{
    size_t size = m_buffer.size();
    if (m_atBreak) {
        if (!size)
            return 0;
    } else {
        if (size != m_buffer.capacity())
            return 0;
    }

    UStringSearch* searcher = WebCore::searcher();

    UErrorCode status = U_ZERO_ERROR;
    usearch_setText(searcher, m_buffer.span().data(), size, &status);
    ASSERT(U_SUCCESS(status));

    usearch_setOffset(searcher, m_prefixLength, &status);
    ASSERT(U_SUCCESS(status));

    int matchStart = usearch_next(searcher, &status);
    ASSERT(U_SUCCESS(status));

nextMatch:
    if (!(matchStart >= 0 && static_cast<size_t>(matchStart) < size)) {
        ASSERT(matchStart == USEARCH_DONE);
        return 0;
    }

    // Matches that start in the overlap area are only tentative.
    // The same match may appear later, matching more characters,
    // possibly including a combining character that's not yet in the buffer.
    if (!m_atBreak && static_cast<size_t>(matchStart) >= size - m_overlap) {
        size_t overlap = m_overlap;
        if (m_options.contains(FindOption::AtWordStarts)) {
            // Ensure that there is sufficient context before matchStart the next time around for
            // determining if it is at a word boundary.
            unsigned wordBoundaryContextStart = matchStart;
            U16_BACK_1(m_buffer.span(), 0, wordBoundaryContextStart);
            wordBoundaryContextStart = startOfLastWordBoundaryContext(m_buffer.subspan(0, wordBoundaryContextStart));
            overlap = std::min(size - 1, std::max(overlap, size - wordBoundaryContextStart));
        }
        memcpySpan(m_buffer.mutableSpan(), m_buffer.span().last(overlap));
        m_prefixLength -= std::min(m_prefixLength, size - overlap);
        m_buffer.shrink(overlap);
        return 0;
    }

    size_t matchedLength = usearch_getMatchedLength(searcher);
    ASSERT_WITH_SECURITY_IMPLICATION(matchStart + matchedLength <= size);

    // If this match is "bad", move on to the next match.
    if (isBadMatch(m_buffer.subspan(matchStart).data(), matchedLength)
        || (m_options.contains(FindOption::AtWordStarts) && !isWordStartMatch(matchStart, matchedLength))
        || (m_options.contains(FindOption::AtWordEnds) && !isWordEndMatch(matchStart, matchedLength))) {
        matchStart = usearch_next(searcher, &status);
        ASSERT(U_SUCCESS(status));
        goto nextMatch;
    }

    size_t newSize = size - (matchStart + 1);
    memmoveSpan(m_buffer.mutableSpan(), m_buffer.subspan(matchStart + 1, newSize));
    m_prefixLength -= std::min<size_t>(m_prefixLength, matchStart + 1);
    m_buffer.shrink(newSize);

    start = size - matchStart;
    return matchedLength;
}

#else

inline SearchBuffer::SearchBuffer(const String& target, FindOptions options)
    : m_target(foldQuoteMarks(options & CaseInsensitive ? target.foldCase() : target))
    , m_options(options)
    , m_buffer(m_target.length())
    , m_isCharacterStartBuffer(m_target.length())
    , m_isBufferFull(false)
    , m_cursor(0)
{
    ASSERT(!m_target.isEmpty());
    m_target.replace(noBreakSpace, ' ');
}

inline SearchBuffer::~SearchBuffer() = default;

inline void SearchBuffer::reachedBreak()
{
    m_cursor = 0;
    m_isBufferFull = false;
}

inline bool SearchBuffer::atBreak() const
{
    return !m_cursor && !m_isBufferFull;
}

inline void SearchBuffer::append(char16_t c, bool isStart)
{
    m_buffer[m_cursor] = foldQuoteMarkAndReplaceNoBreakSpace(c);
    m_isCharacterStartBuffer[m_cursor] = isStart;
    if (++m_cursor == m_target.length()) {
        m_cursor = 0;
        m_isBufferFull = true;
    }
}

inline size_t SearchBuffer::append(const char16_t* characters, size_t length)
{
    ASSERT(length);
    if (!(m_options & CaseInsensitive)) {
        append(characters[0], true);
        return 1;
    }
    constexpr int maxFoldedCharacters = 16; // sensible maximum is 3, this should be more than enough
    char16_t foldedCharacters[maxFoldedCharacters];
    UErrorCode status = U_ZERO_ERROR;
    int numFoldedCharacters = u_strFoldCase(foldedCharacters, maxFoldedCharacters, characters, 1, U_FOLD_CASE_DEFAULT, &status);
    ASSERT(U_SUCCESS(status));
    ASSERT(numFoldedCharacters);
    ASSERT(numFoldedCharacters <= maxFoldedCharacters);
    if (U_SUCCESS(status) && numFoldedCharacters) {
        numFoldedCharacters = std::min(numFoldedCharacters, maxFoldedCharacters);
        append(foldedCharacters[0], true);
        for (int i = 1; i < numFoldedCharacters; ++i)
            append(foldedCharacters[i], false);
    }
    return 1;
}

inline bool SearchBuffer::needsMoreContext() const
{
    return false;
}

void SearchBuffer::prependContext(const char16_t*, size_t)
{
    ASSERT_NOT_REACHED();
}

inline size_t SearchBuffer::search(size_t& start)
{
    if (!m_isBufferFull)
        return 0;
    if (!m_isCharacterStartBuffer[m_cursor])
        return 0;

    size_t tailSpace = m_target.length() - m_cursor;
    if (memcmp(&m_buffer[m_cursor], m_target.characters(), tailSpace * sizeof(char16_t)) != 0)
        return 0;
    if (memcmp(&m_buffer[0], m_target.characters() + tailSpace, m_cursor * sizeof(char16_t)) != 0)
        return 0;

    start = length();

    // Now that we've found a match once, we don't want to find it again, because those
    // are the SearchBuffer semantics, allowing for a buffer where you append more than one
    // character at a time. To do this we take advantage of m_isCharacterStartBuffer, but if
    // we want to get rid of that in the future we could track this with a separate boolean
    // or even move the characters to the start of the buffer and set m_isBufferFull to false.
    m_isCharacterStartBuffer[m_cursor] = false;

    return start;
}

// Returns the number of characters that were appended to the buffer (what we are searching in).
// That's not necessarily the same length as the passed-in target string, because case folding
// can make two strings match even though they're not the same length.
size_t SearchBuffer::length() const
{
    size_t bufferSize = m_target.length();
    size_t length = 0;
    for (size_t i = 0; i < bufferSize; ++i)
        length += m_isCharacterStartBuffer[i];
    return length;
}

#endif

// --------

uint64_t characterCount(const SimpleRange& range, TextIteratorBehaviors behaviors)
{
    auto adjustedRange = range;
    auto ordering = treeOrder<ComposedTree>(range.start, range.end);
    if (is_gt(ordering))
        std::swap(adjustedRange.start, adjustedRange.end);
    else if (!is_lt(ordering))
        return 0;
    uint64_t length = 0;
    for (TextIterator it(adjustedRange, behaviors); !it.atEnd(); it.advance())
        length += it.text().length();
    return length;
}

static inline bool isInsideReplacedElement(TextIterator& iterator, TextIteratorBehaviors behaviors)
{
    ASSERT(!iterator.atEnd());
    ASSERT(iterator.text().length() == 1);
    RefPtr node = iterator.node();
    return node && isRendererReplacedElement(node->renderer(), behaviors);
}

constexpr uint64_t clampedAdd(uint64_t a, uint64_t b)
{
    auto sum = a + b;
    return sum >= a ? sum : std::numeric_limits<uint64_t>::max();
}

SimpleRange resolveCharacterRange(const SimpleRange& scope, CharacterRange range, TextIteratorBehaviors behaviors)
{
    auto resultRange = SimpleRange { range.location ? scope.end : scope.start, (range.location || range.length) ? scope.end : scope.start };
    auto rangeEnd = clampedAdd(range.location, range.length);
    uint64_t location = 0;
    for (TextIterator it(scope, behaviors); !it.atEnd(); it.advance()) {
        unsigned length = it.text().length();
        auto textRunRange = it.range();

        auto found = [&] (uint64_t targetLocation) -> bool {
            return targetLocation >= location && targetLocation - location <= length;
        };
        bool foundStart = found(range.location);
        bool foundEnd = found(rangeEnd);

        if (foundEnd) {
            // FIXME: This is a workaround for the fact that the end of a run is often at the wrong position for emitted '\n's or if the renderer of the current node is a replaced element.
            // FIXME: consider controlling this with TextIteratorBehavior instead of doing it unconditionally to help us eventually phase it out everywhere.
            if (length == 1 && (it.text()[0] == '\n' || isInsideReplacedElement(it, behaviors))) {
                it.advance();
                if (!it.atEnd())
                    textRunRange.end = it.range().start;
                else {
                    if (auto end = makeBoundaryPoint(VisiblePosition(makeDeprecatedLegacyPosition(textRunRange.start)).next().deepEquivalent()))
                        textRunRange.end = *end;
                }
            }
        }

        auto boundary = [&] (uint64_t targetLocation) -> BoundaryPoint {
            if (is<Text>(textRunRange.start.container)) {
                ASSERT(targetLocation - location <= downcast<Text>(textRunRange.start.container.get()).length());
                unsigned offset = textRunRange.start.offset + targetLocation - location;
                return { textRunRange.start.container.copyRef(), offset };
            }
            return targetLocation == location ? textRunRange.start : textRunRange.end;
        };

        if (foundStart)
            resultRange.start = boundary(range.location);
        if (foundEnd) {
            resultRange.end = boundary(rangeEnd);
            break;
        }

        location += length;
    }
    return resultRange;
}

// --------

bool hasAnyPlainText(const SimpleRange& range, TextIteratorBehaviors behaviors, IgnoreCollapsedRanges ignoreCollapsedRanges)
{
    for (TextIterator iterator { range, behaviors }; !iterator.atEnd(); iterator.advance()) {
        if (ignoreCollapsedRanges == IgnoreCollapsedRanges::Yes && iterator.range().collapsed())
            continue;

        if (!iterator.text().isEmpty())
            return true;
    }
    return false;
}

String plainText(const SimpleRange& range, TextIteratorBehaviors defaultBehavior, bool isDisplayString)
{
    // The initial buffer size can be critical for performance: https://bugs.webkit.org/show_bug.cgi?id=81192
    constexpr unsigned initialCapacity = 1 << 15;

    Ref document = range.start.document();

    unsigned bufferLength = 0;
    StringBuilder builder;
    builder.reserveCapacity(initialCapacity);
    TextIteratorBehaviors behaviors = defaultBehavior;
    if (!isDisplayString)
        behaviors.add(TextIteratorBehavior::EmitsTextsWithoutTranscoding);

    for (TextIterator it(range, behaviors); !it.atEnd(); it.advance()) {
        it.appendTextToStringBuilder(builder);
        bufferLength += it.text().length();
    }

    if (!bufferLength)
        return emptyString();

    String result = builder.toString();

    if (isDisplayString)
        document->displayStringModifiedByEncoding(result);

    return result;
}

String plainTextReplacingNoBreakSpace(const SimpleRange& range, TextIteratorBehaviors defaultBehaviors, bool isDisplayString)
{
    return makeStringByReplacingAll(plainText(range, defaultBehaviors, isDisplayString), noBreakSpace, ' ');
}

static void forEachMatch(const SimpleRange& range, const String& target, FindOptions options, NOESCAPE const Function<bool(CharacterRange)>& match)
{
    SearchBuffer buffer(target, options);
    if (buffer.needsMoreContext()) {
        auto beforeStartRange = SimpleRange { makeBoundaryPointBeforeNodeContents(range.start.document()), range.start };
        for (SimplifiedBackwardsTextIterator backwardsIterator(beforeStartRange); !backwardsIterator.atEnd(); backwardsIterator.advance()) {
            buffer.prependContext(backwardsIterator.text());
            if (!buffer.needsMoreContext())
                break;
        }
    }

    CharacterIterator findIterator(range, findIteratorOptions(options));
    while (!findIterator.atEnd()) {
        findIterator.advance(buffer.append(findIterator.text()));
        while (1) {
            size_t matchStartOffset;
            size_t newMatchLength = buffer.search(matchStartOffset);
            if (!newMatchLength) {
                if (findIterator.atBreak() && !buffer.atBreak()) {
                    buffer.reachedBreak();
                    continue;
                }
                break;
            }
            size_t lastCharacterInBufferOffset = findIterator.characterOffset();
            ASSERT(lastCharacterInBufferOffset >= matchStartOffset);
            if (match(CharacterRange(lastCharacterInBufferOffset - matchStartOffset, newMatchLength)))
                return;
        }
    }
}

static SimpleRange rangeForMatch(const SimpleRange& range, FindOptions options, CharacterRange match)
{
    auto noMatchResult = [&] () {
        auto& boundary = options.contains(FindOption::Backwards) ? range.start : range.end;
        return SimpleRange { boundary, boundary };
    };

    if (!match.length)
        return noMatchResult();

    CharacterIterator it(range, findIteratorOptions(options));

    it.advance(match.location);
    if (it.atEnd())
        return noMatchResult();
    auto start = it.range().start;

    it.advance(match.length - 1);
    if (it.atEnd())
        return noMatchResult();

    return { WTFMove(start), it.range().end };
}

SimpleRange findClosestPlainText(const SimpleRange& range, const String& target, FindOptions options, uint64_t targetOffset)
{
    CharacterRange closestMatch;
    uint64_t closestMatchDistance = std::numeric_limits<uint64_t>::max();
    forEachMatch(range, target, options, [&] (CharacterRange match) {
        auto distance = [] (uint64_t a, uint64_t b) -> uint64_t {
            return std::abs(static_cast<int64_t>(a - b));
        };
        auto matchDistance = std::min(distance(match.location, targetOffset), distance(match.location + match.length, targetOffset));
        if (matchDistance > closestMatchDistance)
            return false;
        if (matchDistance == closestMatchDistance && !options.contains(FindOption::Backwards))
            return false;
        closestMatch = match;
        if (!matchDistance && !options.contains(FindOption::Backwards))
            return true;
        closestMatchDistance = matchDistance;
        return false;
    });
    return rangeForMatch(range, options, closestMatch);
}

SimpleRange findPlainText(const SimpleRange& range, const String& target, FindOptions options)
{
    // When searching forward stop since we want the first match.
    // When searching backward keep going since we want the last match.
    bool stopAfterFindingMatch = !options.contains(FindOption::Backwards);
    CharacterRange lastMatchFound;
    forEachMatch(range, target, options, [&] (CharacterRange match) {
        lastMatchFound = match;
        return stopAfterFindingMatch;
    });
    return rangeForMatch(range, options, lastMatchFound);
}

bool containsPlainText(const String& document, const String& target, FindOptions options)
{
    SearchBuffer buffer { target, options };
    StringView remainingText { document };
    while (!remainingText.isEmpty()) {
        size_t charactersAppended = buffer.append(document);
        remainingText = remainingText.substring(charactersAppended);
        if (remainingText.isEmpty())
            buffer.reachedBreak();
        size_t matchStartOffset;
        if (buffer.search(matchStartOffset))
            return true;
    }
    return false;
}

}

#if ENABLE(TREE_DEBUGGING)

void showTree(const WebCore::TextIterator& pos)
{
    pos.showTreeForThis();
}

void showTree(const WebCore::TextIterator* pos)
{
    if (pos)
        pos->showTreeForThis();
}

#endif
