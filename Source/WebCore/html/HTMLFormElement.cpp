/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2010, 2012-2016 Apple Inc. All rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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
#include "HTMLFormElement.h"

#include "CommonAtomStrings.h"
#include "DOMFormData.h"
#include "DOMTokenList.h"
#include "DiagnosticLoggingClient.h"
#include "Document.h"
#include "ElementAncestorIteratorInlines.h"
#include "Event.h"
#include "EventNames.h"
#include "FormController.h"
#include "FormData.h"
#include "FormDataEvent.h"
#include "FrameLoader.h"
#include "HTMLDialogElement.h"
#include "HTMLFieldSetElement.h"
#include "HTMLFormControlsCollection.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLObjectElement.h"
#include "HTMLTableElement.h"
#include "InputTypeNames.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "LocalFrameLoaderClient.h"
#include "MixedContentChecker.h"
#include "NodeName.h"
#include "NodeRareData.h"
#include "Page.h"
#include "PseudoClassChangeInvalidation.h"
#include "RadioNodeList.h"
#include "RenderTextControl.h"
#include "ScriptDisallowedScope.h"
#include "Settings.h"
#include "SubmitEvent.h"
#include "TypedElementDescendantIteratorInlines.h"
#include "UserGestureIndicator.h"
#include <limits>
#include <numeric>
#include <wtf/Ref.h>
#include <wtf/SetForScope.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(HTMLFormElement);

using namespace HTMLNames;

struct FormRelAttributes {
    bool noopener { false };
    bool noreferrer { false };
    bool opener { false };
};

static FormRelAttributes parseFormRelAttributes(StringView string)
{
    FormRelAttributes attributes;
    for (auto token : string.split(' ')) {
        if (equalLettersIgnoringASCIICase(token, "noopener"_s))
            attributes.noopener = true;
        else if (equalLettersIgnoringASCIICase(token, "noreferrer"_s))
            attributes.noreferrer = true;
        else if (equalLettersIgnoringASCIICase(token, "opener"_s))
            attributes.opener = true;
    }
    return attributes;
}

HTMLFormElement::HTMLFormElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document, TypeFlag::HasDidMoveToNewDocument)
{
    ASSERT(hasTagName(formTag));
}

Ref<HTMLFormElement> HTMLFormElement::create(Document& document)
{
    return adoptRef(*new HTMLFormElement(formTag, document));
}

Ref<HTMLFormElement> HTMLFormElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new HTMLFormElement(tagName, document));
}

HTMLFormElement::~HTMLFormElement()
{
    document().formController().willDeleteForm(*this);
    if (!shouldAutocomplete())
        document().unregisterForDocumentSuspensionCallbacks(*this);

    m_defaultButton = nullptr;
    for (auto& weakElement : m_listedElements) {
        ASSERT(weakElement);
        RefPtr listedElement = weakElement->asFormListedElement();
        ASSERT(listedElement);
        listedElement->formWillBeDestroyed();
    }
    for (auto& imageElement : m_imageElements)
        imageElement->formWillBeDestroyed();
}

Node::InsertedIntoAncestorResult HTMLFormElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    HTMLElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    if (insertionType.connectedToDocument)
        document().didAssociateFormControl(*this);
    return InsertedIntoAncestorResult::Done;
}

void HTMLFormElement::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    auto& root = traverseToRootNode(); // Do not rely on rootNode() because our IsInTreeScope is outdated.
    auto listedElements = copyListedElementsVector();
    for (auto& listedElement : listedElements)
        listedElement->formOwnerRemovedFromTree(root);
    auto imageElements = WTF::compactMap(m_imageElements, [](auto& weakPtr) {
        return RefPtr { weakPtr.get() };
    });
    for (auto& imageElement : imageElements)
        imageElement->formOwnerRemovedFromTree(root);
    HTMLElement::removedFromAncestor(removalType, oldParentOfRemovedTree);
    if (removalType.disconnectedFromDocument)
      m_controlsCollection = nullptr; // Avoid leaks since HTMLCollection has a back Ref to this element.
}

unsigned HTMLFormElement::length() const
{
    unsigned length = 0;
    for (auto& weakElement : m_listedElements) {
        RefPtr element { weakElement.get() };
        ASSERT(element);
        auto* listedElement = element->asFormListedElement();
        ASSERT(listedElement);
        if (listedElement->isEnumeratable())
            ++length;
    }
    return length;
}

