/*
  ==============================================================================

   This file is part of the JUCE framework.
   Copyright (c) Raw Material Software Limited

   JUCE is an open source framework subject to commercial or open source
   licensing.

   By downloading, installing, or using the JUCE framework, or combining the
   JUCE framework with any other source code, object code, content or any other
   copyrightable work, you agree to the terms of the JUCE End User Licence
   Agreement, and all incorporated terms including the JUCE Privacy Policy and
   the JUCE Website Terms of Service, as applicable, which will bind you. If you
   do not agree to the terms of these agreements, we will not license the JUCE
   framework to you, and you must discontinue the installation or download
   process and cease use of the JUCE framework.

   JUCE End User Licence Agreement: https://juce.com/legal/juce-9-licence/
   JUCE Privacy Policy: https://juce.com/juce-privacy-policy
   JUCE Website Terms of Service: https://juce.com/juce-website-terms-of-service/

   Or:

   You may also use this code under the terms of the AGPLv3:
   https://www.gnu.org/licenses/agpl-3.0.en.html

   THE JUCE FRAMEWORK IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL
   WARRANTIES, WHETHER EXPRESSED OR IMPLIED, INCLUDING WARRANTY OF
   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, ARE DISCLAIMED.

  ==============================================================================
*/

namespace juce::detail
{

//==============================================================================
// On Linux int64 and int64_t don't resolve to the same type and don't have C style casts between
// each other, hence we need a two-step conversion.
template <typename T>
static int64_t fromJuceInt64 (const T& convertible) { return (int64_t) (int64) convertible; }

template <typename T>
static int64_t toJuceInt64   (const T& convertible) { return (int64) (int64_t) convertible; }

//==============================================================================
namespace qjs = choc::javascript::quickjs;

using VarOrError = std::variant<var, String>;

static var discardError (VarOrError variant)
{
    const auto* v = std::get_if<var> (&variant);
    return v != nullptr ? *v : var::undefined();
}

static VarOrError quickJSToJuce (const qjs::QuickJSContext::ValuePtr& ptr);
static var::NativeFunction makeNativeFunction (qjs::JSContext* ctx, qjs::JSValueConst fn, qjs::JSValueConst self);

static std::vector<var> quickJSToJuce (Span<qjs::JSValueConst> args, qjs::JSContext* ctx)
{
    std::vector<var> argList;
    argList.reserve (args.size());

    for (const auto& arg : args)
        argList.push_back (discardError (quickJSToJuce ({ qjs::JS_DupValue (ctx, arg), ctx })));

    return argList;
}

static qjs::JSValue juceToQuickJs (const var& v, qjs::JSContext* ctx)
{
    using namespace qjs;

    if (v.isVoid())
        return JS_NULL;

    if (v.isUndefined())
        return JS_UNDEFINED;

    if (v.isInt())
        return JS_NewInt32   (ctx, static_cast<int> (v));

    if (v.isInt64())
        return JS_NewInt64   (ctx, static_cast<int64> (v));

    if (v.isDouble())
        return JS_NewFloat64 (ctx, static_cast<double>  (v));

    if (v.isBool())
        return JS_NewBool    (ctx, static_cast<bool>    (v));

    if (v.isString())
    {
        const String x = v;
        return JS_NewStringLen (ctx, x.toRawUTF8(), x.getNumBytesAsUTF8());
    }

    if (auto fn = v.getNativeFunction())
    {
        using Fn = var::NativeFunction;
        static constexpr auto size = sizeof (fn);

        const auto cb = [] (JSContext* localContext,
                            JSValueConst thisVal,
                            int argc,
                            JSValueConst* argv,
                            int,
                            JSValue* funcData) -> JSValue
        {
            if (funcData == nullptr)
            {
                jassertfalse;
                return {};
            }

            size_t bufferSize{};
            void* buffer = qjs::JS_GetArrayBuffer (localContext, &bufferSize, *funcData);

            if (buffer == nullptr || bufferSize != size)
            {
                jassertfalse;
                return {};
            }

            const auto thisConverted = discardError (quickJSToJuce ({ qjs::JS_DupValue (localContext, thisVal), localContext }));
            const auto argsConverted = quickJSToJuce ({ argv, (size_t) argc }, localContext);
            const var::NativeFunctionArgs args { thisConverted, argsConverted.data(), (int) argsConverted.size() };

            const auto resultVar = (*static_cast<Fn*> (buffer)) (args);

            return juceToQuickJs (resultVar, localContext);
        };

        const auto free = [] (JSRuntime*, void*, void* buffer)
        {
            auto* localFn = static_cast<Fn*> (buffer);
            localFn->~Fn();
            delete[] static_cast<uint8_t*> (buffer);
        };

        std::unique_ptr<uint8_t[]> storage { new uint8_t[size] };
        new (storage.get()) Fn { std::move (fn) };

        qjs::QuickJSContext::ValuePtr callbackAsData { qjs::JS_NewArrayBuffer (ctx,
                                                                               storage.release(),
                                                                               size,
                                                                               free,
                                                                               nullptr,
                                                                               false),
                                                       ctx };
        return JS_NewCFunctionData (ctx, cb, 0, 0, 1, &callbackAsData.value);
    }

    if (auto* array = v.getArray())
    {
        auto result = JS_NewArray (ctx);

        for (const auto [index, value] : enumerate (*array, uint32_t{}))
            JS_SetPropertyUint32 (ctx, result, index, juceToQuickJs (value, ctx));

        return result;
    }

    if (auto* obj = v.getDynamicObject())
    {
        auto result = JS_NewObject (ctx);

        for (const auto& pair : obj->getProperties())
        {
            const auto name = pair.name.toString();
            JS_SetPropertyStr (ctx, result, name.toRawUTF8(), juceToQuickJs (pair.value, ctx));
        }

        return result;
    }

    jassertfalse;
    return JS_UNDEFINED;
}

//==============================================================================
JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wsubobject-linkage")
struct JSFunctionArguments
{
    explicit JSFunctionArguments (qjs::JSContext* contextIn)
        : context (contextIn)
    {
    }

