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
#include "JSTestOverloadedConstructors.h"

#include "ActiveDOMObject.h"
#include "ContextDestructionObserverInlines.h"
#include "ExtendedDOMClientIsoSubspaces.h"
#include "ExtendedDOMIsoSubspaces.h"
#include "JSBlob.h"
#include "JSDOMBinding.h"
#include "JSDOMConstructor.h"
#include "JSDOMConvertBufferSource.h"
#include "JSDOMConvertInterface.h"
#include "JSDOMConvertNumbers.h"
#include "JSDOMConvertStrings.h"
#include "JSDOMConvertUnion.h"
#include "JSDOMConvertVariadic.h"
#include "JSDOMExceptionHandling.h"
#include "JSDOMGlobalObjectInlines.h"
#include "JSDOMWrapperCache.h"
#include "ScriptExecutionContext.h"
#include "WebCoreJSClientData.h"
#include <JavaScriptCore/FunctionPrototype.h>
#include <JavaScriptCore/HeapAnalyzer.h>
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/JSDestructibleObjectHeapCellType.h>
#include <JavaScriptCore/SlotVisitorMacros.h>
#include <JavaScriptCore/SubspaceInlines.h>
#include <wtf/GetPtr.h>
#include <wtf/PointerPreparations.h>
#include <wtf/URL.h>
#include <wtf/text/MakeString.h>

namespace WebCore {
using namespace JSC;

// Attributes

static JSC_DECLARE_CUSTOM_GETTER(jsTestOverloadedConstructorsConstructor);

class JSTestOverloadedConstructorsPrototype final : public JSC::JSNonFinalObject {
public:
    using Base = JSC::JSNonFinalObject;
    static JSTestOverloadedConstructorsPrototype* create(JSC::VM& vm, JSDOMGlobalObject* globalObject, JSC::Structure* structure)
    {
        JSTestOverloadedConstructorsPrototype* ptr = new (NotNull, JSC::allocateCell<JSTestOverloadedConstructorsPrototype>(vm)) JSTestOverloadedConstructorsPrototype(vm, globalObject, structure);
        ptr->finishCreation(vm);
        return ptr;
    }

    DECLARE_INFO;
    template<typename CellType, JSC::SubspaceAccess>
    static JSC::GCClient::IsoSubspace* subspaceFor(JSC::VM& vm)
    {
        STATIC_ASSERT_ISO_SUBSPACE_SHARABLE(JSTestOverloadedConstructorsPrototype, Base);
        return &vm.plainObjectSpace();
    }
    static JSC::Structure* createStructure(JSC::VM& vm, JSC::JSGlobalObject* globalObject, JSC::JSValue prototype)
    {
        return JSC::Structure::create(vm, globalObject, prototype, JSC::TypeInfo(JSC::ObjectType, StructureFlags), info());
    }

private:
    JSTestOverloadedConstructorsPrototype(JSC::VM& vm, JSC::JSGlobalObject*, JSC::Structure* structure)
        : JSC::JSNonFinalObject(vm, structure)
    {
    }