HTMLElement* HTMLFormElement::item(unsigned index)
{
    return elements()->item(index);
}

std::optional<Variant<RefPtr<RadioNodeList>, RefPtr<Element>>> HTMLFormElement::namedItem(const AtomString& name)
{
    auto namedItems = namedElements(name);

    if (namedItems.isEmpty())
        return std::nullopt;
    if (namedItems.size() == 1)
        return Variant<RefPtr<RadioNodeList>, RefPtr<Element>> { RefPtr<Element> { WTFMove(namedItems[0]) } };

    return Variant<RefPtr<RadioNodeList>, RefPtr<Element>> { RefPtr<RadioNodeList> { radioNodeList(name) } };
}

Vector<AtomString> HTMLFormElement::supportedPropertyNames() const
{
    // FIXME: Should be implemented (only needed for enumeration with includeDontEnumProperties mode
    // since this class is annotated with LegacyUnenumerableNamedProperties).
    return { };
}

void HTMLFormElement::submitImplicitly(Event& event, bool fromImplicitSubmissionTrigger)
{
    unsigned submissionTriggerCount = 0;
    for (auto& listedElement : m_listedElements) {
        RefPtr formElement = dynamicDowncast<HTMLFormControlElement>(*listedElement);
        if (!formElement)
            continue;
        if (formElement->isSuccessfulSubmitButton()) {
            formElement->dispatchSimulatedClick(&event);
            return;
        }
        if (formElement->canTriggerImplicitSubmission())
            ++submissionTriggerCount;
    }

    if (!submissionTriggerCount)
        return;

    // Older iOS apps using WebViews expect the behavior of auto submitting multi-input forms.
    if (fromImplicitSubmissionTrigger && (submissionTriggerCount == 1 || document().settings().allowMultiElementImplicitSubmission()))
        submitIfPossible(&event);
}

bool HTMLFormElement::validateInteractively()
{
    for (auto& listedElement : m_listedElements) {
        if (auto* control = listedElement->asValidatedFormListedElement())
            control->hideVisibleValidationMessage();
    }

    Vector<RefPtr<ValidatedFormListedElement>> unhandledInvalidControls;
    if (!checkInvalidControlsAndCollectUnhandled(unhandledInvalidControls))
        return true;
    // Because the form has invalid controls, we abort the form submission and
    // show a validation message on a focusable form control.

    // Make sure layout is up-to-date in case we call isFocusable() (which
    // has !renderer()->needsLayout() assertion).
    ASSERT(!document().view() || !document().view()->needsLayout());

    Ref<HTMLFormElement> protectedThis(*this);

    // Focus on the first focusable control and show a validation message.
    bool shouldFocus = true;
    for (auto& control : unhandledInvalidControls) {
        if (auto validationAnchor = control->focusableValidationAnchorElement()) {
            if (shouldFocus) {
                shouldFocus = false;
                control->focusAndShowValidationMessage(validationAnchor.releaseNonNull());
            }
        } else
            control->reportNonFocusableControlError();
    }

    return false;
}

void HTMLFormElement::submitIfPossible(Event* event, HTMLFormControlElement* submitter, FormSubmissionTrigger trigger)
{
    // https://html.spec.whatwg.org/#form-submission-algorithm
    if (!isConnected())
        return;

    RefPtr frame { document().frame() };
    if (m_isSubmittingOrPreparingForSubmission || !frame)
        return;

    m_isSubmittingOrPreparingForSubmission = true;
    m_shouldSubmit = false;

    for (auto& element : m_listedElements) {
        if (auto* formControlElement = dynamicDowncast<HTMLFormControlElement>(*element))
            formControlElement->setInteractedWithSinceLastFormSubmitEvent(true);
    }

    bool shouldValidate = document().page() && document().page()->settings().interactiveFormValidationEnabled() && !noValidate();
    if (shouldValidate) {
        RefPtr submitElement = submitter ? submitter : findSubmitter(event);
        if (submitElement && submitElement->formNoValidate())
            shouldValidate = false;
    }

    // Interactive validation must be done before dispatching the submit event.
    if (shouldValidate && !validateInteractively()) {
        m_isSubmittingOrPreparingForSubmission = false;
        return;
    }

    RefPtr targetFrame = frame->loader().findFrameForNavigation(effectiveTarget(event, submitter), &document());
    if (!targetFrame)
        targetFrame = frame.get();
    auto formState = FormState::create(*this, textFieldValues(), document(), NotSubmittedByJavaScript);
    if (RefPtr localTargetFrame = dynamicDowncast<LocalFrame>(targetFrame))
        localTargetFrame->loader().client().dispatchWillSendSubmitEvent(WTFMove(formState));

    Ref protectedThis { *this };

    auto submitEvent = SubmitEvent::create(submitter);
    dispatchEvent(submitEvent);

    // Event handling could have resulted in m_shouldSubmit becoming true as a side effect, too.
    if (!submitEvent->defaultPrevented())
        m_shouldSubmit = true;

    m_isSubmittingOrPreparingForSubmission = false;

    if (!m_shouldSubmit)
        return;

    if (auto plannedFormSubmission = std::exchange(m_plannedFormSubmission, nullptr))
        plannedFormSubmission->cancel();

    submit(event, !submitter, trigger, submitter);
}