    JSFunctionArguments (qjs::JSContext* contextIn, const var::NativeFunctionArgs& args)
        : JSFunctionArguments (contextIn, Span { args.arguments, (size_t) args.numArguments })
    {
    }

    JSFunctionArguments (qjs::JSContext* contextIn, Span<const var> args)
        : context (contextIn)
    {
        values.reserve (args.size());

        for (const auto& arg : args)
            values.push_back (juceToQuickJs (arg, context));
    }

    ~JSFunctionArguments()
    {
        for (const auto& value : values)
            qjs::JS_FreeValue (context, value);
    }

    void add (const var& arg)
    {
        values.push_back (juceToQuickJs (arg, context));
    }

    qjs::JSValue* getArguments()
    {
        return values.data();
    }

    int getSize() const
    {
        return (int) values.size();
    }

private:
    qjs::JSContext* context;
    std::vector<qjs::JSValue> values;

    JUCE_DECLARE_NON_COPYABLE (JSFunctionArguments)
    JUCE_DECLARE_NON_MOVEABLE (JSFunctionArguments)
};
JUCE_END_IGNORE_WARNINGS_GCC_LIKE

//==============================================================================
// Throws on failure
static var tryQuickJSToJuce (const qjs::QuickJSContext::ValuePtr& ptr,
                             const qjs::JSValue* parent = nullptr)
{
    using namespace qjs;

    jassert (ptr.context != nullptr);

    if (JS_IsUndefined (ptr.value))
        return var::undefined();

    if (JS_IsNull (ptr.value))
        return var{};

    if (JS_IsNumber (ptr.value))
    {
        if(JS_IsBigInt(ptr.value))
        {
            int64_t i = 0;
            JS_ToBigInt64 (ptr.context, std::addressof (i), ptr.value);
            return var((juce::int64)i);
        }if(JS_IsInteger(ptr.value))
        {
            int32_t i = 0;
            JS_ToInt32 (ptr.context, std::addressof (i), ptr.value);
            return i;
        }
        {
            double d = 0;
            JS_ToFloat64 (ptr.context, std::addressof (d), ptr.value);
            return d;
        }
    }

    if (JS_IsBool (ptr.value))
        return JS_ToBool (ptr.context, ptr.value) != 0;

    if (JS_IsString (ptr.value))
    {
        size_t len = 0;
        const auto* s = JS_ToCStringLen2 (ptr.context, std::addressof (len), ptr.value, false);
        const ScopeGuard scope { [&] { JS_FreeCString (ptr.context, s); } };
        return String::fromUTF8 (s, (int) len);
    }

    if (JS_IsArray (ptr.context, ptr.value))
    {
        const auto lengthProp = ptr["length"];
        uint32_t len = 0;
        JS_ToUint32 (ptr.context, &len, lengthProp.get());

        Array<var> result;
        result.ensureStorageAllocated ((int) len);

        for (auto i = decltype (len){}; i < len; ++i)
            result.add (tryQuickJSToJuce (ptr[i], &ptr.value));

        return result;
    }

    if (JS_IsFunction (ptr.context, ptr.value))
    {
        const qjs::QuickJSContext::ValuePtr parentToUse { parent != nullptr ? JS_DupValue (ptr.context, *parent)
                                                                            : JS_GetGlobalObject (ptr.context),
                                                          ptr.context };

        return makeNativeFunction (ptr.context, ptr.value, parentToUse.value);
    }

    if (JS_IsObject (ptr.value))
    {
        std::vector<std::string> propNames;

        for (auto obj = ptr.takeValue (JS_DupValue (ptr.context, ptr.value));;)
        {
            JSPropertyEnum* properties = nullptr;
            uint32_t numProps = 0;

            if (JS_GetOwnPropertyNames (ptr.context, &properties, &numProps, obj.get(), JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) != 0
                || properties == nullptr)
            {
                return new DynamicObject;
            }

            const ScopeGuard scope { [&] { js_free (ptr.context, properties); } };

            propNames.reserve (numProps);

            for (uint32_t i = 0; i < numProps; ++i)
            {
                const auto* name = JS_AtomToCString (ptr.context, properties[i].atom);
                std::string nameString (name);

                if (nameString != QuickJSContext::objectNameAttribute)
                    propNames.push_back (std::move (nameString));

                JS_FreeCString (ptr.context, name);
                JS_FreeAtom (ptr.context, properties[i].atom);
            }

            auto proto = ptr.takeValue (JS_GetPrototype (ptr.context, obj.get()));

            if (! JS_IsObject (proto.get()))
                break;

            obj = std::move (proto);
        }

        DynamicObject::Ptr result = new DynamicObject;

        for (auto& propName : propNames)
            result->setProperty (String (propName), tryQuickJSToJuce (ptr[propName.c_str()], &ptr.value));

        return result.get();
    }

    ptr.throwIfError();
    return {};
}

static VarOrError quickJSToJuce (const qjs::QuickJSContext::ValuePtr& ptr)
{
    try
    {
        return tryQuickJSToJuce (ptr);
    }
    catch (const choc::javascript::Error& error)
    {
        return String (error.what());
    }
}

//==============================================================================
// Any type that references the QuickJS types inside the anonymous namespace added by us requires
// this with GCC. Suppressing this warning is fine, since these classes are only visible and used
// in a single translation unit.
JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wsubobject-linkage")
class QuickJSWrapper
{
public:
    class ValueHandle
    {
    public:
        ValueHandle() = default;

