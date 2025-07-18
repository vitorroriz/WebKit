/*
    This file is part of the WebKit open source project.
    This file has been generated by generate-bindings.pl. DO NOT MODIFY!

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "JSTestNamedAndIndexedSetterThrowingException.h"

#include "ActiveDOMObject.h"
#include "ContextDestructionObserverInlines.h"
#include "Document.h"
#include "DocumentInlines.h"
#include "ExtendedDOMClientIsoSubspaces.h"
#include "ExtendedDOMIsoSubspaces.h"
#include "JSDOMAbstractOperations.h"
#include "JSDOMBinding.h"
#include "JSDOMConstructorNotConstructable.h"
#include "JSDOMConvertStrings.h"
#include "JSDOMExceptionHandling.h"
#include "JSDOMGlobalObjectInlines.h"
#include "JSDOMWrapperCache.h"
#include "Quirks.h"
#include "ScriptExecutionContext.h"
#include "WebCoreJSClientData.h"
#include <JavaScriptCore/FunctionPrototype.h>
#include <JavaScriptCore/HeapAnalyzer.h>
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/JSDestructibleObjectHeapCellType.h>
#include <JavaScriptCore/PropertyNameArray.h>
#include <JavaScriptCore/SlotVisitorMacros.h>
#include <JavaScriptCore/SubspaceInlines.h>
#include <wtf/GetPtr.h>
#include <wtf/PointerPreparations.h>
#include <wtf/URL.h>
#include <wtf/text/MakeString.h>

namespace WebCore {
using namespace JSC;

// Attributes

static JSC_DECLARE_CUSTOM_GETTER(jsTestNamedAndIndexedSetterThrowingExceptionConstructor);

class JSTestNamedAndIndexedSetterThrowingExceptionPrototype final : public JSC::JSNonFinalObject {
public:
    using Base = JSC::JSNonFinalObject;
    static JSTestNamedAndIndexedSetterThrowingExceptionPrototype* create(JSC::VM& vm, JSDOMGlobalObject* globalObject, JSC::Structure* structure)
    {
        JSTestNamedAndIndexedSetterThrowingExceptionPrototype* ptr = new (NotNull, JSC::allocateCell<JSTestNamedAndIndexedSetterThrowingExceptionPrototype>(vm)) JSTestNamedAndIndexedSetterThrowingExceptionPrototype(vm, globalObject, structure);
        ptr->finishCreation(vm);
        return ptr;
    }

    DECLARE_INFO;
    template<typename CellType, JSC::SubspaceAccess>
    static JSC::GCClient::IsoSubspace* subspaceFor(JSC::VM& vm)
    {
        STATIC_ASSERT_ISO_SUBSPACE_SHARABLE(JSTestNamedAndIndexedSetterThrowingExceptionPrototype, Base);
        return &vm.plainObjectSpace();
    }
    static JSC::Structure* createStructure(JSC::VM& vm, JSC::JSGlobalObject* globalObject, JSC::JSValue prototype)
    {
        return JSC::Structure::create(vm, globalObject, prototype, JSC::TypeInfo(JSC::ObjectType, StructureFlags), info());
    }

private:
    JSTestNamedAndIndexedSetterThrowingExceptionPrototype(JSC::VM& vm, JSC::JSGlobalObject*, JSC::Structure* structure)
        : JSC::JSNonFinalObject(vm, structure)
    {
    }

    void finishCreation(JSC::VM&);
};
STATIC_ASSERT_ISO_SUBSPACE_SHARABLE(JSTestNamedAndIndexedSetterThrowingExceptionPrototype, JSTestNamedAndIndexedSetterThrowingExceptionPrototype::Base);

using JSTestNamedAndIndexedSetterThrowingExceptionDOMConstructor = JSDOMConstructorNotConstructable<JSTestNamedAndIndexedSetterThrowingException>;

template<> const ClassInfo JSTestNamedAndIndexedSetterThrowingExceptionDOMConstructor::s_info = { "TestNamedAndIndexedSetterThrowingException"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSTestNamedAndIndexedSetterThrowingExceptionDOMConstructor) };

template<> JSValue JSTestNamedAndIndexedSetterThrowingExceptionDOMConstructor::prototypeForStructure(JSC::VM& vm, const JSDOMGlobalObject& globalObject)
{
    UNUSED_PARAM(vm);
    return globalObject.functionPrototype();
}

template<> void JSTestNamedAndIndexedSetterThrowingExceptionDOMConstructor::initializeProperties(VM& vm, JSDOMGlobalObject& globalObject)
{
    putDirect(vm, vm.propertyNames->length, jsNumber(0), JSC::PropertyAttribute::ReadOnly | JSC::PropertyAttribute::DontEnum);
    JSString* nameString = jsNontrivialString(vm, "TestNamedAndIndexedSetterThrowingException"_s);
    m_originalName.set(vm, this, nameString);
    putDirect(vm, vm.propertyNames->name, nameString, JSC::PropertyAttribute::ReadOnly | JSC::PropertyAttribute::DontEnum);
    putDirect(vm, vm.propertyNames->prototype, JSTestNamedAndIndexedSetterThrowingException::prototype(vm, globalObject), JSC::PropertyAttribute::ReadOnly | JSC::PropertyAttribute::DontEnum | JSC::PropertyAttribute::DontDelete);
}

/* Hash table for prototype */