    void finishCreation(JSC::VM&);
};
STATIC_ASSERT_ISO_SUBSPACE_SHARABLE(JSTestOverloadedConstructorsPrototype, JSTestOverloadedConstructorsPrototype::Base);

using JSTestOverloadedConstructorsDOMConstructor = JSDOMConstructor<JSTestOverloadedConstructors>;

static inline EncodedJSValue constructJSTestOverloadedConstructors1(JSGlobalObject* lexicalGlobalObject, CallFrame* callFrame)
{
    SUPPRESS_UNCOUNTED_LOCAL auto& vm = lexicalGlobalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    auto* castedThis = jsCast<JSTestOverloadedConstructorsDOMConstructor*>(callFrame->jsCallee());
    ASSERT(castedThis);
    EnsureStillAliveScope argument0 = callFrame->uncheckedArgument(0);
    auto arrayBufferConversionResult = convert<IDLArrayBuffer>(*lexicalGlobalObject, argument0.value(), [](JSC::JSGlobalObject& lexicalGlobalObject, JSC::ThrowScope& scope) { throwArgumentTypeError(lexicalGlobalObject, scope, 0, "arrayBuffer"_s, "TestOverloadedConstructors"_s, nullptr, "ArrayBuffer"_s); });
    if (arrayBufferConversionResult.hasException(throwScope)) [[unlikely]]
       return encodedJSValue();
    auto object = TestOverloadedConstructors::create(arrayBufferConversionResult.releaseReturnValue());
    if constexpr (IsExceptionOr<decltype(object)>)
        RETURN_IF_EXCEPTION(throwScope, { });
    static_assert(TypeOrExceptionOrUnderlyingType<decltype(object)>::isRef);
    auto jsValue = toJSNewlyCreated<IDLInterface<TestOverloadedConstructors>>(*lexicalGlobalObject, *castedThis->globalObject(), throwScope, WTFMove(object));
    if constexpr (IsExceptionOr<decltype(object)>)
        RETURN_IF_EXCEPTION(throwScope, { });
    setSubclassStructureIfNeeded<TestOverloadedConstructors>(lexicalGlobalObject, callFrame, asObject(jsValue));
    RETURN_IF_EXCEPTION(throwScope, { });
    return JSValue::encode(jsValue);
}

static inline EncodedJSValue constructJSTestOverloadedConstructors2(JSGlobalObject* lexicalGlobalObject, CallFrame* callFrame)
{
    SUPPRESS_UNCOUNTED_LOCAL auto& vm = lexicalGlobalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    auto* castedThis = jsCast<JSTestOverloadedConstructorsDOMConstructor*>(callFrame->jsCallee());
    ASSERT(castedThis);
    EnsureStillAliveScope argument0 = callFrame->uncheckedArgument(0);
    auto arrayBufferViewConversionResult = convert<IDLArrayBufferView>(*lexicalGlobalObject, argument0.value(), [](JSC::JSGlobalObject& lexicalGlobalObject, JSC::ThrowScope& scope) { throwArgumentTypeError(lexicalGlobalObject, scope, 0, "arrayBufferView"_s, "TestOverloadedConstructors"_s, nullptr, "ArrayBufferView"_s); });
    if (arrayBufferViewConversionResult.hasException(throwScope)) [[unlikely]]
       return encodedJSValue();
    auto object = TestOverloadedConstructors::create(arrayBufferViewConversionResult.releaseReturnValue());
    if constexpr (IsExceptionOr<decltype(object)>)
        RETURN_IF_EXCEPTION(throwScope, { });
    static_assert(TypeOrExceptionOrUnderlyingType<decltype(object)>::isRef);
    auto jsValue = toJSNewlyCreated<IDLInterface<TestOverloadedConstructors>>(*lexicalGlobalObject, *castedThis->globalObject(), throwScope, WTFMove(object));
    if constexpr (IsExceptionOr<decltype(object)>)
        RETURN_IF_EXCEPTION(throwScope, { });
    setSubclassStructureIfNeeded<TestOverloadedConstructors>(lexicalGlobalObject, callFrame, asObject(jsValue));
    RETURN_IF_EXCEPTION(throwScope, { });
    return JSValue::encode(jsValue);
}

static inline EncodedJSValue constructJSTestOverloadedConstructors3(JSGlobalObject* lexicalGlobalObject, CallFrame* callFrame)
{
    SUPPRESS_UNCOUNTED_LOCAL auto& vm = lexicalGlobalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    auto* castedThis = jsCast<JSTestOverloadedConstructorsDOMConstructor*>(callFrame->jsCallee());
    ASSERT(castedThis);
    EnsureStillAliveScope argument0 = callFrame->uncheckedArgument(0);
    auto blobConversionResult = convert<IDLInterface<Blob>>(*lexicalGlobalObject, argument0.value(), [](JSC::JSGlobalObject& lexicalGlobalObject, JSC::ThrowScope& scope) { throwArgumentTypeError(lexicalGlobalObject, scope, 0, "blob"_s, "TestOverloadedConstructors"_s, nullptr, "Blob"_s); });
    if (blobConversionResult.hasException(throwScope)) [[unlikely]]
       return encodedJSValue();
    auto object = TestOverloadedConstructors::create(*blobConversionResult.releaseReturnValue());
    if constexpr (IsExceptionOr<decltype(object)>)
        RETURN_IF_EXCEPTION(throwScope, { });
    static_assert(TypeOrExceptionOrUnderlyingType<decltype(object)>::isRef);
    auto jsValue = toJSNewlyCreated<IDLInterface<TestOverloadedConstructors>>(*lexicalGlobalObject, *castedThis->globalObject(), throwScope, WTFMove(object));
    if constexpr (IsExceptionOr<decltype(object)>)
        RETURN_IF_EXCEPTION(throwScope, { });
    setSubclassStructureIfNeeded<TestOverloadedConstructors>(lexicalGlobalObject, callFrame, asObject(jsValue));
    RETURN_IF_EXCEPTION(throwScope, { });
    return JSValue::encode(jsValue);
}

static inline EncodedJSValue constructJSTestOverloadedConstructors4(JSGlobalObject* lexicalGlobalObject, CallFrame* callFrame)
{
    SUPPRESS_UNCOUNTED_LOCAL auto& vm = lexicalGlobalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    auto* castedThis = jsCast<JSTestOverloadedConstructorsDOMConstructor*>(callFrame->jsCallee());
    ASSERT(castedThis);
    EnsureStillAliveScope argument0 = callFrame->uncheckedArgument(0);
    auto stringConversionResult = convert<IDLDOMString>(*lexicalGlobalObject, argument0.value());
    if (stringConversionResult.hasException(throwScope)) [[unlikely]]
       return encodedJSValue();
    auto object = TestOverloadedConstructors::create(stringConversionResult.releaseReturnValue());
    if constexpr (IsExceptionOr<decltype(object)>)
        RETURN_IF_EXCEPTION(throwScope, { });
    static_assert(TypeOrExceptionOrUnderlyingType<decltype(object)>::isRef);
    auto jsValue = toJSNewlyCreated<IDLInterface<TestOverloadedConstructors>>(*lexicalGlobalObject, *castedThis->globalObject(), throwScope, WTFMove(object));
    if constexpr (IsExceptionOr<decltype(object)>)
        RETURN_IF_EXCEPTION(throwScope, { });
    setSubclassStructureIfNeeded<TestOverloadedConstructors>(lexicalGlobalObject, callFrame, asObject(jsValue));
    RETURN_IF_EXCEPTION(throwScope, { });
    return JSValue::encode(jsValue);
}

static inline EncodedJSValue constructJSTestOverloadedConstructors5(JSGlobalObject* lexicalGlobalObject, CallFrame* callFrame)
{
    SUPPRESS_UNCOUNTED_LOCAL auto& vm = lexicalGlobalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    auto* castedThis = jsCast<JSTestOverloadedConstructorsDOMConstructor*>(callFrame->jsCallee());
    ASSERT(castedThis);
    auto longArgs = convertVariadicArguments<IDLLong>(*lexicalGlobalObject, *callFrame, 0);
    RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
    auto object = TestOverloadedConstructors::create(WTFMove(longArgs));
    if constexpr (IsExceptionOr<decltype(object)>)
        RETURN_IF_EXCEPTION(throwScope, { });
    static_assert(TypeOrExceptionOrUnderlyingType<decltype(object)>::isRef);
    auto jsValue = toJSNewlyCreated<IDLInterface<TestOverloadedConstructors>>(*lexicalGlobalObject, *castedThis->globalObject(), throwScope, WTFMove(object));
    if constexpr (IsExceptionOr<decltype(object)>)
        RETURN_IF_EXCEPTION(throwScope, { });
    setSubclassStructureIfNeeded<TestOverloadedConstructors>(lexicalGlobalObject, callFrame, asObject(jsValue));
    RETURN_IF_EXCEPTION(throwScope, { });
    return JSValue::encode(jsValue);
}

template<> EncodedJSValue JSC_HOST_CALL_ATTRIBUTES JSTestOverloadedConstructorsDOMConstructor::construct(JSGlobalObject* lexicalGlobalObject, CallFrame* callFrame)
{
    SUPPRESS_UNCOUNTED_LOCAL auto& vm = lexicalGlobalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    UNUSED_PARAM(throwScope);
    size_t argsCount = std::min<size_t>(1, callFrame->argumentCount());
    if (argsCount == 0) {
        RELEASE_AND_RETURN(throwScope, (constructJSTestOverloadedConstructors5(lexicalGlobalObject, callFrame)));
    }
    if (argsCount == 1) {
        JSValue distinguishingArg = callFrame->uncheckedArgument(0);
        if (distinguishingArg.isObject() && asObject(distinguishingArg)->inherits<JSArrayBuffer>())
            RELEASE_AND_RETURN(throwScope, (constructJSTestOverloadedConstructors1(lexicalGlobalObject, callFrame)));
        if (distinguishingArg.isObject() && asObject(distinguishingArg)->inherits<JSArrayBufferView>())
            RELEASE_AND_RETURN(throwScope, (constructJSTestOverloadedConstructors2(lexicalGlobalObject, callFrame)));
        if (distinguishingArg.isObject() && asObject(distinguishingArg)->inherits<JSBlob>())
            RELEASE_AND_RETURN(throwScope, (constructJSTestOverloadedConstructors3(lexicalGlobalObject, callFrame)));
        if (distinguishingArg.isNumber())
            RELEASE_AND_RETURN(throwScope, (constructJSTestOverloadedConstructors5(lexicalGlobalObject, callFrame)));
        RELEASE_AND_RETURN(throwScope, (constructJSTestOverloadedConstructors4(lexicalGlobalObject, callFrame)));
    }
    return throwVMTypeError(lexicalGlobalObject, throwScope);
}
JSC_ANNOTATE_HOST_FUNCTION(JSTestOverloadedConstructorsConstructorConstruct, JSTestOverloadedConstructorsDOMConstructor::construct);

template<> const ClassInfo JSTestOverloadedConstructorsDOMConstructor::s_info = { "TestOverloadedConstructors"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSTestOverloadedConstructorsDOMConstructor) };