        // Takes ownership of a reference to the value
        ValueHandle (QuickJSWrapper& engineIn, qjs::JSValue valueIn)
            : engine (&engineIn), value (valueIn)
        {
            engine->handles.insert (this);
        }

        ValueHandle (const ValueHandle& other)
            : engine (other.engine),
              value (engine != nullptr ? qjs::JS_DupValue (engine->getQuickJSContext(), other.value) : qjs::JSValue{})
        {
            if (engine != nullptr)
                engine->handles.insert (this);
        }

        ValueHandle (ValueHandle&& other) noexcept
            : engine (std::exchange (other.engine, {})),
              value (std::exchange (other.value, {}))
        {
            if (engine != nullptr)
            {
                auto node = engine->handles.extract (&other);
                node.value() = this;
                engine->handles.insert (std::move (node));
            }
        }

        ValueHandle& operator= (const ValueHandle&) = delete;
        ValueHandle& operator= (ValueHandle&&) = delete;

        ~ValueHandle()
        {
            release();
        }

        void release()
        {
            if (engine == nullptr)
                return;

            engine->handles.erase (this);
            qjs::JS_FreeValue (std::exchange (engine, nullptr)->getQuickJSContext(), value);
        }

        QuickJSWrapper* getEngine() const  { return engine; }
        qjs::JSValue get() const           { return value; }