void HTMLFormElement::submit()
{
    submit(nullptr, true, NotSubmittedByJavaScript);
}

void HTMLFormElement::submitFromJavaScript()
{
    submit(nullptr, UserGestureIndicator::processingUserGesture(), SubmittedByJavaScript);
}

ExceptionOr<void> HTMLFormElement::requestSubmit(HTMLElement* submitter)
{
    // Update layout before processing form actions in case the style changes
    // the form or button relationships.
    protectedDocument()->updateLayoutIgnorePendingStylesheets();

    RefPtr<HTMLFormControlElement> control;
    if (submitter) {
        // https://html.spec.whatwg.org/multipage/forms.html#dom-form-requestsubmit
        control = dynamicDowncast<HTMLFormControlElement>(*submitter);
        if (!control || !control->isSubmitButton())
            return Exception { ExceptionCode::TypeError, "The specified element is not a submit button."_s };
        if (control->form() != this)
            return Exception { ExceptionCode::NotFoundError, "The specified element is not owned by this form element."_s };
    }

    submitIfPossible(nullptr, control.get(), SubmittedByJavaScript);
    return { };
}

StringPairVector HTMLFormElement::textFieldValues() const
{
    return WTF::compactMap(m_listedElements, [](auto& weakElement) -> std::optional<std::pair<String, String>> {
        RefPtr input = dynamicDowncast<HTMLInputElement>(weakElement.get());
        if (!input || !input->isTextField())
            return std::nullopt;
        return std::pair { input->name().string(), input->value() };
    });
}

RefPtr<HTMLFormControlElement> HTMLFormElement::findSubmitButton(HTMLFormControlElement* submitter, bool needButtonActivation)
{
    if (submitter)
        return submitter;
    if (!needButtonActivation)
        return nullptr;
    RefPtr<HTMLFormControlElement> firstSuccessfulSubmitButton;
    for (auto& listedElement : m_listedElements) {
        auto* control = dynamicDowncast<HTMLFormControlElement>(*listedElement);
        if (!control)
            continue;
        if (control->isActivatedSubmit())
            return nullptr;
        if (!firstSuccessfulSubmitButton && control->isSuccessfulSubmitButton())
            firstSuccessfulSubmitButton = control;
    }
    return firstSuccessfulSubmitButton;
}

void HTMLFormElement::submit(Event* event, bool processingUserGesture, FormSubmissionTrigger trigger, HTMLFormControlElement* submitter)
{
    // The submitIfPossible function also does this check, but we need to do it here
    // too, since there are some code paths that bypass that function.
    if (!isConnected())
        return;

    if (m_isConstructingEntryList)
        return;

    RefPtr view { document().view() };
    RefPtr frame { document().frame() };
    if (!view || !frame)
        return;

    if (trigger != SubmittedByJavaScript && m_isSubmittingOrPreparingForSubmission) {
        m_shouldSubmit = true;
        return;
    }

    m_isSubmittingOrPreparingForSubmission = true;
    m_wasUserSubmitted = processingUserGesture;

    if (event && !submitter) {
        // In a case of implicit submission without a submit button, 'submit' event handler might add a submit button. We search for a submit button again.
        auto listedElements = copyListedElementsVector();
        for (auto& element : listedElements) {
            if (auto* control = dynamicDowncast<HTMLFormControlElement>(element.get()); control && control->isSuccessfulSubmitButton()) {
                submitter = control;
                break;
            }
        }
    }

    Ref protectedThis { *this }; // Form submission can execute arbitary JavaScript.

    auto shouldLockHistory = processingUserGesture ? LockHistory::No : LockHistory::Yes;
    auto formSubmission = FormSubmission::create(*this, submitter, m_attributes, event, shouldLockHistory, trigger);

    if (!isConnected())
        return;

    auto relAttributes = parseFormRelAttributes(getAttribute(HTMLNames::relAttr));
    if (relAttributes.noopener || relAttributes.noreferrer || (!relAttributes.opener && isBlankTargetFrameName(formSubmission->target()) && !formSubmission->requestURL().protocolIsJavaScript()))
        formSubmission->setNewFrameOpenerPolicy(NewFrameOpenerPolicy::Suppress);
    if (relAttributes.noreferrer)
        formSubmission->setReferrerPolicy(ReferrerPolicy::NoReferrer);

    m_plannedFormSubmission = formSubmission;

    if (formSubmission->method() == FormSubmission::Method::Dialog)
        submitDialog(WTFMove(formSubmission));
    else
        frame->loader().submitForm(WTFMove(formSubmission));

    m_shouldSubmit = false;
    m_isSubmittingOrPreparingForSubmission = false;
}