template<> JSValue JSTestOverloadedConstructorsDOMConstructor::prototypeForStructure(JSC::VM& vm, const JSDOMGlobalObject& globalObject)
{
    UNUSED_PARAM(vm);
    return globalObject.functionPrototype();
}

template<> void JSTestOverloadedConstructorsDOMConstructor::initializeProperties(VM& vm, JSDOMGlobalObject& globalObject)
{
    putDirect(vm, vm.propertyNames->length, jsNumber(0), JSC::PropertyAttribute::ReadOnly | JSC::PropertyAttribute::DontEnum);
    JSString* nameString = jsNontrivialString(vm, "TestOverloadedConstructors"_s);
    m_originalName.set(vm, this, nameString);
    putDirect(vm, vm.propertyNames->name, nameString, JSC::PropertyAttribute::ReadOnly | JSC::PropertyAttribute::DontEnum);
    putDirect(vm, vm.propertyNames->prototype, JSTestOverloadedConstructors::prototype(vm, globalObject), JSC::PropertyAttribute::ReadOnly | JSC::PropertyAttribute::DontEnum | JSC::PropertyAttribute::DontDelete);
}

/* Hash table for prototype */

static const std::array<HashTableValue, 1> JSTestOverloadedConstructorsPrototypeTableValues {
    HashTableValue { "constructor"_s, static_cast<unsigned>(PropertyAttribute::DontEnum), NoIntrinsic, { HashTableValue::GetterSetterType, jsTestOverloadedConstructorsConstructor, 0 } },
};