    private:
        QuickJSWrapper* engine = nullptr;
        qjs::JSValue value{};
    };

    explicit QuickJSWrapper (const RelativeTime* maximumExecutionTimeIn)
        : maximumExecutionTime { *maximumExecutionTimeIn }
    {
        qjs::JS_SetInterruptHandler (getQuickJSRuntime(), handleInterrupt, (void*) this);
        qjs::JS_SetRuntimeOpaque (getQuickJSRuntime(), this);
    }

    ~QuickJSWrapper()
    {
        while (! handles.empty())
            (*handles.begin())->release();
    }

    static QuickJSWrapper& fromContext (qjs::JSContext* ctx)
    {
        return *static_cast<QuickJSWrapper*> (qjs::JS_GetRuntimeOpaque (qjs::JS_GetRuntime (ctx)));
    }

    qjs::JSContext* getQuickJSContext() const
    {
        return impl->context;
    }

    qjs::JSRuntime* getQuickJSRuntime() const
    {
        return impl->runtime;
    }

    void stop()
    {
        timeout = (int64) Time::getMillisecondCounterHiRes();
    }

    class ScopedExecution
    {
    public:
        explicit ScopedExecution (QuickJSWrapper& engineIn)
            : engine (engineIn)
        {
            if (engine.executionDepth++ == 0)
                engine.resetTimeout();
        }

        ~ScopedExecution()
        {
            --engine.executionDepth;
        }

    private:
        QuickJSWrapper& engine;

        JUCE_DECLARE_NON_COPYABLE (ScopedExecution)
    };

private:
    void resetTimeout()
    {
        timeout = (int64) Time::getMillisecondCounterHiRes() + maximumExecutionTime.inMilliseconds();
    }

    static int handleInterrupt (qjs::JSRuntime*, void* opaque)
    {
        auto& self = *static_cast<QuickJSWrapper*> (opaque);
        return (int64) Time::getMillisecondCounterHiRes() >= self.timeout;
    }

    std::unique_ptr<qjs::QuickJSContext> impl = std::make_unique<qjs::QuickJSContext>();
    const RelativeTime& maximumExecutionTime;
    std::atomic<int64> timeout{};
    std::set<ValueHandle*> handles;
    int executionDepth = 0;

    JUCE_DECLARE_NON_COPYABLE (QuickJSWrapper)
};

class JSFunction
{
public:
    JSFunction (QuickJSWrapper& engine, qjs::JSValueConst fnIn, qjs::JSValueConst selfIn)
        : fn (engine, qjs::JS_DupValue (engine.getQuickJSContext(), fnIn)),
          self (engine, qjs::JS_DupValue (engine.getQuickJSContext(), selfIn))
    {
    }