// https://html.spec.whatwg.org/#submit-dialog
void HTMLFormElement::submitDialog(Ref<FormSubmission>&& formSubmission)
{
    // Let subject be the nearest ancestor dialog element of form, if any.
    RefPtr dialog = ancestorsOfType<HTMLDialogElement>(*this).first();

    // If there isn't one, or if it does not have an open attribute, do nothing.
    if (!dialog || !dialog->isOpen())
        return;

    // Then, close the dialog subject. If there is a result, let that be the return value.
    dialog->close(formSubmission->returnValue());
}

void HTMLFormElement::reset()
{
    if (m_isInResetFunction)
        return;

    RefPtr protectedFrame { document().frame() };
    if (!protectedFrame)
        return;

    Ref<HTMLFormElement> protectedThis(*this);

    SetForScope isInResetFunctionRestorer(m_isInResetFunction, true);

    auto event = Event::create(eventNames().resetEvent, Event::CanBubble::Yes, Event::IsCancelable::Yes);
    dispatchEvent(event);
    if (!event->defaultPrevented())
        resetListedFormControlElements();
}

void HTMLFormElement::resetListedFormControlElements()
{
    // Event handling can cause listed elements to be added or deleted while iterating
    // over this collection. Protect these elements until we are done notifying them of
    // the reset operation.
    for (auto& control : copyValidatedListedElementsVector())
        control->reset();
}

void HTMLFormElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    switch (name.nodeName()) {
    case AttributeNames::actionAttr:
        m_attributes.parseAction(newValue);
        if (!m_attributes.action().isEmpty()) {
            if (RefPtr f = document().frame()) {
                if (auto* topFrame = dynamicDowncast<LocalFrame>(f->tree().top()))
                    MixedContentChecker::checkFormForMixedContent(*topFrame, document().completeURL(m_attributes.action()));
            }
        }
        break;
    case AttributeNames::targetAttr:
        m_attributes.setTarget(newValue);
        break;
    case AttributeNames::methodAttr:
        m_attributes.updateMethodType(newValue);
        break;
    case AttributeNames::enctypeAttr:
        m_attributes.updateEncodingType(newValue);
        break;
    case AttributeNames::accept_charsetAttr:
        m_attributes.setAcceptCharset(newValue);
        break;
    case AttributeNames::autocompleteAttr:
        if (!shouldAutocomplete())
            document().registerForDocumentSuspensionCallbacks(*this);
        else
            document().unregisterForDocumentSuspensionCallbacks(*this);
        break;
    case AttributeNames::relAttr:
        if (m_relList)
            m_relList->associatedAttributeValueChanged();
        break;
    default:
        HTMLElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
        break;
    }
}