static const std::array<HashTableValue, 1> JSTestNamedAndIndexedSetterThrowingExceptionPrototypeTableValues {
    HashTableValue { "constructor"_s, static_cast<unsigned>(PropertyAttribute::DontEnum), NoIntrinsic, { HashTableValue::GetterSetterType, jsTestNamedAndIndexedSetterThrowingExceptionConstructor, 0 } },
};

const ClassInfo JSTestNamedAndIndexedSetterThrowingExceptionPrototype::s_info = { "TestNamedAndIndexedSetterThrowingException"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSTestNamedAndIndexedSetterThrowingExceptionPrototype) };

void JSTestNamedAndIndexedSetterThrowingExceptionPrototype::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    reifyStaticProperties(vm, JSTestNamedAndIndexedSetterThrowingException::info(), JSTestNamedAndIndexedSetterThrowingExceptionPrototypeTableValues, *this);
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

const ClassInfo JSTestNamedAndIndexedSetterThrowingException::s_info = { "TestNamedAndIndexedSetterThrowingException"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSTestNamedAndIndexedSetterThrowingException) };

JSTestNamedAndIndexedSetterThrowingException::JSTestNamedAndIndexedSetterThrowingException(Structure* structure, JSDOMGlobalObject& globalObject, Ref<TestNamedAndIndexedSetterThrowingException>&& impl)
    : JSDOMWrapper<TestNamedAndIndexedSetterThrowingException>(structure, globalObject, WTFMove(impl))
{
}

static_assert(!std::is_base_of<ActiveDOMObject, TestNamedAndIndexedSetterThrowingException>::value, "Interface is not marked as [ActiveDOMObject] even though implementation class subclasses ActiveDOMObject.");

JSObject* JSTestNamedAndIndexedSetterThrowingException::createPrototype(VM& vm, JSDOMGlobalObject& globalObject)
{
    auto* structure = JSTestNamedAndIndexedSetterThrowingExceptionPrototype::createStructure(vm, &globalObject, globalObject.objectPrototype());
    structure->setMayBePrototype(true);
    return JSTestNamedAndIndexedSetterThrowingExceptionPrototype::create(vm, &globalObject, structure);
}

JSObject* JSTestNamedAndIndexedSetterThrowingException::prototype(VM& vm, JSDOMGlobalObject& globalObject)
{
    return getDOMPrototype<JSTestNamedAndIndexedSetterThrowingException>(vm, globalObject);
}

JSValue JSTestNamedAndIndexedSetterThrowingException::getConstructor(VM& vm, const JSGlobalObject* globalObject)
{
    return getDOMConstructor<JSTestNamedAndIndexedSetterThrowingExceptionDOMConstructor, DOMConstructorID::TestNamedAndIndexedSetterThrowingException>(vm, *jsCast<const JSDOMGlobalObject*>(globalObject));
}

void JSTestNamedAndIndexedSetterThrowingException::destroy(JSC::JSCell* cell)
{
    JSTestNamedAndIndexedSetterThrowingException* thisObject = static_cast<JSTestNamedAndIndexedSetterThrowingException*>(cell);
    thisObject->JSTestNamedAndIndexedSetterThrowingException::~JSTestNamedAndIndexedSetterThrowingException();
}