const ClassInfo JSTestOverloadedConstructorsPrototype::s_info = { "TestOverloadedConstructors"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSTestOverloadedConstructorsPrototype) };

void JSTestOverloadedConstructorsPrototype::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    reifyStaticProperties(vm, JSTestOverloadedConstructors::info(), JSTestOverloadedConstructorsPrototypeTableValues, *this);
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

const ClassInfo JSTestOverloadedConstructors::s_info = { "TestOverloadedConstructors"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSTestOverloadedConstructors) };

JSTestOverloadedConstructors::JSTestOverloadedConstructors(Structure* structure, JSDOMGlobalObject& globalObject, Ref<TestOverloadedConstructors>&& impl)
    : JSDOMWrapper<TestOverloadedConstructors>(structure, globalObject, WTFMove(impl))
{
}

static_assert(!std::is_base_of<ActiveDOMObject, TestOverloadedConstructors>::value, "Interface is not marked as [ActiveDOMObject] even though implementation class subclasses ActiveDOMObject.");

JSObject* JSTestOverloadedConstructors::createPrototype(VM& vm, JSDOMGlobalObject& globalObject)
{
    auto* structure = JSTestOverloadedConstructorsPrototype::createStructure(vm, &globalObject, globalObject.objectPrototype());
    structure->setMayBePrototype(true);
    return JSTestOverloadedConstructorsPrototype::create(vm, &globalObject, structure);
}