unsigned HTMLFormElement::formElementIndexWithFormAttribute(Element* element, unsigned rangeStart, unsigned rangeEnd)
{
    if (m_listedElements.isEmpty())
        return 0;

    ASSERT(rangeStart <= rangeEnd);

    if (rangeStart == rangeEnd)
        return rangeStart;

    unsigned left = rangeStart;
    unsigned right = rangeEnd - 1;
    unsigned short position;

    // Does binary search on m_listedElements in order to find the index to be inserted.
    while (left != right) {
        unsigned middle = std::midpoint(left, right);
        ASSERT(middle < m_listedElementsBeforeIndex || middle >= m_listedElementsAfterIndex);
        position = element->compareDocumentPosition(*m_listedElements[middle]);
        if (position & DOCUMENT_POSITION_FOLLOWING)
            right = middle;
        else
            left = middle + 1;
    }
    
    ASSERT(left < m_listedElementsBeforeIndex || left >= m_listedElementsAfterIndex);
    position = element->compareDocumentPosition(*m_listedElements[left]);
    if (position & DOCUMENT_POSITION_FOLLOWING)
        return left;
    return left + 1;
}

unsigned HTMLFormElement::formElementIndex(FormListedElement& listedElement)
{
    HTMLElement& listedHTMLElement = listedElement.asHTMLElement();

    // Treats separately the case where this element has the form attribute
    // for performance consideration.
    if (listedHTMLElement.hasAttributeWithoutSynchronization(formAttr) && listedHTMLElement.isConnected()) {
        unsigned short position;
        if (document().settings().shadowRootReferenceTargetEnabled())
            position = listedHTMLElement.treeScope().retargetToScope(*this)->compareDocumentPosition(listedHTMLElement);
        else
            position = compareDocumentPosition(listedHTMLElement);
        ASSERT(!(position & DOCUMENT_POSITION_DISCONNECTED));
        if (position & DOCUMENT_POSITION_PRECEDING) {
            ++m_listedElementsBeforeIndex;
            ++m_listedElementsAfterIndex;
            return HTMLFormElement::formElementIndexWithFormAttribute(&listedHTMLElement, 0, m_listedElementsBeforeIndex - 1);
        }
        if (position & DOCUMENT_POSITION_FOLLOWING && !(position & DOCUMENT_POSITION_CONTAINED_BY))
            return HTMLFormElement::formElementIndexWithFormAttribute(&listedHTMLElement, m_listedElementsAfterIndex, m_listedElements.size());
    }

    unsigned currentListedElementsAfterIndex = m_listedElementsAfterIndex;
    ++m_listedElementsAfterIndex;

    if (!listedHTMLElement.isDescendantOf(*this))
        return currentListedElementsAfterIndex;

    auto descendants = descendantsOfType<HTMLElement>(*this);

    // Check for the special case where this element is the very last thing in
    // the form's tree of children; we don't want to walk the entire tree in that
    // common case that occurs during parsing; instead we'll just return a value
    // that says "add this form element to the end of the array".
    if (!++descendants.beginAt(listedHTMLElement))
        return currentListedElementsAfterIndex;

    unsigned i = m_listedElementsBeforeIndex;
    for (auto& element : descendants) {
        if (&element == &listedHTMLElement)
            return i;
        if (!element.isFormListedElement())
            continue;
        if (element.asFormListedElement()->form() != this)
            continue;
        ++i;
    }
    return currentListedElementsAfterIndex;
}

void HTMLFormElement::registerFormListedElement(FormListedElement& element)
{
    m_listedElements.insert(formElementIndex(element), element.asHTMLElement());

    auto* control = dynamicDowncast<HTMLFormControlElement>(element);
    if (!control || !control->isSuccessfulSubmitButton())
        return;

    if (!m_defaultButton)
        control->invalidateStyleForSubtree();
    else
        resetDefaultButton();
}

void HTMLFormElement::unregisterFormListedElement(FormListedElement& element)
{
    unsigned index = m_listedElements.find(&element.asHTMLElement());
    ASSERT(index < m_listedElements.size());
    if (index < m_listedElementsBeforeIndex)
        --m_listedElementsBeforeIndex;
    if (index < m_listedElementsAfterIndex)
        --m_listedElementsAfterIndex;
    removeFromPastNamesMap(element);
    m_listedElements.removeAt(index);

    if (auto* nodeLists = this->nodeLists())
        nodeLists->invalidateCaches();

    if (&element == m_defaultButton)
        resetDefaultButton();
}

void HTMLFormElement::addInvalidFormControl(const HTMLElement& formControlElement)
{
    ASSERT_WITH_MESSAGE(!is<HTMLFieldSetElement>(formControlElement), "FieldSet are never candidates for constraint validation.");
    ASSERT(static_cast<const Element&>(formControlElement).matchesInvalidPseudoClass());

    std::optional<Style::PseudoClassChangeInvalidation> styleInvalidation;
    if (m_invalidFormControls.isEmptyIgnoringNullReferences())
        emplace(styleInvalidation, *this, { { CSSSelector::PseudoClass::Valid, false }, { CSSSelector::PseudoClass::Invalid, true } });

    m_invalidFormControls.add(formControlElement);
}