bool JSTestNamedAndIndexedSetterThrowingException::legacyPlatformObjectGetOwnProperty(JSObject* object, JSGlobalObject* lexicalGlobalObject, PropertyName propertyName, PropertySlot& slot, bool ignoreNamedProperties)
{
    auto throwScope = DECLARE_THROW_SCOPE(JSC::getVM(lexicalGlobalObject));
    auto* thisObject = jsCast<JSTestNamedAndIndexedSetterThrowingException*>(object);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    if (auto index = parseIndex(propertyName)) {
        if (auto item = thisObject->wrapped().item(index.value()); !!item) [[likely]] {
            auto value = toJS<IDLDOMString>(*lexicalGlobalObject, throwScope, WTFMove(item));
            RETURN_IF_EXCEPTION(throwScope, false);
            slot.setValue(thisObject, 0, value);
            return true;
        }
        return JSObject::getOwnPropertySlot(object, lexicalGlobalObject, propertyName, slot);
    }
    if (!ignoreNamedProperties) {
        using GetterIDLType = IDLDOMString;
        auto getterFunctor = visibleNamedPropertyItemAccessorFunctor<GetterIDLType, JSTestNamedAndIndexedSetterThrowingException>([] (JSTestNamedAndIndexedSetterThrowingException& thisObject, PropertyName propertyName) -> decltype(auto) {
            return thisObject.wrapped().namedItem(propertyNameToAtomString(propertyName));
        });
        if (auto namedProperty = accessVisibleNamedProperty<LegacyOverrideBuiltIns::No>(*lexicalGlobalObject, *thisObject, propertyName, getterFunctor)) {
            auto value = toJS<IDLDOMString>(*lexicalGlobalObject, throwScope, WTFMove(namedProperty.value()));
            RETURN_IF_EXCEPTION(throwScope, false);
            slot.setValue(thisObject, 0, value);
            return true;
        }
    }
    return JSObject::getOwnPropertySlot(object, lexicalGlobalObject, propertyName, slot);
}

bool JSTestNamedAndIndexedSetterThrowingException::getOwnPropertySlot(JSObject* object, JSGlobalObject* lexicalGlobalObject, PropertyName propertyName, PropertySlot& slot)
{
    bool ignoreNamedProperties = false;
    return legacyPlatformObjectGetOwnProperty(object, lexicalGlobalObject, propertyName, slot, ignoreNamedProperties);
}

bool JSTestNamedAndIndexedSetterThrowingException::getOwnPropertySlotByIndex(JSObject* object, JSGlobalObject* lexicalGlobalObject, unsigned index, PropertySlot& slot)
{
    SUPPRESS_UNCOUNTED_LOCAL auto& vm = JSC::getVM(lexicalGlobalObject);
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    auto* thisObject = jsCast<JSTestNamedAndIndexedSetterThrowingException*>(object);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    if (index <= MAX_ARRAY_INDEX) [[likely]] {
        if (auto item = thisObject->wrapped().item(index); !!item) [[likely]] {
            auto value = toJS<IDLDOMString>(*lexicalGlobalObject, throwScope, WTFMove(item));
            RETURN_IF_EXCEPTION(throwScope, false);
            slot.setValue(thisObject, 0, value);
            return true;
        }
        return JSObject::getOwnPropertySlotByIndex(object, lexicalGlobalObject, index, slot);
    }
    auto propertyName = Identifier::from(vm, index);
    using GetterIDLType = IDLDOMString;
    auto getterFunctor = visibleNamedPropertyItemAccessorFunctor<GetterIDLType, JSTestNamedAndIndexedSetterThrowingException>([] (JSTestNamedAndIndexedSetterThrowingException& thisObject, PropertyName propertyName) -> decltype(auto) {
        return thisObject.wrapped().namedItem(propertyNameToAtomString(propertyName));
    });
    if (auto namedProperty = accessVisibleNamedProperty<LegacyOverrideBuiltIns::No>(*lexicalGlobalObject, *thisObject, propertyName, getterFunctor)) {
        auto value = toJS<IDLDOMString>(*lexicalGlobalObject, throwScope, WTFMove(namedProperty.value()));
        RETURN_IF_EXCEPTION(throwScope, false);
        slot.setValue(thisObject, 0, value);
        return true;
    }
    return JSObject::getOwnPropertySlotByIndex(object, lexicalGlobalObject, index, slot);
}