JSObject* JSTestOverloadedConstructors::prototype(VM& vm, JSDOMGlobalObject& globalObject)
{
    return getDOMPrototype<JSTestOverloadedConstructors>(vm, globalObject);
}

JSValue JSTestOverloadedConstructors::getConstructor(VM& vm, const JSGlobalObject* globalObject)
{
    return getDOMConstructor<JSTestOverloadedConstructorsDOMConstructor, DOMConstructorID::TestOverloadedConstructors>(vm, *jsCast<const JSDOMGlobalObject*>(globalObject));
}

void JSTestOverloadedConstructors::destroy(JSC::JSCell* cell)
{
    JSTestOverloadedConstructors* thisObject = static_cast<JSTestOverloadedConstructors*>(cell);
    thisObject->JSTestOverloadedConstructors::~JSTestOverloadedConstructors();
}

JSC_DEFINE_CUSTOM_GETTER(jsTestOverloadedConstructorsConstructor, (JSGlobalObject* lexicalGlobalObject, EncodedJSValue thisValue, PropertyName))
{
    SUPPRESS_UNCOUNTED_LOCAL auto& vm = JSC::getVM(lexicalGlobalObject);
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    auto* prototype = jsDynamicCast<JSTestOverloadedConstructorsPrototype*>(JSValue::decode(thisValue));
    if (!prototype) [[unlikely]]
        return throwVMTypeError(lexicalGlobalObject, throwScope);
    return JSValue::encode(JSTestOverloadedConstructors::getConstructor(vm, prototype->globalObject()));
}

JSC::GCClient::IsoSubspace* JSTestOverloadedConstructors::subspaceForImpl(JSC::VM& vm)
{
    return WebCore::subspaceForImpl<JSTestOverloadedConstructors, UseCustomHeapCellType::No>(vm, "JSTestOverloadedConstructors"_s,
        [] (auto& spaces) { return spaces.m_clientSubspaceForTestOverloadedConstructors.get(); },
        [] (auto& spaces, auto&& space) { spaces.m_clientSubspaceForTestOverloadedConstructors = std::forward<decltype(space)>(space); },
        [] (auto& spaces) { return spaces.m_subspaceForTestOverloadedConstructors.get(); },
        [] (auto& spaces, auto&& space) { spaces.m_subspaceForTestOverloadedConstructors = std::forward<decltype(space)>(space); }
    );
}