void HTMLFormElement::removeInvalidFormControlIfNeeded(const HTMLElement& formControlElement)
{
    if (!m_invalidFormControls.contains(formControlElement))
        return;

    std::optional<Style::PseudoClassChangeInvalidation> styleInvalidation;
    if (m_invalidFormControls.computeSize() == 1)
        emplace(styleInvalidation, *this, { { CSSSelector::PseudoClass::Valid, true }, { CSSSelector::PseudoClass::Invalid, false } });

    m_invalidFormControls.remove(formControlElement);
}

bool HTMLFormElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name() == actionAttr || HTMLElement::isURLAttribute(attribute);
}

void HTMLFormElement::registerImgElement(HTMLImageElement& element)
{
    ASSERT(m_imageElements.find(&element) == notFound);
    m_imageElements.append(&element);
}

void HTMLFormElement::unregisterImgElement(HTMLImageElement& element)
{
    removeFromPastNamesMap(element);
    bool removed = m_imageElements.removeFirst(&element);
    ASSERT_UNUSED(removed, removed);
}

Ref<HTMLFormControlsCollection> HTMLFormElement::elements()
{
    // Ordinarily JS wrapper keeps the collection alive but this function is used by HTMLFormElement::namedElements internally without creating one.
    // This cache is cleared whenever this element is disconnected from a document.
    if (!m_controlsCollection) {
        Ref controlsCollection = ensureRareData().ensureNodeLists().addCachedCollection<HTMLFormControlsCollection>(*this, CollectionType::FormControls);
        if (!isConnected())
            return controlsCollection;
        m_controlsCollection = WTFMove(controlsCollection);
    }
    return *m_controlsCollection;
}

Ref<HTMLCollection> HTMLFormElement::elementsForNativeBindings()
{
    return elements();
}

String HTMLFormElement::name() const
{
    return getNameAttribute();
}

bool HTMLFormElement::noValidate() const
{
    return hasAttributeWithoutSynchronization(novalidateAttr);
}

String HTMLFormElement::action() const
{
    auto& value = attributeWithoutSynchronization(actionAttr);
    if (value.isEmpty())
        return document().url().string();
    return document().completeURL(value).string();
}

String HTMLFormElement::method() const
{
    return FormSubmission::Attributes::methodString(m_attributes.method());
}

DOMTokenList& HTMLFormElement::relList()
{
    if (!m_relList) {
        lazyInitialize(m_relList, makeUniqueWithoutRefCountedCheck<DOMTokenList>(*this, HTMLNames::relAttr, [](Document&, StringView token) {
            return equalLettersIgnoringASCIICase(token, "noreferrer"_s) || equalLettersIgnoringASCIICase(token, "noopener"_s) || equalLettersIgnoringASCIICase(token, "opener"_s);
        }));
    }
    return *m_relList;
}

AtomString HTMLFormElement::target() const
{
    return attributeWithoutSynchronization(targetAttr);
}

AtomString HTMLFormElement::effectiveTarget(const Event* event, HTMLFormControlElement* overrideSubmitter) const
{
    if (RefPtr submitter = overrideSubmitter ? overrideSubmitter : findSubmitter(event)) {
        auto& targetValue = submitter->attributeWithoutSynchronization(formtargetAttr);
        if (!targetValue.isNull())
            return makeTargetBlankIfHasDanglingMarkup(targetValue);
    }

    auto targetValue = target();
    if (!targetValue.isNull())
        return makeTargetBlankIfHasDanglingMarkup(targetValue);

    return document().baseTarget();
}

bool HTMLFormElement::wasUserSubmitted() const
{
    return m_wasUserSubmitted;
}

HTMLFormControlElement* HTMLFormElement::findSubmitter(const Event* event) const
{
    if (!event)
        return nullptr;
    auto* node = dynamicDowncast<Node>(event->target());
    if (!node)
        return nullptr;
    auto* element = dynamicDowncast<Element>(*node);
    if (!element)
        element = node->parentElement();
    return element ? lineageOfType<HTMLFormControlElement>(*element).first() : nullptr;
}