void JSTestNamedAndIndexedSetterThrowingException::getOwnPropertyNames(JSObject* object, JSGlobalObject* lexicalGlobalObject, PropertyNameArray& propertyNames, DontEnumPropertiesMode mode)
{
    SUPPRESS_UNCOUNTED_LOCAL auto& vm = JSC::getVM(lexicalGlobalObject);
    auto* thisObject = jsCast<JSTestNamedAndIndexedSetterThrowingException*>(object);
    ASSERT_GC_OBJECT_INHERITS(object, info());
    for (unsigned i = 0, count = thisObject->wrapped().length(); i < count; ++i)
        propertyNames.add(Identifier::from(vm, i));
    for (auto& propertyName : thisObject->wrapped().supportedPropertyNames())
        propertyNames.add(Identifier::fromString(vm, propertyName));
    JSObject::getOwnPropertyNames(object, lexicalGlobalObject, propertyNames, mode);
}

bool JSTestNamedAndIndexedSetterThrowingException::put(JSCell* cell, JSGlobalObject* lexicalGlobalObject, PropertyName propertyName, JSValue value, PutPropertySlot& putPropertySlot)
{
    auto* thisObject = jsCast<JSTestNamedAndIndexedSetterThrowingException*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    if (thisObject != putPropertySlot.thisValue()) [[unlikely]]
        return JSObject::put(thisObject, lexicalGlobalObject, propertyName, value, putPropertySlot);
    auto throwScope = DECLARE_THROW_SCOPE(lexicalGlobalObject->vm());

    if (auto index = parseIndex(propertyName)) {
        auto nativeValue = convert<IDLDOMString>(*lexicalGlobalObject, value);
        if (nativeValue.hasException(throwScope)) [[unlikely]]
            return true;
        invokeFunctorPropagatingExceptionIfNecessary(*lexicalGlobalObject, throwScope, [&] { return thisObject->wrapped().setItem(index.value(), nativeValue.releaseReturnValue()); });
        return true;
    }

    if (!propertyName.isSymbol()) {
        PropertySlot slot { thisObject, PropertySlot::InternalMethodType::VMInquiry, &lexicalGlobalObject->vm() };
        JSValue prototype = thisObject->getPrototypeDirect();
        bool found = prototype.isObject() && asObject(prototype)->getPropertySlot(lexicalGlobalObject, propertyName, slot);
        slot.disallowVMEntry.reset();
        RETURN_IF_EXCEPTION(throwScope, false);
        if (!found) {
            auto nativeValue = convert<IDLDOMString>(*lexicalGlobalObject, value);
            if (nativeValue.hasException(throwScope)) [[unlikely]]
                return true;
            invokeFunctorPropagatingExceptionIfNecessary(*lexicalGlobalObject, throwScope, [&] { return thisObject->wrapped().setNamedItem(propertyNameToString(propertyName), nativeValue.releaseReturnValue()); });
            return true;
        }
    }

    throwScope.assertNoException();
    PropertyDescriptor ownDescriptor;
    PropertySlot slot(thisObject, PropertySlot::InternalMethodType::GetOwnProperty);;
    bool ignoreNamedProperties = true;
    bool hasOwnProperty = legacyPlatformObjectGetOwnProperty(thisObject, lexicalGlobalObject, propertyName, slot, ignoreNamedProperties);
    RETURN_IF_EXCEPTION(throwScope, false);
    if (hasOwnProperty) {
        ownDescriptor.setPropertySlot(lexicalGlobalObject, propertyName, slot);
        RETURN_IF_EXCEPTION(throwScope, false);
    }
    RELEASE_AND_RETURN(throwScope, ordinarySetWithOwnDescriptor(lexicalGlobalObject, thisObject, propertyName, value, putPropertySlot.thisValue(), WTFMove(ownDescriptor), putPropertySlot.isStrictMode()));
}

