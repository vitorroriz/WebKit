/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "RenderTreePosition.h"
#include "RenderWidget.h"

namespace WebCore {

class RenderGrid;
class RenderTreeUpdater;

class RenderTreeBuilder {
public:
    RenderTreeBuilder(RenderView&);
    ~RenderTreeBuilder();

    // This avoids having to convert all sites that need RenderTreeBuilder in one go.
    // FIXME: Remove.
    static RenderTreeBuilder* current() { return s_current; }

    static bool isRebuildRootForChildren(const RenderElement&);

    void attach(RenderElement& parent, RenderPtr<RenderObject>, RenderObject* beforeChild = nullptr);

    enum class IsInternalMove : bool { No, Yes };
    enum class WillBeDestroyed : bool { No, Yes };
    enum class CanCollapseAnonymousBlock : bool { No, Yes };
    RenderPtr<RenderObject> detach(RenderElement&, RenderObject&, WillBeDestroyed, CanCollapseAnonymousBlock = CanCollapseAnonymousBlock::Yes) WARN_UNUSED_RETURN;

    enum class TearDownType : uint8_t {
        Root,                          // destroy root renderer
        SubtreeWithRootStillAttached,  // subtree teardown when renderers are still attached to the tree (common case)
        SubtreeWithRootAlreadyDetached // subtree teardown when destroy root gets detached first followed by destroying renderers (e.g. pseudo subtree)
    };
    void destroy(RenderObject& renderer, CanCollapseAnonymousBlock = CanCollapseAnonymousBlock::Yes);

    // NormalizeAfterInsertion::Yes ensures that the destination subtree is consistent after the insertion (anonymous wrappers etc).
    enum class NormalizeAfterInsertion : bool { No, Yes };
    void move(RenderBoxModelObject& from, RenderBoxModelObject& to, RenderObject& child, NormalizeAfterInsertion);

    void updateAfterDescendants(RenderElement&);
    void destroyAndCleanUpAnonymousWrappers(RenderObject& child, const RenderElement* destroyRoot);
    void normalizeTreeAfterStyleChange(RenderElement&, RenderStyle& oldStyle);

    bool hasBrokenContinuation() const { return m_hasBrokenContinuation; }

private:
    static void markBoxForRelayoutAfterSplit(RenderBox&);

    void attachInternal(RenderElement& parent, RenderPtr<RenderObject>, RenderObject* beforeChild);

    void childFlowStateChangesAndAffectsParentBlock(RenderElement& child);
    void childFlowStateChangesAndNoLongerAffectsParentBlock(RenderElement& child);
    void attachIgnoringContinuation(RenderElement& parent, RenderPtr<RenderObject>, RenderObject* beforeChild = nullptr);
    void attachToRenderGrid(RenderGrid& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild = nullptr);
    void attachToRenderElement(RenderElement& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild = nullptr);
    void attachToRenderElementInternal(RenderElement& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild = nullptr);

    RenderPtr<RenderObject> detachFromRenderElement(RenderElement& parent, RenderObject& child, WillBeDestroyed) WARN_UNUSED_RETURN;
    RenderPtr<RenderObject> detachFromRenderGrid(RenderGrid& parent, RenderObject& child, WillBeDestroyed) WARN_UNUSED_RETURN;

    void move(RenderBoxModelObject& from, RenderBoxModelObject& to, RenderObject& child, RenderObject* beforeChild, NormalizeAfterInsertion);
    // Move all of the kids from |startChild| up to but excluding |endChild|. 0 can be passed as the |endChild| to denote
    // that all the kids from |startChild| onwards should be moved.
    void moveChildren(RenderBoxModelObject& from, RenderBoxModelObject& to, RenderObject* startChild, RenderObject* endChild, NormalizeAfterInsertion);
    void moveChildren(RenderBoxModelObject& from, RenderBoxModelObject& to, RenderObject* startChild, RenderObject* endChild, RenderObject* beforeChild, NormalizeAfterInsertion);
    void moveAllChildrenIncludingFloats(RenderBlock& from, RenderBlock& toBlock, RenderTreeBuilder::NormalizeAfterInsertion);
    void moveAllChildren(RenderBoxModelObject& from, RenderBoxModelObject& to, NormalizeAfterInsertion);
    void moveAllChildren(RenderBoxModelObject& from, RenderBoxModelObject& to, RenderObject* beforeChild, NormalizeAfterInsertion);

    void removeFloatingObjects(RenderBlock&);

    RenderObject* splitAnonymousBoxesAroundChild(RenderBox& parent, RenderObject& originalBeforeChild);
    void createAnonymousWrappersForInlineContent(RenderBlock& parent, RenderObject* insertionPoint = nullptr);
    void removeAnonymousWrappersForInlineChildrenIfNeeded(RenderElement& parent);

    void reportVisuallyNonEmptyContent(const RenderElement& parent, const RenderObject& child);

    void setHasBrokenContinuation() { m_hasBrokenContinuation = true; }

    static RenderPtr<RenderBox> createAnonymousBoxWithSameTypeAndWithStyle(const RenderBox&, const RenderStyle&);

    class FirstLetter;
    class List;
    class MultiColumn;
    class Table;
    class Ruby;
    class FormControls;
    class Block;
    class BlockFlow;
    class Inline;
    class SVG;
#if ENABLE(MATHML)
    class MathML;
#endif
    class Continuation;

    FirstLetter& firstLetterBuilder() { return m_firstLetterBuilder; }
    List& listBuilder() { return m_listBuilder; }
    MultiColumn& multiColumnBuilder() { return m_multiColumnBuilder; }
    Table& tableBuilder() { return m_tableBuilder; }
    Ruby& rubyBuilder() { return m_rubyBuilder; }
    FormControls& formControlsBuilder() { return m_formControlsBuilder; }
    Block& blockBuilder() { return m_blockBuilder; }
    BlockFlow& blockFlowBuilder() { return m_blockFlowBuilder; }
    Inline& inlineBuilder() { return m_inlineBuilder; }
    SVG& svgBuilder() { return m_svgBuilder; }
#if ENABLE(MATHML)
    MathML& mathMLBuilder() { return m_mathMLBuilder; }
#endif
    Continuation& continuationBuilder() { return m_continuationBuilder; }

    WidgetHierarchyUpdatesSuspensionScope m_widgetHierarchyUpdatesSuspensionScope;
    RenderView& m_view;
    RenderTreeBuilder* m_previous { nullptr };
    static RenderTreeBuilder* s_current;

    const UniqueRef<FirstLetter> m_firstLetterBuilder;
    const UniqueRef<List> m_listBuilder;
    const UniqueRef<MultiColumn> m_multiColumnBuilder;
    const UniqueRef<Table> m_tableBuilder;
    const UniqueRef<Ruby> m_rubyBuilder;
    const UniqueRef<FormControls> m_formControlsBuilder;
    const UniqueRef<Block> m_blockBuilder;
    const UniqueRef<BlockFlow> m_blockFlowBuilder;
    const UniqueRef<Inline> m_inlineBuilder;
    const UniqueRef<SVG> m_svgBuilder;
#if ENABLE(MATHML)
    const UniqueRef<MathML> m_mathMLBuilder;
#endif
    const UniqueRef<Continuation> m_continuationBuilder;
    bool m_hasBrokenContinuation { false };
    IsInternalMove m_internalMovesType { IsInternalMove::No };
    TearDownType m_tearDownType { TearDownType::Root };
    CheckedPtr<const RenderElement> m_subtreeDestroyRoot;
    SingleThreadWeakPtr<const RenderObject> m_anonymousDestroyRoot;
};

}