HTMLFormControlElement* HTMLFormElement::defaultButton() const
{
    if (m_defaultButton)
        return m_defaultButton.get();
    for (auto& listedElement : m_listedElements) {
        if (auto* control = dynamicDowncast<HTMLFormControlElement>(*listedElement); control && control->isSuccessfulSubmitButton()) {
            m_defaultButton = *control;
            return control;
        }
    }
    return nullptr;
}

void HTMLFormElement::resetDefaultButton()
{
    if (!m_defaultButton) {
        // Computing the default button is not cheap, we don't want to do it unless needed.
        // If there was no default button set, the only style to invalidate is the element
        // being added to the form. This is done explicitly in registerFormElement().
        return;
    }

    ScriptDisallowedScope::InMainThread scriptDisallowedScope;

    auto oldDefault = WTFMove(m_defaultButton);
    defaultButton();
    if (m_defaultButton != oldDefault) {
        if (oldDefault)
            oldDefault->invalidateStyleForSubtree();
        if (m_defaultButton)
            m_defaultButton->invalidateStyleForSubtree();
    }
}

bool HTMLFormElement::checkValidity()
{
    Vector<RefPtr<ValidatedFormListedElement>> controls;
    return !checkInvalidControlsAndCollectUnhandled(controls);
}

bool HTMLFormElement::checkInvalidControlsAndCollectUnhandled(Vector<RefPtr<ValidatedFormListedElement>>& unhandledInvalidControls)
{
    Ref<HTMLFormElement> protectedThis(*this);
    // Copy m_listedElements because event handlers called from HTMLFormControlElement::checkValidity() might change m_listedElements.
    bool hasInvalidControls = false;
    for (auto& control : copyValidatedListedElementsVector()) {
        // checkValidity() can trigger events that change the DOM hence why we check for control->form() twice.
        if (control->form() == this && !control->checkValidity(&unhandledInvalidControls) && control->form() == this)
            hasInvalidControls = true;
    }
    return hasInvalidControls;
}

bool HTMLFormElement::reportValidity()
{
    Ref<HTMLFormElement> protectedThis(*this);

    // Update layout before processing form actions in case the style changes
    // the form or button relationships.
    protectedDocument()->updateLayoutIgnorePendingStylesheets();

    return validateInteractively();
}

#if ASSERT_ENABLED
void HTMLFormElement::assertItemCanBeInPastNamesMap(FormAssociatedElement& item) const
{
    ASSERT(item.form() == this);
    HTMLElement& element = item.asHTMLElement();

    if (item.isFormListedElement()) {
        ASSERT(m_listedElements.find(&element) != notFound);
        return;
    }

    ASSERT(element.hasTagName(imgTag));
    ASSERT(m_imageElements.find(&downcast<HTMLImageElement>(element)) != notFound);
}
#endif

RefPtr<HTMLElement> HTMLFormElement::elementFromPastNamesMap(const AtomString& pastName) const
{
    if (pastName.isEmpty() || m_pastNamesMap.isEmpty())
        return nullptr;
    auto weakElement = m_pastNamesMap.get(pastName);
    if (!weakElement)
        return nullptr;
    RefPtr element { weakElement.get() };
#if ASSERT_ENABLED
    assertItemCanBeInPastNamesMap(*element->asFormAssociatedElement());
#endif
    return element;
}

void HTMLFormElement::addToPastNamesMap(FormAssociatedElement& item, const AtomString& pastName)
{
#if ASSERT_ENABLED
    assertItemCanBeInPastNamesMap(item);
#endif
    if (pastName.isEmpty())
        return;
    m_pastNamesMap.set(pastName.impl(), item.asHTMLElement());
}

void HTMLFormElement::removeFromPastNamesMap(FormAssociatedElement& item)
{
    if (m_pastNamesMap.isEmpty())
        return;

    m_pastNamesMap.removeIf([&element = item.asHTMLElement()] (auto& iterator) {
        return iterator.value == &element;
    });
}

bool HTMLFormElement::matchesValidPseudoClass() const
{
    return m_invalidFormControls.isEmptyIgnoringNullReferences();
}

bool HTMLFormElement::matchesInvalidPseudoClass() const
{
    return !matchesValidPseudoClass();
}