void JSTestOverloadedConstructors::analyzeHeap(JSCell* cell, HeapAnalyzer& analyzer)
{
    auto* thisObject = jsCast<JSTestOverloadedConstructors*>(cell);
    analyzer.setWrappedObjectForCell(cell, &thisObject->wrapped());
    if (RefPtr context = thisObject->scriptExecutionContext())
        analyzer.setLabelForCell(cell, makeString("url "_s, context->url().string()));
    Base::analyzeHeap(cell, analyzer);
}

bool JSTestOverloadedConstructorsOwner::isReachableFromOpaqueRoots(JSC::Handle<JSC::Unknown> handle, void*, AbstractSlotVisitor& visitor, ASCIILiteral* reason)
{
    UNUSED_PARAM(handle);
    UNUSED_PARAM(visitor);
    UNUSED_PARAM(reason);
    return false;
}

void JSTestOverloadedConstructorsOwner::finalize(JSC::Handle<JSC::Unknown> handle, void* context)
{
    auto* jsTestOverloadedConstructors = static_cast<JSTestOverloadedConstructors*>(handle.slot()->asCell());
    auto& world = *static_cast<DOMWrapperWorld*>(context);
    uncacheWrapper(world, jsTestOverloadedConstructors->protectedWrapped().ptr(), jsTestOverloadedConstructors);
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
#if ENABLE(BINDING_INTEGRITY)
#if PLATFORM(WIN)
#pragma warning(disable: 4483)
extern "C" { extern void (*const __identifier("??_7TestOverloadedConstructors@WebCore@@6B@")[])(); }
#else
extern "C" { extern void* _ZTVN7WebCore26TestOverloadedConstructorsE[]; }
#endif
template<std::same_as<TestOverloadedConstructors> T>
static inline void verifyVTable(TestOverloadedConstructors* ptr) 
{
    if constexpr (std::is_polymorphic_v<T>) {
        const void* actualVTablePointer = getVTablePointer<T>(ptr);
#if PLATFORM(WIN)
        void* expectedVTablePointer = __identifier("??_7TestOverloadedConstructors@WebCore@@6B@");
#else
        void* expectedVTablePointer = &_ZTVN7WebCore26TestOverloadedConstructorsE[2];
#endif

        // If you hit this assertion you either have a use after free bug, or
        // TestOverloadedConstructors has subclasses. If TestOverloadedConstructors has subclasses that get passed
        // to toJS() we currently require TestOverloadedConstructors you to opt out of binding hardening
        // by adding the SkipVTableValidation attribute to the interface IDL definition
        RELEASE_ASSERT(actualVTablePointer == expectedVTablePointer);
    }
}
#endif
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

JSC::JSValue toJSNewlyCreated(JSC::JSGlobalObject*, JSDOMGlobalObject* globalObject, Ref<TestOverloadedConstructors>&& impl)
{
#if ENABLE(BINDING_INTEGRITY)
    verifyVTable<TestOverloadedConstructors>(impl.ptr());
#endif
    return createWrapper<TestOverloadedConstructors>(globalObject, WTFMove(impl));
}

JSC::JSValue toJS(JSC::JSGlobalObject* lexicalGlobalObject, JSDOMGlobalObject* globalObject, TestOverloadedConstructors& impl)
{
    return wrap(lexicalGlobalObject, globalObject, impl);
}

TestOverloadedConstructors* JSTestOverloadedConstructors::toWrapped(JSC::VM&, JSC::JSValue value)
{
    if (auto* wrapper = jsDynamicCast<JSTestOverloadedConstructors*>(value))
        return &wrapper->wrapped();
    return nullptr;
}

}