bool JSTestNamedAndIndexedSetterThrowingException::putByIndex(JSCell* cell, JSGlobalObject* lexicalGlobalObject, unsigned index, JSValue value, bool shouldThrow)
{
    auto* thisObject = jsCast<JSTestNamedAndIndexedSetterThrowingException*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    SUPPRESS_UNCOUNTED_LOCAL auto& vm = JSC::getVM(lexicalGlobalObject);
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    if (index <= MAX_ARRAY_INDEX) [[likely]] {
        auto nativeValue = convert<IDLDOMString>(*lexicalGlobalObject, value);
        if (nativeValue.hasException(throwScope)) [[unlikely]]
            return true;
        invokeFunctorPropagatingExceptionIfNecessary(*lexicalGlobalObject, throwScope, [&] { return thisObject->wrapped().setItem(index, nativeValue.releaseReturnValue()); });
        return true;
    }

    auto propertyName = Identifier::from(vm, index);
    PropertySlot slot { thisObject, PropertySlot::InternalMethodType::VMInquiry, &vm };
    JSValue prototype = thisObject->getPrototypeDirect();
    bool found = prototype.isObject() && asObject(prototype)->getPropertySlot(lexicalGlobalObject, propertyName, slot);
    slot.disallowVMEntry.reset();
    RETURN_IF_EXCEPTION(throwScope, false);
    if (!found) {
        auto nativeValue = convert<IDLDOMString>(*lexicalGlobalObject, value);
        if (nativeValue.hasException(throwScope)) [[unlikely]]
            return true;
        invokeFunctorPropagatingExceptionIfNecessary(*lexicalGlobalObject, throwScope, [&] { return thisObject->wrapped().setNamedItem(propertyNameToString(propertyName), nativeValue.releaseReturnValue()); });
        return true;
    }

    throwScope.assertNoException();
    PutPropertySlot putPropertySlot(thisObject, shouldThrow);
    RELEASE_AND_RETURN(throwScope, ordinarySetSlow(lexicalGlobalObject, thisObject, propertyName, value, putPropertySlot.thisValue(), shouldThrow));
}

bool JSTestNamedAndIndexedSetterThrowingException::defineOwnProperty(JSObject* object, JSGlobalObject* lexicalGlobalObject, PropertyName propertyName, const PropertyDescriptor& propertyDescriptor, bool shouldThrow)
{
    auto* thisObject = jsCast<JSTestNamedAndIndexedSetterThrowingException*>(object);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    auto throwScope = DECLARE_THROW_SCOPE(lexicalGlobalObject->vm());

    if (auto index = parseIndex(propertyName)) {
        if (!propertyDescriptor.isDataDescriptor())
            return typeError(lexicalGlobalObject, throwScope, shouldThrow, "Cannot set indexed properties on this object"_s);
        auto nativeValue = convert<IDLDOMString>(*lexicalGlobalObject, propertyDescriptor.value());
        if (nativeValue.hasException(throwScope)) [[unlikely]]
            return true;
        invokeFunctorPropagatingExceptionIfNecessary(*lexicalGlobalObject, throwScope, [&] { return thisObject->wrapped().setItem(index.value(), nativeValue.releaseReturnValue()); });
        return true;
    }

    if (!propertyName.isSymbol()) {
        PropertySlot slot { thisObject, PropertySlot::InternalMethodType::VMInquiry, &lexicalGlobalObject->vm() };
        bool found = JSObject::getOwnPropertySlot(thisObject, lexicalGlobalObject, propertyName, slot);
        slot.disallowVMEntry.reset();
        RETURN_IF_EXCEPTION(throwScope, false);
        if (!found) {
            if (!propertyDescriptor.isDataDescriptor())
                return typeError(lexicalGlobalObject, throwScope, shouldThrow, "Cannot set named properties on this object"_s);
            auto nativeValue = convert<IDLDOMString>(*lexicalGlobalObject, propertyDescriptor.value());
            if (nativeValue.hasException(throwScope)) [[unlikely]]
                return true;
            invokeFunctorPropagatingExceptionIfNecessary(*lexicalGlobalObject, throwScope, [&] { return thisObject->wrapped().setNamedItem(propertyNameToString(propertyName), nativeValue.releaseReturnValue()); });
            return true;
        }
    }

    PropertyDescriptor newPropertyDescriptor = propertyDescriptor;
    throwScope.release();
    return JSObject::defineOwnProperty(object, lexicalGlobalObject, propertyName, newPropertyDescriptor, shouldThrow);
}

