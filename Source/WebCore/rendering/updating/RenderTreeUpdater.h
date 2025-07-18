/*
 * Copyright (C) 2016-2025 Apple Inc. All rights reserved.
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

#include "RenderTreeBuilder.h"
#include "RenderTreePosition.h"
#include "StyleChange.h"
#include "StyleTreeResolver.h"
#include "StyleUpdate.h"
#include <wtf/Vector.h>

namespace WebCore {

class ContainerNode;
class Document;
class Element;
class Node;
class RenderStyle;
class Text;

class RenderTreeUpdater {
public:
    RenderTreeUpdater(Document&, Style::PostResolutionCallbackDisabler&);
    ~RenderTreeUpdater();

    void commit(std::unique_ptr<Style::Update>);

    static void tearDownRenderers(Element&);
    static void tearDownRenderersForShadowRootInsertion(Element&);
    static void tearDownRenderersAfterSlotChange(Element& host);
    static void tearDownRenderer(Text&);

private:
    class GeneratedContent;
    class ViewTransition;

    void updateRenderTree(ContainerNode& root);
    void updateTextRenderer(Text&, const Style::TextUpdate*, const ContainerNode* root = nullptr);
    void createTextRenderer(Text&, const Style::TextUpdate*);
    void updateElementRenderer(Element&, const Style::ElementUpdate&);
    void updateSVGRenderer(Element&);
    void updateRendererStyle(RenderElement&, RenderStyle&&, StyleDifference);
    void updateRenderViewStyle();
    void createRenderer(Element&, RenderStyle&&);
    void updateBeforeDescendants(Element&, const Style::ElementUpdate*);
    void updateAfterDescendants(Element&, const Style::ElementUpdate*);
    bool textRendererIsNeeded(const Text& textNode);
    void storePreviousRenderer(Node&);

    void destroyAndCancelAnimationsForSubtree(RenderElement&);

    struct Parent {
        Element* element { nullptr };
        const Style::ElementUpdate* update { nullptr };
        std::optional<RenderTreePosition> renderTreePosition;

        bool didCreateOrDestroyChildRenderer { false };
        RenderObject* previousChildRenderer { nullptr };
        bool hasPrecedingInFlowChild { false };

        Parent(ContainerNode& root);
        Parent(Element&, const Style::ElementUpdate*);
    };
    Parent& parent() { return m_parentStack.last(); }
    Parent& renderingParent();
    RenderTreePosition& renderTreePosition();

    GeneratedContent& generatedContent() { return m_generatedContent; }
    ViewTransition& viewTransition() { return m_viewTransition; }

    void pushParent(Element&, const Style::ElementUpdate*);
    void popParent();
    void popParentsToDepth(unsigned depth);

    // FIXME: Use OptionSet.
    enum class TeardownType { Full, FullAfterSlotOrShadowRootChange, RendererUpdate, RendererUpdateCancelingAnimations };
    static void tearDownRenderers(Element&, TeardownType);
    static void tearDownRenderers(Element&, TeardownType, RenderTreeBuilder&);
    enum class NeedsRepaintAndLayout : bool { No, Yes };
    static void tearDownTextRenderer(Text&, const ContainerNode* root, RenderTreeBuilder&, NeedsRepaintAndLayout = NeedsRepaintAndLayout::Yes);
    static void tearDownLeftoverChildrenOfComposedTree(Element&, RenderTreeBuilder&);
    static void tearDownLeftoverPaginationRenderersIfNeeded(Element&, RenderTreeBuilder&);

    void updateRebuildRoots();

    RenderView& renderView();

    const Ref<Document> m_document;
    std::unique_ptr<Style::Update> m_styleUpdate;

    Vector<Parent> m_parentStack;

    const UniqueRef<GeneratedContent> m_generatedContent;
    const UniqueRef<ViewTransition> m_viewTransition;

    RenderTreeBuilder m_builder;
};

} // namespace WebCore
