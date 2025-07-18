/*
 * Copyright (C) 2006-2018 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TextControlInnerElements.h"

#include "ContainerNodeInlines.h"
#include "CSSPrimitiveValue.h"
#include "CSSToLengthConversionData.h"
#include "CommonAtomStrings.h"
#include "Document.h"
#include "DocumentInlines.h"
#include "EventNames.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "LocalFrame.h"
#include "LocalizedStrings.h"
#include "MouseEvent.h"
#include "Quirks.h"
#include "RenderSearchField.h"
#include "RenderStyleSetters.h"
#include "RenderTextControl.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "ResolvedStyle.h"
#include "ScriptController.h"
#include "ScriptDisallowedScope.h"
#include "ShadowRoot.h"
#include "StyleResolver.h"
#include "TextEvent.h"
#include "TextEventInputType.h"
#include "UserAgentParts.h"
#include <wtf/Ref.h>
#include <wtf/SetForScope.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(TextControlInnerContainer);
WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(TextControlInnerElement);
WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(TextControlInnerTextElement);
WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(TextControlPlaceholderElement);
WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(SearchFieldResultsButtonElement);
WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(SearchFieldCancelButtonElement);

using namespace CSS::Literals;
using namespace HTMLNames;

TextControlInnerContainer::TextControlInnerContainer(Document& document)
    : HTMLDivElement(divTag, document, TypeFlag::HasCustomStyleResolveCallbacks)
{
}

Ref<TextControlInnerContainer> TextControlInnerContainer::create(Document& document)
{
    return adoptRef(*new TextControlInnerContainer(document));
}
    
RenderPtr<RenderElement> TextControlInnerContainer::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderTextControlInnerContainer>(*this, WTFMove(style));
}

static inline bool isStrongPasswordTextField(const Element* element)
{
    RefPtr inputElement = dynamicDowncast<HTMLInputElement>(element);
    return inputElement && inputElement->hasAutofillStrongPasswordButton();
}

#if ENABLE(FORM_CONTROL_REFRESH)
static inline bool isNumberInput(const Element* element)
{
    RefPtr inputElement = dynamicDowncast<HTMLInputElement>(element);
    return inputElement && inputElement->isNumberField();
}
#endif

std::optional<Style::UnadjustedStyle> TextControlInnerContainer::resolveCustomStyle(const Style::ResolutionContext& resolutionContext, const RenderStyle* shadowHostStyle)
{
    RefPtr shadowHost = this->shadowHost();
    auto elementStyle = resolveStyle(resolutionContext);
    CheckedRef elementStyleStyle = *elementStyle.style;
    if (isStrongPasswordTextField(shadowHost.get())) {
        elementStyleStyle->setFlexWrap(FlexWrap::Wrap);
        elementStyleStyle->setOverflowX(Overflow::Hidden);
        elementStyleStyle->setOverflowY(Overflow::Hidden);
    }

    if (shadowHostStyle)
        RenderTheme::singleton().adjustTextControlInnerContainerStyle(elementStyleStyle.get(), *shadowHostStyle, shadowHost.get());

    return elementStyle;
}

TextControlInnerElement::TextControlInnerElement(Document& document)
    : HTMLDivElement(divTag, document, TypeFlag::HasCustomStyleResolveCallbacks)
{
}

Ref<TextControlInnerElement> TextControlInnerElement::create(Document& document)
{
    return adoptRef(*new TextControlInnerElement(document));
}

std::optional<Style::UnadjustedStyle> TextControlInnerElement::resolveCustomStyle(const Style::ResolutionContext&, const RenderStyle* shadowHostStyle)
{
    auto newStyle = RenderStyle::createPtr();
    newStyle->inheritFrom(*shadowHostStyle);
    newStyle->setFlexGrow(1);

    // Needed for correct shrinking.
    newStyle->setLogicalMinWidth(0_css_px);

    newStyle->setDisplay(DisplayType::Block);
    newStyle->setDirection(TextDirection::LTR);
    // We don't want the shadow DOM to be editable, so we set this block to read-only in case the input itself is editable.
    newStyle->setUserModify(UserModify::ReadOnly);

    if (isStrongPasswordTextField(shadowHost())) {
        newStyle->setFlexShrink(0);
        newStyle->setTextOverflow(TextOverflow::Clip);
        newStyle->setOverflowX(Overflow::Hidden);
        newStyle->setOverflowY(Overflow::Hidden);

        // Set "flex-basis: 1em". Note that CSSPrimitiveValue::resolveAsLength<int>() only needs the element's
        // style to calculate em lengths. Since the element might not be in a document, just pass nullptr
        // for the root element style, the parent element style, and the render view.
        auto emSize = CSSPrimitiveValue::create(1, CSSUnitType::CSS_EM);
        int pixels = emSize->resolveAsLength<int>(CSSToLengthConversionData { *newStyle, nullptr, nullptr, nullptr });
        newStyle->setFlexBasis(Style::FlexBasis::Fixed { static_cast<float>(pixels) });
    }

    return Style::UnadjustedStyle { WTFMove(newStyle) };
}

// MARK: TextControlInnerTextElement

inline TextControlInnerTextElement::TextControlInnerTextElement(Document& document)
    : HTMLDivElement(divTag, document, TypeFlag::HasCustomStyleResolveCallbacks )
{
}

Ref<TextControlInnerTextElement> TextControlInnerTextElement::create(Document& document, bool isEditable)
{
    auto result = adoptRef(*new TextControlInnerTextElement(document));
    constexpr bool initialization = true;
    ScriptDisallowedScope::EventAllowedScope eventAllowedScope { result };
    result->updateInnerTextElementEditabilityImpl(isEditable, initialization);
    return result;
}

void TextControlInnerTextElement::updateInnerTextElementEditabilityImpl(bool isEditable, bool initialization)
{
    const auto& value = isEditable ? plaintextOnlyAtom() : falseAtom();
    if (initialization) {
        Attribute attribute(contenteditableAttr, value);
        parserSetAttributes(singleElementSpan(attribute));
    } else
        setAttributeWithoutSynchronization(contenteditableAttr, value);
}

void TextControlInnerTextElement::defaultEventHandler(Event& event)
{
    // FIXME: In the future, we should add a way to have default event listeners.
    // Then we would add one to the text field's inner div, and we wouldn't need this subclass.
    // Or possibly we could just use a normal event listener.
    if (event.isBeforeTextInsertedEvent()) {
        // A TextControlInnerTextElement can have no host if its been detached,
        // but kept alive by an EditCommand. In this case, an undo/redo can
        // cause events to be sent to the TextControlInnerTextElement. To
        // prevent an infinite loop, we must check for this case before sending
        // the event up the chain.
        if (RefPtr host = shadowHost())
            host->defaultEventHandler(event);
    }
    if (!event.defaultHandled())
        HTMLDivElement::defaultEventHandler(event);
}

RenderPtr<RenderElement> TextControlInnerTextElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderTextControlInnerBlock>(*this, WTFMove(style));
}

RenderTextControlInnerBlock* TextControlInnerTextElement::renderer() const
{
    return downcast<RenderTextControlInnerBlock>(HTMLDivElement::renderer());
}

std::optional<Style::UnadjustedStyle> TextControlInnerTextElement::resolveCustomStyle(const Style::ResolutionContext&, const RenderStyle* shadowHostStyle)
{
    Ref shadowHost = *this->shadowHost();
    auto style = downcast<HTMLTextFormControlElement>(shadowHost.get()).createInnerTextStyle(*shadowHostStyle);

    if (shadowHostStyle)
        RenderTheme::singleton().adjustTextControlInnerTextStyle(style, *shadowHostStyle, shadowHost.ptr());

    return Style::UnadjustedStyle { makeUnique<RenderStyle>(WTFMove(style)) };
}

// MARK: TextControlPlaceholderElement

inline TextControlPlaceholderElement::TextControlPlaceholderElement(Document& document)
    : HTMLDivElement(divTag, document, TypeFlag::HasCustomStyleResolveCallbacks)
{
}

Ref<TextControlPlaceholderElement> TextControlPlaceholderElement::create(Document& document)
{
    auto element = adoptRef(*new TextControlPlaceholderElement(document));
    ScriptDisallowedScope::EventAllowedScope eventAllowedScope { element };
    element->setUserAgentPart(UserAgentParts::placeholder());
    return element;
}

std::optional<Style::UnadjustedStyle> TextControlPlaceholderElement::resolveCustomStyle(const Style::ResolutionContext& resolutionContext, const RenderStyle* shadowHostStyle)
{
    auto style = resolveStyle(resolutionContext);

    Ref controlElement = downcast<HTMLTextFormControlElement>(*containingShadowRoot()->host());
    CheckedRef styleStyle = *style.style;
    styleStyle->setDisplay(controlElement->isPlaceholderVisible() ? DisplayType::Block : DisplayType::None);

    if (RefPtr inputElement = dynamicDowncast<HTMLInputElement>(controlElement)) {
        styleStyle->setTextOverflow(inputElement->shouldTruncateText(*shadowHostStyle) ? TextOverflow::Ellipsis : TextOverflow::Clip);
        styleStyle->setPaddingTop(0_css_px);
        styleStyle->setPaddingBottom(0_css_px);
    }

    if (shadowHostStyle)
        RenderTheme::singleton().adjustTextControlInnerPlaceholderStyle(styleStyle.get(), *shadowHostStyle, protectedShadowHost().get());

    return style;
}

// MARK: SearchFieldResultsButtonElement

static inline bool searchFieldStyleHasExplicitlySpecifiedTextFieldAppearance(const RenderStyle& style)
{
    auto appearance = style.appearance();
    return appearance == StyleAppearance::TextField && appearance == style.usedAppearance();
}

inline SearchFieldResultsButtonElement::SearchFieldResultsButtonElement(Document& document)
    : HTMLDivElement(divTag, document, TypeFlag::HasCustomStyleResolveCallbacks)
{
}

Ref<SearchFieldResultsButtonElement> SearchFieldResultsButtonElement::create(Document& document)
{
    return adoptRef(*new SearchFieldResultsButtonElement(document));
}

std::optional<Style::UnadjustedStyle> SearchFieldResultsButtonElement::resolveCustomStyle(const Style::ResolutionContext& resolutionContext, const RenderStyle* shadowHostStyle)
{
    m_canAdjustStyleForAppearance = true;

    RefPtr input = downcast<HTMLInputElement>(shadowHost());
    if (input && input->maxResults() >= 0)
        return std::nullopt;

    if (!shadowHostStyle)
        return std::nullopt;

    if (searchFieldStyleHasExplicitlySpecifiedTextFieldAppearance(*shadowHostStyle)) {
        auto elementStyle = resolveStyle(resolutionContext);
        elementStyle.style->setDisplay(DisplayType::None);
        return elementStyle;
    }

    // By default, input[type=search] can use either the searchfield or textfield appearance depending
    // on the platform and writing mode. Only adjust the style when that default is used.
    auto usedAppearance = shadowHostStyle->usedAppearance();
    if (usedAppearance != StyleAppearance::SearchField && usedAppearance != StyleAppearance::TextField) {
        m_canAdjustStyleForAppearance = false;
        return resolveStyle(resolutionContext);
    }

    return std::nullopt;
}

void SearchFieldResultsButtonElement::defaultEventHandler(Event& event)
{
    // On mousedown, bring up a menu, if needed
    if (RefPtr input = downcast<HTMLInputElement>(shadowHost())) {
        auto* mouseEvent = dynamicDowncast<MouseEvent>(event);
        if (event.type() == eventNames().mousedownEvent && mouseEvent && mouseEvent->button() == MouseButton::Left) {
            input->focus();
            input->select();
#if !PLATFORM(IOS_FAMILY)
            protectedDocument()->updateStyleIfNeeded();

            if (CheckedPtr searchFieldRenderer = dynamicDowncast<RenderSearchField>(input->renderer())) {
                if (searchFieldRenderer->popupIsVisible())
                    searchFieldRenderer->hidePopup();
                else if (input->maxResults() > 0)
                    searchFieldRenderer->showPopup();
            }
#endif
            event.setDefaultHandled();
        }
    }

    if (!event.defaultHandled())
        HTMLDivElement::defaultEventHandler(event);
}

#if !PLATFORM(IOS_FAMILY)
bool SearchFieldResultsButtonElement::willRespondToMouseClickEventsWithEditability(Editability) const
{
    return true;
}
#endif

// MARK: SearchFieldCancelButtonElement

inline SearchFieldCancelButtonElement::SearchFieldCancelButtonElement(Document& document)
    : HTMLDivElement(divTag, document, TypeFlag::HasCustomStyleResolveCallbacks)
{
}

Ref<SearchFieldCancelButtonElement> SearchFieldCancelButtonElement::create(Document& document)
{
    auto element = adoptRef(*new SearchFieldCancelButtonElement(document));

    ScriptDisallowedScope::EventAllowedScope eventAllowedScope { element };
    element->setUserAgentPart(UserAgentParts::webkitSearchCancelButton());
#if !PLATFORM(IOS_FAMILY)
    element->setAttributeWithoutSynchronization(aria_labelAttr, AtomString { AXSearchFieldCancelButtonText() });
#endif
    element->setAttributeWithoutSynchronization(roleAttr, HTMLNames::buttonTag->localName());
    return element;
}

std::optional<Style::UnadjustedStyle> SearchFieldCancelButtonElement::resolveCustomStyle(const Style::ResolutionContext& resolutionContext, const RenderStyle* shadowHostStyle)
{
    auto elementStyle = resolveStyle(resolutionContext);
    Ref inputElement = downcast<HTMLInputElement>(*shadowHost());
    elementStyle.style->setVisibility(elementStyle.style->usedVisibility() == Visibility::Hidden || inputElement->value()->isEmpty() ? Visibility::Hidden : Visibility::Visible);

    if (shadowHostStyle && searchFieldStyleHasExplicitlySpecifiedTextFieldAppearance(*shadowHostStyle))
        elementStyle.style->setDisplay(DisplayType::None);

    return elementStyle;
}

void SearchFieldCancelButtonElement::defaultEventHandler(Event& event)
{
    RefPtr input = downcast<HTMLInputElement>(shadowHost());
    if (!input || !input->isMutable()) {
        if (!event.defaultHandled())
            HTMLDivElement::defaultEventHandler(event);
        return;
    }

    if (auto* mouseEvent = dynamicDowncast<MouseEvent>(event); event.type() == eventNames().mousedownEvent && mouseEvent && mouseEvent->button() == MouseButton::Left) {
        input->focus();
        input->select();
        event.setDefaultHandled();
    }

    if (isAnyClick(event)) {
        input->setValue(emptyString(), DispatchChangeEvent);
        event.setDefaultHandled();
    }

    if (!event.defaultHandled())
        HTMLDivElement::defaultEventHandler(event);
}

#if !PLATFORM(IOS_FAMILY)
bool SearchFieldCancelButtonElement::willRespondToMouseClickEventsWithEditability(Editability editability) const
{
    RefPtr input = downcast<HTMLInputElement>(shadowHost());
    if (input && input->isMutable())
        return true;

    return HTMLDivElement::willRespondToMouseClickEventsWithEditability(editability);
}
#endif

}