bool JSTestNamedAndIndexedSetterThrowingException::deleteProperty(JSCell* cell, JSGlobalObject* lexicalGlobalObject, PropertyName propertyName, DeletePropertySlot& slot)
{
    auto& thisObject = *jsCast<JSTestNamedAndIndexedSetterThrowingException*>(cell);
    SUPPRESS_UNCOUNTED_LOCAL auto& impl = thisObject.wrapped();

    // Temporary quirk for ungap/@custom-elements polyfill (rdar://problem/111008826), consider removing in 2025.
    if (auto* document = dynamicDowncast<Document>(jsDynamicCast<JSDOMGlobalObject*>(lexicalGlobalObject)->scriptExecutionContext())) {
        if (document->quirks().needsConfigurableIndexedPropertiesQuirk()) [[unlikely]]
            return JSObject::deleteProperty(cell, lexicalGlobalObject, propertyName, slot);
    }

    if (auto index = parseIndex(propertyName))
        return !impl.isSupportedPropertyIndex(index.value());
    if (!propertyName.isSymbol() && impl.isSupportedPropertyName(propertyNameToString(propertyName))) {
        PropertySlot slotForGet { &thisObject, PropertySlot::InternalMethodType::VMInquiry, &lexicalGlobalObject->vm() };
        if (!JSObject::getOwnPropertySlot(&thisObject, lexicalGlobalObject, propertyName, slotForGet))
            return false;
    }
    return JSObject::deleteProperty(cell, lexicalGlobalObject, propertyName, slot);
}

bool JSTestNamedAndIndexedSetterThrowingException::deletePropertyByIndex(JSCell* cell, JSGlobalObject* lexicalGlobalObject, unsigned index)
{
    UNUSED_PARAM(lexicalGlobalObject);
    auto& thisObject = *jsCast<JSTestNamedAndIndexedSetterThrowingException*>(cell);
    SUPPRESS_UNCOUNTED_LOCAL auto& impl = thisObject.wrapped();

    // Temporary quirk for ungap/@custom-elements polyfill (rdar://problem/111008826), consider removing in 2025.
    if (auto* document = dynamicDowncast<Document>(jsDynamicCast<JSDOMGlobalObject*>(lexicalGlobalObject)->scriptExecutionContext())) {
        if (document->quirks().needsConfigurableIndexedPropertiesQuirk()) [[unlikely]]
            return JSObject::deletePropertyByIndex(cell, lexicalGlobalObject, index);
    }

    return !impl.isSupportedPropertyIndex(index);
}

JSC_DEFINE_CUSTOM_GETTER(jsTestNamedAndIndexedSetterThrowingExceptionConstructor, (JSGlobalObject* lexicalGlobalObject, EncodedJSValue thisValue, PropertyName))
{
    SUPPRESS_UNCOUNTED_LOCAL auto& vm = JSC::getVM(lexicalGlobalObject);
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    auto* prototype = jsDynamicCast<JSTestNamedAndIndexedSetterThrowingExceptionPrototype*>(JSValue::decode(thisValue));
    if (!prototype) [[unlikely]]
        return throwVMTypeError(lexicalGlobalObject, throwScope);
    return JSValue::encode(JSTestNamedAndIndexedSetterThrowingException::getConstructor(vm, prototype->globalObject()));
}

JSC::GCClient::IsoSubspace* JSTestNamedAndIndexedSetterThrowingException::subspaceForImpl(JSC::VM& vm)
{
    return WebCore::subspaceForImpl<JSTestNamedAndIndexedSetterThrowingException, UseCustomHeapCellType::No>(vm, "JSTestNamedAndIndexedSetterThrowingException"_s,
        [] (auto& spaces) { return spaces.m_clientSubspaceForTestNamedAndIndexedSetterThrowingException.get(); },
        [] (auto& spaces, auto&& space) { spaces.m_clientSubspaceForTestNamedAndIndexedSetterThrowingException = std::forward<decltype(space)>(space); },
        [] (auto& spaces) { return spaces.m_subspaceForTestNamedAndIndexedSetterThrowingException.get(); },
        [] (auto& spaces, auto&& space) { spaces.m_subspaceForTestNamedAndIndexedSetterThrowingException = std::forward<decltype(space)>(space); }
    );
}