// FIXME: Use Ref<HTMLElement> for the function result since there are no non-HTML elements returned here.
Vector<Ref<Element>> HTMLFormElement::namedElements(const AtomString& name)
{
    if (name.isEmpty())
        return { };

    // http://www.whatwg.org/specs/web-apps/current-work/multipage/forms.html#dom-form-nameditem
    Vector<Ref<Element>> namedItems = elements()->namedItems(name);

    auto elementFromPast = elementFromPastNamesMap(name);
    if (namedItems.size() == 1 && namedItems.first().ptr() != elementFromPast)
        addToPastNamesMap(*downcast<HTMLElement>(namedItems.first().get()).asFormAssociatedElement(), name);
    else if (elementFromPast && namedItems.isEmpty())
        namedItems.append(*elementFromPast);

    return namedItems;
}

bool HTMLFormElement::isSupportedPropertyName(const AtomString& name)
{
    return !name.isEmpty() && elements()->isSupportedPropertyName(name);
}

void HTMLFormElement::resumeFromDocumentSuspension()
{
    ASSERT(!shouldAutocomplete());

    document().postTask([formElement = Ref { *this }] (ScriptExecutionContext&) {
        formElement->resetListedFormControlElements();
    });
}

void HTMLFormElement::didMoveToNewDocument(Document& oldDocument, Document& newDocument)
{
    if (!shouldAutocomplete()) {
        oldDocument.unregisterForDocumentSuspensionCallbacks(*this);
        newDocument.registerForDocumentSuspensionCallbacks(*this);
    }

    HTMLElement::didMoveToNewDocument(oldDocument, newDocument);
}

bool HTMLFormElement::shouldAutocomplete() const
{
    return !equalLettersIgnoringASCIICase(attributeWithoutSynchronization(autocompleteAttr), "off"_s);
}

void HTMLFormElement::finishParsingChildren()
{
    HTMLElement::finishParsingChildren();
    document().formController().restoreControlStateIn(*this);
}

const Vector<WeakPtr<HTMLElement, WeakPtrImplWithEventTargetData>>& HTMLFormElement::unsafeListedElements() const
{
    ASSERT(ScriptDisallowedScope::InMainThread::hasDisallowedScope());
    return m_listedElements;
}

Vector<Ref<FormListedElement>> HTMLFormElement::copyListedElementsVector() const
{
    return WTF::map(m_listedElements, [] (auto& weakElement) {
        RefPtr element { weakElement.get() };
        ASSERT(element);
        auto* formListedElement = element->asFormListedElement();
        ASSERT(formListedElement);
        return Ref<FormListedElement>(*formListedElement);
    });
}

Vector<Ref<ValidatedFormListedElement>> HTMLFormElement::copyValidatedListedElementsVector() const
{
    return WTF::compactMap(m_listedElements, [](auto& weakElement) {
        return RefPtr { weakElement->asValidatedFormListedElement() };
    });
}

HTMLFormElement* HTMLFormElement::findClosestFormAncestor(const Element& startElement)
{
    return const_cast<HTMLFormElement*>(ancestorsOfType<HTMLFormElement>(startElement).first());
}

const AtomString& HTMLFormElement::autocomplete() const
{
    return equalLettersIgnoringASCIICase(attributeWithoutSynchronization(autocompleteAttr), "off"_s) ? offAtom() : onAtom();
}

RefPtr<DOMFormData> HTMLFormElement::constructEntryList(RefPtr<HTMLFormControlElement>&& submitter, Ref<DOMFormData>&& domFormData, StringPairVector* formValues)
{
    // https://html.spec.whatwg.org/multipage/form-control-infrastructure.html#constructing-form-data-set
    ASSERT(isMainThread());
    
    if (m_isConstructingEntryList)
        return nullptr;
    
    SetForScope isConstructingEntryListScope(m_isConstructingEntryList, true);

    if (submitter)
        submitter->setActivatedSubmit(true);
    
    for (auto& control : this->copyListedElementsVector()) {
        auto& element = control->asHTMLElement();
        if (!element.isDisabledFormControl())
            control->appendFormData(domFormData.get());
        if (RefPtr input = dynamicDowncast<HTMLInputElement>(element); formValues && input) {
            if (input->isTextField()) {
                formValues->append({ input->name(), input->value() });
                input->addSearchResult();
            }
        }
    }
    
    dispatchEvent(FormDataEvent::create(eventNames().formdataEvent, Event::CanBubble::Yes, Event::IsCancelable::No, Event::IsComposed::No, domFormData.copyRef()));

    if (submitter)
        submitter->setActivatedSubmit(false);
    
    return domFormData->clone();
}

} // namespace