    var operator() (const var::NativeFunctionArgs& args) const
    {
        auto* engine = fn.getEngine();

        if (engine == nullptr)
            return var::undefined();

        const QuickJSWrapper::ScopedExecution execution { *engine };

        auto* ctx = engine->getQuickJSContext();
        JSFunctionArguments convertedArgs { ctx, args };

        const qjs::QuickJSContext::ValuePtr result { qjs::JS_Call (ctx,
                                                                   fn.get(),
                                                                   self.get(),
                                                                   (int) convertedArgs.getSize(),
                                                                   convertedArgs.getArguments()),
                                                     ctx };

        return discardError (quickJSToJuce (result));
    }

private:
    QuickJSWrapper::ValueHandle fn;
    QuickJSWrapper::ValueHandle self;
};
JUCE_END_IGNORE_WARNINGS_GCC_LIKE

static var::NativeFunction makeNativeFunction (qjs::JSContext* ctx, qjs::JSValueConst fn, qjs::JSValueConst self)
{
    return JSFunction { QuickJSWrapper::fromContext (ctx), fn, self };
}

using SetterFn = qjs::JSValue (*) (qjs::JSContext* ctx,
                                   qjs::JSValueConst thisVal,
                                   qjs::JSValueConst val,
                                   int magic);
using GetterFn = qjs::JSValue (*) (qjs::JSContext* ctx, qjs::JSValueConst thisVal, int magic);

// A replacement for the JS_CGETSET_MAGIC_DEF macro in QuickJS
static qjs::JSCFunctionListEntry makeFunctionListEntry (const char* name,
                                                        GetterFn getter,
                                                        SetterFn setter,
                                                        int16_t magic)
{
    qjs::JSCFunctionListEntry e { name, JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE, qjs::JS_DEF_CGETSET_MAGIC, magic, {} };
    e.u.getset.get.getter_magic = getter;
    e.u.getset.set.setter_magic = setter;
    return e;
}

// A replacement for the JS_UNDEFINED macro in QuickJS
static qjs::JSValue makeUndefined()
{
   #if defined(JS_NAN_BOXING) // Differentiates between 32 and 64 bit builds
    return (((uint64_t)(qjs::JS_TAG_UNDEFINED) << 32) | (uint32_t)(0));
   #else
    return qjs::JSValue (static_cast<int32_t> (0), qjs::JS_TAG_UNDEFINED);
   #endif
}

static qjs::JSClassID createClassId()
{
    // A passed in value of 0 asks QuickJS to allocate us a new unique ID. QuickJS uses global
    // variables for the bookkeeping, so it's safe to use this function to initialise globals.
    qjs::JSClassID newId = 0;
    return qjs::JS_NewClassID (&newId);
}

//==============================================================================
/*  Attached as an opaque pointer to the corresponding JS object. Its lifetime is managed by the
    QuickJS engine, which calls the finalise function when the corresponding JSValue is deleted.
*/
struct DynamicObjectWrapper
{
    DynamicObjectWrapper (detail::QuickJSWrapper& engineIn, DynamicObject::Ptr objectIn)
        : engine (engineIn), object (objectIn)
    {
        getDynamicObjects().insert (this);
    }

    int16_t getOrdinal (const Identifier& identifier)
    {
        if (const auto& it = ordinals.find (identifier); it != ordinals.end())
            return it->second;

        identifiers.emplace_back (identifier);
        const auto newSize = identifiers.size() - 1;
        jassert (newSize <= (size_t) std::numeric_limits<int16_t>::max());
        const auto newOrdinal = (int16_t) newSize;
        ordinals[identifier] = newOrdinal;
        return newOrdinal;
    }

    auto getIdentifier (int ordinal) const
    {
        jassert ((size_t) ordinal < identifiers.size());
        return identifiers[(size_t) ordinal];
    }

    NamedValueSet& getProperties() const
    {
        return object->getProperties();
    }

    static void finaliser (qjs::JSRuntime*, qjs::JSValue val)
    {
        auto* wrapper = static_cast<DynamicObjectWrapper*> (qjs::JS_GetOpaque (val, getClassId()));
        wrapper->finalise();
    }

    void finalise()
    {
        getDynamicObjects().erase (this);
        delete this;
    }