void JSTestNamedAndIndexedSetterThrowingException::analyzeHeap(JSCell* cell, HeapAnalyzer& analyzer)
{
    auto* thisObject = jsCast<JSTestNamedAndIndexedSetterThrowingException*>(cell);
    analyzer.setWrappedObjectForCell(cell, &thisObject->wrapped());
    if (RefPtr context = thisObject->scriptExecutionContext())
        analyzer.setLabelForCell(cell, makeString("url "_s, context->url().string()));
    Base::analyzeHeap(cell, analyzer);
}

bool JSTestNamedAndIndexedSetterThrowingExceptionOwner::isReachableFromOpaqueRoots(JSC::Handle<JSC::Unknown> handle, void*, AbstractSlotVisitor& visitor, ASCIILiteral* reason)
{
    UNUSED_PARAM(handle);
    UNUSED_PARAM(visitor);
    UNUSED_PARAM(reason);
    return false;
}

void JSTestNamedAndIndexedSetterThrowingExceptionOwner::finalize(JSC::Handle<JSC::Unknown> handle, void* context)
{
    auto* jsTestNamedAndIndexedSetterThrowingException = static_cast<JSTestNamedAndIndexedSetterThrowingException*>(handle.slot()->asCell());
    auto& world = *static_cast<DOMWrapperWorld*>(context);
    uncacheWrapper(world, jsTestNamedAndIndexedSetterThrowingException->protectedWrapped().ptr(), jsTestNamedAndIndexedSetterThrowingException);
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
#if ENABLE(BINDING_INTEGRITY)
#if PLATFORM(WIN)
#pragma warning(disable: 4483)
extern "C" { extern void (*const __identifier("??_7TestNamedAndIndexedSetterThrowingException@WebCore@@6B@")[])(); }
#else
extern "C" { extern void* _ZTVN7WebCore42TestNamedAndIndexedSetterThrowingExceptionE[]; }
#endif
template<std::same_as<TestNamedAndIndexedSetterThrowingException> T>
static inline void verifyVTable(TestNamedAndIndexedSetterThrowingException* ptr) 
{
    if constexpr (std::is_polymorphic_v<T>) {
        const void* actualVTablePointer = getVTablePointer<T>(ptr);
#if PLATFORM(WIN)
        void* expectedVTablePointer = __identifier("??_7TestNamedAndIndexedSetterThrowingException@WebCore@@6B@");
#else
        void* expectedVTablePointer = &_ZTVN7WebCore42TestNamedAndIndexedSetterThrowingExceptionE[2];
#endif

        // If you hit this assertion you either have a use after free bug, or
        // TestNamedAndIndexedSetterThrowingException has subclasses. If TestNamedAndIndexedSetterThrowingException has subclasses that get passed
        // to toJS() we currently require TestNamedAndIndexedSetterThrowingException you to opt out of binding hardening
        // by adding the SkipVTableValidation attribute to the interface IDL definition
        RELEASE_ASSERT(actualVTablePointer == expectedVTablePointer);
    }
}
#endif
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

JSC::JSValue toJSNewlyCreated(JSC::JSGlobalObject*, JSDOMGlobalObject* globalObject, Ref<TestNamedAndIndexedSetterThrowingException>&& impl)
{
#if ENABLE(BINDING_INTEGRITY)
    verifyVTable<TestNamedAndIndexedSetterThrowingException>(impl.ptr());
#endif
    return createWrapper<TestNamedAndIndexedSetterThrowingException>(globalObject, WTFMove(impl));
}

JSC::JSValue toJS(JSC::JSGlobalObject* lexicalGlobalObject, JSDOMGlobalObject* globalObject, TestNamedAndIndexedSetterThrowingException& impl)
{
    return wrap(lexicalGlobalObject, globalObject, impl);
}

TestNamedAndIndexedSetterThrowingException* JSTestNamedAndIndexedSetterThrowingException::toWrapped(JSC::VM&, JSC::JSValue value)
{
    if (auto* wrapper = jsDynamicCast<JSTestNamedAndIndexedSetterThrowingException*>(value))
        return &wrapper->wrapped();
    return nullptr;
}

}