    static void createClass (qjs::JSRuntime* runtime)
    {
        qjs::JSClassDef classDef {};
        classDef.class_name = "juce_DynamicObject";
        classDef.finalizer  = finaliser;
        qjs::JS_NewClass (runtime, getClassId(), &classDef);
    }

    //==============================================================================
    static DynamicObjectWrapper* getWrapperForThis (qjs::JSValueConst thisValue, int ordinal)
    {
        auto* self = static_cast<DynamicObjectWrapper*> (qjs::JS_GetOpaque (thisValue, getClassId()));
        return self != nullptr && isPositiveAndBelow (ordinal, (int) self->identifiers.size()) ? self : nullptr;
    }

    static qjs::JSValue throwIncompatibleThis (qjs::JSContext* ctx)
    {
        return qjs::JS_ThrowTypeError (ctx, "function called on an object other than the native object it belongs to");
    }

    static qjs::JSValue callDispatcher (qjs::JSContext* ctx,
                                        qjs::JSValueConst thisValue,
                                        int numArgs,
                                        qjs::JSValueConst* args,
                                        int ordinal)
    {
        auto* self = getWrapperForThis (thisValue, ordinal);

        if (self == nullptr)
            return throwIncompatibleThis (ctx);

        const auto argList = quickJSToJuce (Span { args, (size_t) numArgs }, ctx);
        const auto identifier = self->getIdentifier (ordinal);
        auto result = self->object->invokeMethod (identifier,
                                                  { self->object.get(), argList.data(), (int) argList.size() });
        return juceToQuickJs (result, ctx);
    }

    static qjs::JSValue setDispatcher (qjs::JSContext* ctx,
                                       qjs::JSValueConst thisVal,
                                       qjs::JSValueConst val,
                                       int ordinal)
    {
        auto* self = getWrapperForThis (thisVal, ordinal);

        if (self == nullptr)
            return throwIncompatibleThis (ctx);

        self->object->setProperty (self->getIdentifier (ordinal), discardError (quickJSToJuce ({ qjs::JS_DupValue (ctx, val), ctx })));

        // In case there is a problem we could return e.g. `JS_EXCEPTION` or
        // `JS_ThrowRangeError(ctx, "invalid precision");` here.
        return makeUndefined();
    }

    static qjs::JSValue getDispatcher (qjs::JSContext* ctx, qjs::JSValueConst thisVal, int ordinal)
    {
        auto* self = getWrapperForThis (thisVal, ordinal);

        if (self == nullptr)
            return throwIncompatibleThis (ctx);

        return juceToQuickJs (self->object->getProperty (self->getIdentifier (ordinal)), ctx);
    }

    static qjs::JSClassID getClassId()
    {
        static qjs::JSClassID classId = createClassId();
        return classId;
    }

    static std::set<void*>& getDynamicObjects()
    {
        // Used to check if an opaque ptr attached to a JSValue is a DynamicObjectWrapper
        static std::set<void*> dynamicObjects;
        return dynamicObjects;
    }

    //==============================================================================
    QuickJSWrapper& engine;
    DynamicObject::Ptr object;
    std::map<Identifier, int16_t> ordinals;
    std::vector<Identifier> identifiers;
};

//==============================================================================
static bool hasProperty (qjs::JSContext* ctx, qjs::JSValueConst object, const char* name)
{
    qjs::JSAtom atom = JS_NewAtom (ctx, name);
    ScopeGuard freeAtom { [&] { qjs::JS_FreeAtom (ctx, atom); } };

    return JS_HasProperty (ctx, object, atom) > 0;
}

static qjs::JSValue getOrCreateProperty (qjs::JSContext* ctx, qjs::JSValueConst object, const char* name)
{
    if (! hasProperty (ctx, object, name))
        qjs::JS_SetPropertyStr (ctx, object, name, JS_NewObject (ctx));

    return qjs::JS_GetPropertyStr (ctx, object, name);
}

static uint32_t toUint32 (int64 value)
{
    jassert (0 <= value && value <= (int64) std::numeric_limits<uint32_t>::max());
    return (uint32_t) value;
}

} // namespace juce::detail
