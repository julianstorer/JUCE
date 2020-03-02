/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2017 - ROLI Ltd.

   JUCE is an open source library subject to commercial or open-source
   licensing.

   By using JUCE, you agree to the terms of both the JUCE 5 End-User License
   Agreement and JUCE 5 Privacy Policy (both updated and effective as of the
   27th April 2017).

   End User License Agreement: www.juce.com/juce-5-licence
   Privacy Policy: www.juce.com/juce-5-privacy-policy

   Or: You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   JUCE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

  ==============================================================================
*/

#if JUCE_PLUGINHOST_VST

//==============================================================================
#undef PRAGMA_ALIGN_SUPPORTED

#if JUCE_MSVC
 #pragma warning (push)
 #pragma warning (disable: 4996)
#elif ! JUCE_MINGW
 #define __cdecl
#endif

#if JUCE_CLANG
 #if __has_warning("-Wzero-as-null-pointer-constant")
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"
 #endif
#endif

#if JUCE_GCC
 #pragma GCC diagnostic push
 #pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#endif

#define VST_FORCE_DEPRECATED 0
#define JUCE_VSTINTERFACE_H_INCLUDED 1

namespace Vst2
{
// If the following files cannot be found then you are probably trying to host
// VST2 plug-ins. To do this you must have a VST2 SDK in your header search
// paths or use the "VST (Legacy) SDK Folder" field in the Projucer. The VST2
// SDK can be obtained from the vstsdk3610_11_06_2018_build_37 (or older) VST3
// SDK or JUCE version 5.3.2.
#include <pluginterfaces/vst2.x/aeffect.h>
#include <pluginterfaces/vst2.x/aeffectx.h>
}

#include "juce_VSTCommon.h"

#if JUCE_MSVC
 #pragma warning (pop)
 #pragma warning (disable: 4355) // ("this" used in initialiser list warning)
#endif

#if JUCE_CLANG
 #if __has_warning("-Wzero-as-null-pointer-constant")
  #pragma clang diagnostic pop
 #endif
#endif

#if JUCE_GCC
 #pragma GCC diagnostic pop
#endif

#include "juce_VSTMidiEventList.h"

#if JUCE_MINGW
 #ifndef WM_APPCOMMAND
  #define WM_APPCOMMAND 0x0319
 #endif

 extern "C" void _fpreset();
 extern "C" void _clearfp();
#elif ! JUCE_WINDOWS
 static void _fpreset() {}
 static void _clearfp() {}
#endif

#ifndef JUCE_VST_WRAPPER_LOAD_CUSTOM_MAIN
 #define JUCE_VST_WRAPPER_LOAD_CUSTOM_MAIN
#endif

#ifndef JUCE_VST_WRAPPER_INVOKE_MAIN
#define JUCE_VST_WRAPPER_INVOKE_MAIN  effect = module->moduleMain ((Vst2::audioMasterCallback) &audioMaster);
#endif

//==============================================================================
namespace juce
{

//==============================================================================
namespace
{
    const int fxbVersionNum = 1;

    struct fxProgram
    {
        int32 chunkMagic;    // 'CcnK'
        int32 byteSize;      // of this chunk, excl. magic + byteSize
        int32 fxMagic;       // 'FxCk'
        int32 version;
        int32 fxID;          // fx unique id
        int32 fxVersion;
        int32 numParams;
        char prgName[28];
        float params[1];        // variable no. of parameters
    };

    struct fxSet
    {
        int32 chunkMagic;    // 'CcnK'
        int32 byteSize;      // of this chunk, excl. magic + byteSize
        int32 fxMagic;       // 'FxBk'
        int32 version;
        int32 fxID;          // fx unique id
        int32 fxVersion;
        int32 numPrograms;
        char future[128];
        fxProgram programs[1];  // variable no. of programs
    };

    struct fxChunkSet
    {
        int32 chunkMagic;    // 'CcnK'
        int32 byteSize;      // of this chunk, excl. magic + byteSize
        int32 fxMagic;       // 'FxCh', 'FPCh', or 'FBCh'
        int32 version;
        int32 fxID;          // fx unique id
        int32 fxVersion;
        int32 numPrograms;
        char future[128];
        int32 chunkSize;
        char chunk[8];          // variable
    };

    struct fxProgramSet
    {
        int32 chunkMagic;    // 'CcnK'
        int32 byteSize;      // of this chunk, excl. magic + byteSize
        int32 fxMagic;       // 'FxCh', 'FPCh', or 'FBCh'
        int32 version;
        int32 fxID;          // fx unique id
        int32 fxVersion;
        int32 numPrograms;
        char name[28];
        int32 chunkSize;
        char chunk[8];          // variable
    };

    // Compares a magic value in either endianness.
    static bool compareMagic (int32 magic, const char* name) noexcept
    {
        return magic == (int32) ByteOrder::littleEndianInt (name)
            || magic == (int32) ByteOrder::bigEndianInt (name);
    }

    static int32 fxbName (const char* name) noexcept    { return (int32) ByteOrder::littleEndianInt (name); }
    static int32 fxbSwap (int32 x) noexcept             { return (int32) ByteOrder::swapIfLittleEndian ((uint32) x); }

    static float fxbSwapFloat (const float x) noexcept
    {
       #ifdef JUCE_LITTLE_ENDIAN
        union { uint32 asInt; float asFloat; } n;
        n.asFloat = x;
        n.asInt = ByteOrder::swap (n.asInt);
        return n.asFloat;
       #else
        return x;
       #endif
    }
}

//==============================================================================
namespace
{
    static double getVSTHostTimeNanoseconds() noexcept
    {
       #if JUCE_WINDOWS
        return timeGetTime() * 1000000.0;
       #elif JUCE_LINUX || JUCE_IOS || JUCE_ANDROID
        timeval micro;
        gettimeofday (&micro, nullptr);
        return micro.tv_usec * 1000.0;
       #elif JUCE_MAC
        UnsignedWide micro;
        Microseconds (&micro);
        return micro.lo * 1000.0;
       #endif
    }

    static int shellUIDToCreate = 0;
    static int insideVSTCallback = 0;

    struct IdleCallRecursionPreventer
    {
        IdleCallRecursionPreventer()  : isMessageThread (MessageManager::getInstance()->isThisTheMessageThread())
        {
            if (isMessageThread)
                ++insideVSTCallback;
        }

        ~IdleCallRecursionPreventer()
        {
            if (isMessageThread)
                --insideVSTCallback;
        }

        const bool isMessageThread;
        JUCE_DECLARE_NON_COPYABLE (IdleCallRecursionPreventer)
    };

   #if JUCE_MAC
    static bool makeFSRefFromPath (FSRef* destFSRef, const String& path)
    {
        return FSPathMakeRef (reinterpret_cast<const UInt8*> (path.toRawUTF8()), destFSRef, nullptr) == noErr;
    }
   #endif
}

//==============================================================================
typedef Vst2::AEffect* (VSTCALLBACK *MainCall) (Vst2::audioMasterCallback);
static pointer_sized_int VSTCALLBACK audioMaster (Vst2::AEffect*, int32, int32, pointer_sized_int, void*, float);

//==============================================================================
// Change this to disable logging of various VST activities
#ifndef VST_LOGGING
 #define VST_LOGGING 1
#endif

#if VST_LOGGING
 #define JUCE_VST_LOG(a) Logger::writeToLog(a);
#else
 #define JUCE_VST_LOG(a)
#endif

//==============================================================================
#if JUCE_LINUX

namespace
{
    using EventProcPtr = void (*)(XEvent*);

    Window getChildWindow (Window windowToCheck)
    {
        Window rootWindow, parentWindow;
        Window* childWindows;
        unsigned int numChildren = 0;

        {
            ScopedXDisplay xDisplay;
            XQueryTree (xDisplay.display, windowToCheck, &rootWindow, &parentWindow, &childWindows, &numChildren);
        }

        if (numChildren > 0)
            return childWindows [0];

        return 0;
    }
}

#endif

//==============================================================================
class VSTXMLInfo
{
public:
    static VSTXMLInfo* createFor (const juce::XmlElement& xml)
    {
        if (xml.hasTagName ("VSTParametersStructure"))
            return new VSTXMLInfo (xml);

        if (const auto* x = xml.getChildByName ("VSTParametersStructure"))
            return new VSTXMLInfo (*x);

        return nullptr;
    }

    struct Group;

    struct Base
    {
        Base() noexcept {}
        virtual ~Base() {}

        Group* parent = nullptr;
    };

    struct Param  : public Base
    {
        int paramID;
        juce::String expr, name, label;
        juce::StringArray shortNames;
        juce::String type;
        int numberOfStates;
        float defaultValue;
    };

    struct Group  : public Base
    {
        juce::String name;
        juce::OwnedArray<Base> paramTree;
    };

    struct Range
    {
        Range() noexcept {}
        Range (const juce::String& s)       { set (s); }

        void set (const juce::String& s)
        {
            inclusiveLow  = s.startsWithChar ('[');
            inclusiveHigh = s.endsWithChar   (']');

            auto str = s.removeCharacters ("[]");

            low  = str.upToFirstOccurrenceOf (",", false, false).getFloatValue();
            high = str.fromLastOccurrenceOf  (",", false, false).getFloatValue();
        }

        bool contains (float f) const noexcept
        {
            return (inclusiveLow  ? (f >= low)  : (f > low))
                && (inclusiveHigh ? (f <= high) : (f < high));
        }

        float low = 0;
        float high = 0;

        bool inclusiveLow = false;
        bool inclusiveHigh = false;
    };

    struct Entry
    {
        juce::String name;
        Range range;
    };

    struct ValueType
    {
        juce::String name, label;
        juce::OwnedArray<Entry> entries;
    };

    struct Template
    {
        juce::String name;
        juce::OwnedArray<Param> params;
    };

    const Param* getParamForID (const int paramID, const Group* const grp) const
    {
        for (auto item : (grp != nullptr ? grp->paramTree : paramTree))
        {
            if (auto param = dynamic_cast<const Param*> (item))
                if (param->paramID == paramID)
                    return param;

            if (auto group = dynamic_cast<const Group*> (item))
                if (auto res = getParamForID (paramID, group))
                    return res;
        }

        return nullptr;
    }

    const ValueType* getValueType (const juce::String& name) const
    {
        for (auto v : valueTypes)
            if (v->name == name)
                return v;

        return nullptr;
    }

    juce::OwnedArray<Base> paramTree;
    juce::OwnedArray<ValueType> valueTypes;
    juce::OwnedArray<Template> templates;

    ValueType switchValueType;

private:
    VSTXMLInfo (const juce::XmlElement& xml)
    {
        switchValueType.entries.add (new Entry({ TRANS("Off"), Range ("[0, 0.5[") }));
        switchValueType.entries.add (new Entry({ TRANS("On"),  Range ("[0.5, 1]") }));

        forEachXmlChildElement (xml, item)
        {
            if (item->hasTagName ("Param"))           parseParam (*item, nullptr, nullptr);
            else if (item->hasTagName ("ValueType"))  parseValueType (*item);
            else if (item->hasTagName ("Template"))   parseTemplate (*item);
            else if (item->hasTagName ("Group"))      parseGroup (*item, nullptr);
        }
    }

    void parseParam (const juce::XmlElement& item, Group* group, Template* temp)
    {
        auto param = new Param();

        if (temp != nullptr)
            param->expr = item.getStringAttribute ("id");
        else
            param->paramID = item.getIntAttribute ("id");

        param->name           = item.getStringAttribute ("name");
        param->label          = item.getStringAttribute ("label");
        param->type           = item.getStringAttribute ("type");
        param->numberOfStates = item.getIntAttribute ("numberOfStates");
        param->defaultValue   = (float) item.getDoubleAttribute ("defaultValue");

        param->shortNames.addTokens (item.getStringAttribute ("shortName"), ",", juce::StringRef());
        param->shortNames.trim();
        param->shortNames.removeEmptyStrings();

        if (group != nullptr)
        {
            group->paramTree.add (param);
            param->parent = group;
        }
        else if (temp != nullptr)
        {
            temp->params.add (param);
        }
        else
        {
            paramTree.add (param);
        }
    }

    void parseValueType (const juce::XmlElement& item)
    {
        auto vt = new ValueType();
        valueTypes.add (vt);

        vt->name  = item.getStringAttribute ("name");
        vt->label = item.getStringAttribute ("label");

        int curEntry = 0;
        const int numEntries = item.getNumChildElements();

        forEachXmlChildElementWithTagName (item, entryXml, "Entry")
        {
            auto entry = new Entry();
            entry->name = entryXml->getStringAttribute ("name");

            if (entryXml->hasAttribute ("value"))
            {
                entry->range.set(entryXml->getStringAttribute ("value"));
            }
            else
            {
                entry->range.low  = curEntry / (float) numEntries;
                entry->range.high = (curEntry + 1) / (float) numEntries;

                entry->range.inclusiveLow  = true;
                entry->range.inclusiveHigh = (curEntry == numEntries - 1);
            }

            vt->entries.add (entry);
            ++curEntry;
        }
    }

    void parseTemplate (const juce::XmlElement& item)
    {
        auto temp = new Template();
        templates.add (temp);
        temp->name = item.getStringAttribute ("name");

        forEachXmlChildElement (item, param)
            parseParam (*param, nullptr, temp);
    }

    void parseGroup (const juce::XmlElement& item, Group* parentGroup)
    {
        auto group = new Group();

        if (parentGroup)
        {
            parentGroup->paramTree.add (group);
            group->parent = parentGroup;
        }
        else
        {
            paramTree.add (group);
        }

        group->name = item.getStringAttribute ("name");

        if (item.hasAttribute ("template"))
        {
            juce::StringArray variables;
            variables.addTokens (item.getStringAttribute ("values"), ";", juce::StringRef());
            variables.trim();

            for (auto temp : templates)
            {
                if (temp->name == item.getStringAttribute ("template"))
                {
                    for (int i = 0; i < temp->params.size(); ++i)
                    {
                        auto param = new Param();
                        group->paramTree.add (param);

                        param->parent         = group;
                        param->paramID        = evaluate (temp->params[i]->expr, variables);
                        param->defaultValue   = temp->params[i]->defaultValue;
                        param->label          = temp->params[i]->label;
                        param->name           = temp->params[i]->name;
                        param->numberOfStates = temp->params[i]->numberOfStates;
                        param->shortNames     = temp->params[i]->shortNames;
                        param->type           = temp->params[i]->type;
                    }
                }
            }
        }
        else
        {
            forEachXmlChildElement (item, subItem)
            {
                if (subItem->hasTagName ("Param"))       parseParam (*subItem, group, nullptr);
                else if (subItem->hasTagName ("Group"))  parseGroup (*subItem, group);
            }
        }
    }

    int evaluate (juce::String expr, const juce::StringArray& variables) const
    {
        juce::StringArray names;
        juce::Array<int> vals;

        for (auto& v : variables)
        {
            if (v.contains ("="))
            {
                names.add (v.upToFirstOccurrenceOf ("=", false, false));
                vals.add  (v.fromFirstOccurrenceOf ("=", false, false).getIntValue());
            }
        }

        for (int i = 0; i < names.size(); ++i)
        {
            for (;;)
            {
                const int idx = expr.indexOfWholeWord (names[i]);
                if (idx < 0)
                    break;

                expr = expr.replaceSection (idx, names[i].length(), juce::String (vals[i]));
            }
        }

        expr = expr.retainCharacters ("01234567890-+")
                   .replace ("+", " + ")
                   .replace ("-", " - ");

        juce::StringArray tokens;
        tokens.addTokens (expr, " ", juce::StringRef());

        bool add = true;
        int val = 0;

        for (const auto& s : tokens)
        {
            if (s == "+")
            {
                add = true;
            }
            else if (s == "-")
            {
                add = false;
            }
            else
            {
                if (add)
                    val += s.getIntValue();
                else
                    val -= s.getIntValue();
            }
        }

        return val;
    }
};

//==============================================================================
struct ModuleHandle    : public ReferenceCountedObject
{
    File file;
    MainCall moduleMain, customMain = {};
    String pluginName;
    std::unique_ptr<XmlElement> vstXml;

    using Ptr = ReferenceCountedObjectPtr<ModuleHandle>;

    static Array<ModuleHandle*>& getActiveModules()
    {
        static Array<ModuleHandle*> activeModules;
        return activeModules;
    }

    //==============================================================================
    static Ptr findOrCreateModule (const File& file)
    {
        for (auto* module : getActiveModules())
            if (module->file == file)
                return module;

        const IdleCallRecursionPreventer icrp;
        shellUIDToCreate = 0;
        _fpreset();

        JUCE_VST_LOG ("Attempting to load VST: " + file.getFullPathName());

        Ptr m = new ModuleHandle (file, nullptr);

        if (m->open())
        {
            _fpreset();
            return m;
        }

        return {};
    }

    //==============================================================================
    ModuleHandle (const File& f, MainCall customMainCall)
        : file (f), moduleMain (customMainCall)
    {
        getActiveModules().add (this);

       #if JUCE_WINDOWS || JUCE_LINUX || JUCE_IOS || JUCE_ANDROID
        fullParentDirectoryPathName = f.getParentDirectory().getFullPathName();
       #elif JUCE_MAC
        FSRef ref;
        makeFSRefFromPath (&ref, f.getParentDirectory().getFullPathName());
        FSGetCatalogInfo (&ref, kFSCatInfoNone, nullptr, nullptr, &parentDirFSSpec, nullptr);
       #endif
    }

    ~ModuleHandle()
    {
        getActiveModules().removeFirstMatchingValue (this);
        close();
    }

    //==============================================================================
   #if ! JUCE_MAC
    String fullParentDirectoryPathName;
   #endif

  #if JUCE_WINDOWS || JUCE_LINUX || JUCE_ANDROID
    DynamicLibrary module;

    bool open()
    {
        if (moduleMain != nullptr)
            return true;

        pluginName = file.getFileNameWithoutExtension();

        module.open (file.getFullPathName());

        moduleMain = (MainCall) module.getFunction ("VSTPluginMain");

        if (moduleMain == nullptr)
            moduleMain = (MainCall) module.getFunction ("main");

        JUCE_VST_WRAPPER_LOAD_CUSTOM_MAIN

        if (moduleMain != nullptr)
        {
            vstXml = parseXML (file.withFileExtension ("vstxml"));

           #if JUCE_WINDOWS
            if (vstXml == nullptr)
                vstXml = parseXML (getDLLResource (file, "VSTXML", 1));
           #endif
        }

        return moduleMain != nullptr;
    }

    void close()
    {
        _fpreset(); // (doesn't do any harm)

        module.close();
    }

    void closeEffect (Vst2::AEffect* eff)
    {
        eff->dispatcher (eff, Vst2::effClose, 0, 0, nullptr, 0);
    }

   #if JUCE_WINDOWS
    static String getDLLResource (const File& dllFile, const String& type, int resID)
    {
        DynamicLibrary dll (dllFile.getFullPathName());
        auto dllModule = (HMODULE) dll.getNativeHandle();

        if (dllModule != INVALID_HANDLE_VALUE)
        {
            if (auto res = FindResource (dllModule, MAKEINTRESOURCE (resID), type.toWideCharPointer()))
            {
                if (auto hGlob = LoadResource (dllModule, res))
                {
                    auto* data = static_cast<const char*> (LockResource (hGlob));
                    return String::fromUTF8 (data, SizeofResource (dllModule, res));
                }
            }
        }

        return {};
    }
   #endif
  #else
    Handle resHandle = {};
    CFBundleRef bundleRef = {};

   #if JUCE_MAC
    CFBundleRefNum resFileId = {};
    FSSpec parentDirFSSpec;
   #endif

    bool open()
    {
        if (moduleMain != nullptr)
            return true;

        bool ok = false;

        if (file.hasFileExtension (".vst"))
        {
            auto* utf8 = file.getFullPathName().toRawUTF8();

            if (CFURLRef url = CFURLCreateFromFileSystemRepresentation (nullptr, (const UInt8*) utf8,
                                                                        (CFIndex) strlen (utf8), file.isDirectory()))
            {
                bundleRef = CFBundleCreate (kCFAllocatorDefault, url);
                CFRelease (url);

                if (bundleRef != nullptr)
                {
                    if (CFBundleLoadExecutable (bundleRef))
                    {
                        moduleMain = (MainCall) CFBundleGetFunctionPointerForName (bundleRef, CFSTR("main_macho"));

                        if (moduleMain == nullptr)
                            moduleMain = (MainCall) CFBundleGetFunctionPointerForName (bundleRef, CFSTR("VSTPluginMain"));

                        JUCE_VST_WRAPPER_LOAD_CUSTOM_MAIN

                        if (moduleMain != nullptr)
                        {
                            if (CFTypeRef name = CFBundleGetValueForInfoDictionaryKey (bundleRef, CFSTR("CFBundleName")))
                            {
                                if (CFGetTypeID (name) == CFStringGetTypeID())
                                {
                                    char buffer[1024];

                                    if (CFStringGetCString ((CFStringRef) name, buffer, sizeof (buffer), CFStringGetSystemEncoding()))
                                        pluginName = buffer;
                                }
                            }

                            if (pluginName.isEmpty())
                                pluginName = file.getFileNameWithoutExtension();

                           #if JUCE_MAC
                            resFileId = CFBundleOpenBundleResourceMap (bundleRef);
                           #endif

                            ok = true;

                            auto vstXmlFiles = file
                                                   #if JUCE_MAC
                                                    .getChildFile ("Contents")
                                                    .getChildFile ("Resources")
                                                   #endif
                                                    .findChildFiles (File::findFiles, false, "*.vstxml");

                            if (! vstXmlFiles.isEmpty())
                                vstXml = parseXML (vstXmlFiles.getReference(0));
                        }
                    }

                    if (! ok)
                    {
                        CFBundleUnloadExecutable (bundleRef);
                        CFRelease (bundleRef);
                        bundleRef = nullptr;
                    }
                }
            }
        }

        return ok;
    }

    void close()
    {
        if (bundleRef != nullptr)
        {
           #if JUCE_MAC
            CFBundleCloseBundleResourceMap (bundleRef, resFileId);
           #endif

            if (CFGetRetainCount (bundleRef) == 1)
                CFBundleUnloadExecutable (bundleRef);

            if (CFGetRetainCount (bundleRef) > 0)
                CFRelease (bundleRef);
        }
    }

    void closeEffect (Vst2::AEffect* eff)
    {
        eff->dispatcher (eff, Vst2::effClose, 0, 0, nullptr, 0);
    }

  #endif

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModuleHandle)
};

static const int defaultVSTSampleRateValue = 44100;
static const int defaultVSTBlockSizeValue = 512;

#if JUCE_MSVC
 #pragma warning (push)
 #pragma warning (disable: 4996) // warning about overriding deprecated methods
#endif

//==============================================================================
struct VSTPluginInstance     : public AudioPluginInstance,
                               private Timer,
                               private AsyncUpdater
{
    struct VSTParameter final   : public Parameter
    {
        VSTParameter (VSTPluginInstance& parent,
                      const String& paramName,
                      const Array<String>& shortParamNames,
                      float paramDefaultValue,
                      const String& paramLabel,
                      bool paramIsAutomatable,
                      bool paramIsDiscrete,
                      int numParamSteps,
                      bool isBoolSwitch,
                      const StringArray& paramValueStrings,
                      const VSTXMLInfo::ValueType* paramValueType)
            : pluginInstance (parent),
              name (paramName),
              shortNames (shortParamNames),
              defaultValue (paramDefaultValue),
              label (paramLabel),
              automatable (paramIsAutomatable),
              discrete (paramIsDiscrete),
              numSteps (numParamSteps),
              isSwitch (isBoolSwitch),
              vstValueStrings (paramValueStrings),
              valueType (paramValueType)
        {
        }

        float getValue() const override
        {
            if (auto* effect = pluginInstance.vstEffect)
            {
                const ScopedLock sl (pluginInstance.lock);

                return effect->getParameter (effect, getParameterIndex());
            }

            return 0.0f;
        }

        void setValue (float newValue) override
        {
            if (auto* effect = pluginInstance.vstEffect)
            {
                const ScopedLock sl (pluginInstance.lock);

                if (effect->getParameter (effect, getParameterIndex()) != newValue)
                    effect->setParameter (effect, getParameterIndex(), newValue);
            }
        }

        String getText (float value, int maximumStringLength) const override
        {
            if (valueType != nullptr)
            {
                for (auto& v : valueType->entries)
                    if (v->range.contains (value))
                        return v->name;
            }

            return Parameter::getText (value, maximumStringLength);
        }

        float getValueForText (const String& text) const override
        {
            if (valueType != nullptr)
            {
                for (auto& v : valueType->entries)
                    if (v->name == text)
                        return (v->range.high + v->range.low) / 2.0f;
            }

            return Parameter::getValueForText (text);
        }

        String getCurrentValueAsText() const override
        {
            if (valueType != nullptr || ! vstValueStrings.isEmpty())
                return getText (getValue(), 1024);

            return pluginInstance.getTextForOpcode (getParameterIndex(), Vst2::effGetParamDisplay);
        }

        float getDefaultValue() const override
        {
            return defaultValue;
        }

        String getName (int maximumStringLength) const override
        {
            if (name.isEmpty())
                return pluginInstance.getTextForOpcode (getParameterIndex(),
                                                        Vst2::effGetParamName);

            if (name.length() <= maximumStringLength)
                return name;

            if (! shortNames.isEmpty())
            {
                for (auto& n : shortNames)
                    if (n.length() <= maximumStringLength)
                        return n;

                return shortNames.getLast();
            }

            return name;
        }

        String getLabel() const override
        {
            return label.isEmpty() ? pluginInstance.getTextForOpcode (getParameterIndex(),
                                                                      Vst2::effGetParamLabel)
                                   : label;
        }

        bool isAutomatable() const override
        {
            return automatable;
        }

        bool isDiscrete() const override
        {
            return discrete;
        }

        bool isBoolean() const override
        {
            return isSwitch;
        }

        int getNumSteps() const override
        {
            return numSteps;
        }

        StringArray getAllValueStrings() const override
        {
            return vstValueStrings;
        }

        VSTPluginInstance& pluginInstance;

        const String name;
        const Array<String> shortNames;
        const float defaultValue;
        const String label;
        const bool automatable, discrete;
        const int numSteps;
        const bool isSwitch;
        const StringArray vstValueStrings;
        const VSTXMLInfo::ValueType* const valueType;
    };

    VSTPluginInstance (const ModuleHandle::Ptr& mh, const BusesProperties& ioConfig, Vst2::AEffect* effect,
                       double sampleRateToUse, int blockSizeToUse)
        : AudioPluginInstance (ioConfig),
          vstEffect (effect),
          vstModule (mh),
          name (mh->pluginName),
          bypassParam (new VST2BypassParameter (*this))
    {
        jassert (vstEffect != nullptr);

        if (auto* xml = vstModule->vstXml.get())
            xmlInfo.reset (VSTXMLInfo::createFor (*xml));

        refreshParameterList();

        vstSupportsBypass = (pluginCanDo ("bypass") > 0);
        setRateAndBufferSizeDetails (sampleRateToUse, blockSizeToUse);
    }

    void refreshParameterList() override
    {
        AudioProcessorParameterGroup newParameterTree;

        for (int i = 0; i < vstEffect->numParams; ++i)
        {
            String paramName;
            Array<String> shortParamNames;
            float defaultValue = 0;
            String label;
            bool isAutomatable = dispatch (Vst2::effCanBeAutomated, i, 0, nullptr, 0) != 0;
            bool isDiscrete = false;
            int numSteps = AudioProcessor::getDefaultNumParameterSteps();
            bool isBoolSwitch = false;
            StringArray parameterValueStrings;
            const VSTXMLInfo::ValueType* valueType = nullptr;

            if (xmlInfo != nullptr)
            {
                if (auto* param = xmlInfo->getParamForID (i, nullptr))
                {
                    paramName = param->name;

                    for (auto& n : param->shortNames)
                        shortParamNames.add (n);

                    struct LengthComparator
                    {
                        static int compareElements (const juce::String& first, const juce::String& second) noexcept
                        {
                            return first.length() - second.length();
                        }
                    };

                    LengthComparator comp;
                    shortParamNames.sort (comp);

                    defaultValue = param->defaultValue;
                    label = param->label;

                    if (param->type == "switch")
                    {
                        isBoolSwitch = true;
                        numSteps = 2;
                        valueType = &xmlInfo->switchValueType;
                    }
                    else
                    {
                        valueType = xmlInfo->getValueType (param->type);
                    }

                    if (param->numberOfStates >= 2)
                    {
                        numSteps = param->numberOfStates;

                        if (valueType != nullptr)
                        {
                            for (auto* entry : valueType->entries)
                                parameterValueStrings.add (entry->name);

                            parameterValueStrings.removeEmptyStrings();
                        }
                    }

                    isDiscrete = (numSteps != AudioProcessor::getDefaultNumParameterSteps());
                }
            }

            newParameterTree.addChild (std::make_unique<VSTParameter> (*this, paramName, shortParamNames, defaultValue,
                                                                       label, isAutomatable, isDiscrete, numSteps,
                                                                       isBoolSwitch, parameterValueStrings, valueType));
        }

        setParameterTree (std::move (newParameterTree));
    }

    ~VSTPluginInstance() override
    {
        if (vstEffect != nullptr && vstEffect->magic == 0x56737450 /* 'VstP' */)
        {
            struct VSTDeleter : public CallbackMessage
            {
                VSTDeleter (VSTPluginInstance& inInstance, WaitableEvent& inEvent)
                    : vstInstance (inInstance), completionSignal (inEvent)
                {}

                void messageCallback() override
                {
                    vstInstance.cleanup();
                    completionSignal.signal();
                }

                VSTPluginInstance& vstInstance;
                WaitableEvent& completionSignal;
            };

            if (MessageManager::getInstance()->isThisTheMessageThread())
            {
                cleanup();
            }
            else
            {
                WaitableEvent completionEvent;
                (new VSTDeleter (*this, completionEvent))->post();
                completionEvent.wait();
            }
        }
    }

    void cleanup()
    {
        if (vstEffect != nullptr && vstEffect->magic == 0x56737450 /* 'VstP' */)
        {
           #if JUCE_MAC
            if (vstModule->resFileId != 0)
                UseResFile (vstModule->resFileId);
           #endif

            // Must delete any editors before deleting the plugin instance!
            jassert (getActiveEditor() == nullptr);

            _fpreset(); // some dodgy plug-ins mess around with this

            vstModule->closeEffect (vstEffect);
        }

        vstModule = nullptr;
        vstEffect = nullptr;
    }

    static VSTPluginInstance* create (const ModuleHandle::Ptr& newModule,
                                      double initialSampleRate,
                                      int initialBlockSize)
    {
        if (auto* newEffect = constructEffect (newModule))
        {
            newEffect->resvd2 = 0;

            newEffect->dispatcher (newEffect, Vst2::effIdentify, 0, 0, nullptr, 0);

            auto blockSize = jmax (32, initialBlockSize);

            newEffect->dispatcher (newEffect, Vst2::effSetSampleRate, 0, 0, nullptr, static_cast<float> (initialSampleRate));
            newEffect->dispatcher (newEffect, Vst2::effSetBlockSize,  0, blockSize, nullptr, 0);

            newEffect->dispatcher (newEffect, Vst2::effOpen, 0, 0, nullptr, 0);
            BusesProperties ioConfig = queryBusIO (newEffect);

            return new VSTPluginInstance (newModule, ioConfig, newEffect, initialSampleRate, blockSize);
        }

        return nullptr;
    }

    //==============================================================================
    void fillInPluginDescription (PluginDescription& desc) const override
    {
        desc.name = name;

        {
            char buffer[512] = { 0 };
            dispatch (Vst2::effGetEffectName, 0, 0, buffer, 0);

            desc.descriptiveName = String::createStringFromData (buffer, (int) sizeof (buffer)).trim();

            if (desc.descriptiveName.isEmpty())
                desc.descriptiveName = name;
        }

        desc.fileOrIdentifier = vstModule->file.getFullPathName();
        desc.uid = getUID();
        desc.lastFileModTime = vstModule->file.getLastModificationTime();
        desc.lastInfoUpdateTime = Time::getCurrentTime();
        desc.pluginFormatName = "VST";
        desc.category = getCategory();

        {
            char buffer[512] = { 0 };
            dispatch (Vst2::effGetVendorString, 0, 0, buffer, 0);
            desc.manufacturerName = String::createStringFromData (buffer, (int) sizeof (buffer)).trim();
        }

        desc.version = getVersion();
        desc.numInputChannels = getTotalNumInputChannels();
        desc.numOutputChannels = getTotalNumOutputChannels();
        desc.isInstrument = isSynthPlugin();
    }

    bool initialiseEffect (double initialSampleRate, int initialBlockSize)
    {
        if (vstEffect != nullptr)
        {
            vstEffect->resvd2 = (pointer_sized_int) (pointer_sized_int) this;
            initialise (initialSampleRate, initialBlockSize);
            return true;
        }

        return false;
    }

    void initialise (double initialSampleRate, int initialBlockSize)
    {
        if (initialised || vstEffect == nullptr)
            return;

       #if JUCE_WINDOWS
        // On Windows it's highly advisable to create your plugins using the message thread,
        // because many plugins need a chance to create HWNDs that will get their
        // messages delivered by the main message thread, and that's not possible from
        // a background thread.
        JUCE_ASSERT_MESSAGE_THREAD
       #endif

        JUCE_VST_LOG ("Initialising VST: " + vstModule->pluginName + " (" + getVersion() + ")");
        initialised = true;

        setRateAndBufferSizeDetails (initialSampleRate, initialBlockSize);

        dispatch (Vst2::effIdentify, 0, 0, nullptr, 0);

        if (getSampleRate() > 0)
            dispatch (Vst2::effSetSampleRate, 0, 0, nullptr, (float) getSampleRate());

        if (getBlockSize() > 0)
            dispatch (Vst2::effSetBlockSize, 0, jmax (32, getBlockSize()), nullptr, 0);

        dispatch (Vst2::effOpen, 0, 0, nullptr, 0);

        setRateAndBufferSizeDetails (getSampleRate(), getBlockSize());

        if (getNumPrograms() > 1)
            setCurrentProgram (0);
        else
            dispatch (Vst2::effSetProgram, 0, 0, nullptr, 0);

        for (int i = vstEffect->numInputs;  --i >= 0;)  dispatch (Vst2::effConnectInput,  i, 1, nullptr, 0);
        for (int i = vstEffect->numOutputs; --i >= 0;)  dispatch (Vst2::effConnectOutput, i, 1, nullptr, 0);

        if (getVstCategory() != Vst2::kPlugCategShell) // (workaround for Waves 5 plugins which crash during this call)
            updateStoredProgramNames();

        wantsMidiMessages = pluginCanDo ("receiveVstMidiEvent") > 0 || isSynthPlugin();

       #if JUCE_MAC && JUCE_SUPPORT_CARBON
        usesCocoaNSView = ((unsigned int) pluginCanDo ("hasCockosViewAsConfig") & 0xffff0000ul) == 0xbeef0000ul;
       #endif

        setLatencySamples (vstEffect->initialDelay);
    }

    void* getPlatformSpecificData() override    { return vstEffect; }

    const String getName() const override
    {
        if (vstEffect != nullptr)
        {
            char buffer[512] = { 0 };

            if (dispatch (Vst2::effGetProductString, 0, 0, buffer, 0) != 0)
            {
                String productName = String::createStringFromData (buffer, (int) sizeof (buffer));

                if (productName.isNotEmpty())
                    return productName;
            }
        }

        return name;
    }

    int getUID() const
    {
        int uid = vstEffect != nullptr ? vstEffect->uniqueID : 0;

        if (uid == 0)
            uid = vstModule->file.hashCode();

        return uid;
    }

    double getTailLengthSeconds() const override
    {
        if (vstEffect == nullptr)
            return 0.0;

        if ((vstEffect->flags & Vst2::effFlagsNoSoundInStop) != 0)
            return 0.0;

        auto tailSize = dispatch (Vst2::effGetTailSize, 0, 0, nullptr, 0);
        auto sampleRate = getSampleRate();

        // remain backward compatible with old JUCE plug-ins: anything larger
        // than INT32_MAX is an invalid tail time but old JUCE 64-bit plug-ins
        // would return INT64_MAX for infinite tail time. So treat anything
        // equal or greater than INT32_MAX as infinite tail time.
        if (tailSize >= std::numeric_limits<int32>::max())
            return std::numeric_limits<double>::infinity();

        if (tailSize >= 0 && sampleRate > 0)
            return static_cast<double> (tailSize) / sampleRate;

        return 0.0;
    }

    bool acceptsMidi() const override    { return wantsMidiMessages; }
    bool producesMidi() const override   { return pluginCanDo ("sendVstMidiEvent") > 0; }
    bool supportsMPE() const override    { return pluginCanDo ("MPE") > 0; }

    Vst2::VstPlugCategory getVstCategory() const noexcept     { return (Vst2::VstPlugCategory) dispatch (Vst2::effGetPlugCategory, 0, 0, nullptr, 0); }

    bool isSynthPlugin() const  { return (vstEffect != nullptr && (vstEffect->flags & Vst2::effFlagsIsSynth) != 0); }

    int pluginCanDo (const char* text) const  { return (int) dispatch (Vst2::effCanDo, 0, 0, (void*) text,  0); }

    //==============================================================================
    void prepareToPlay (double rate, int samplesPerBlockExpected) override
    {
        auto numInputBuses  = getBusCount (true);
        auto numOutputBuses = getBusCount (false);

        setRateAndBufferSizeDetails (rate, samplesPerBlockExpected);

        if (numInputBuses <= 1 && numOutputBuses <= 1)
        {
            SpeakerMappings::VstSpeakerConfigurationHolder inArr  (getChannelLayoutOfBus (true,  0));
            SpeakerMappings::VstSpeakerConfigurationHolder outArr (getChannelLayoutOfBus (false, 0));

            dispatch (Vst2::effSetSpeakerArrangement, 0, (pointer_sized_int) &inArr.get(), (void*) &outArr.get(), 0.0f);
        }

        vstHostTime.tempo = 120.0;
        vstHostTime.timeSigNumerator = 4;
        vstHostTime.timeSigDenominator = 4;
        vstHostTime.sampleRate = rate;
        vstHostTime.samplePos = 0;
        vstHostTime.flags = Vst2::kVstNanosValid
                              | Vst2::kVstAutomationWriting
                              | Vst2::kVstAutomationReading;

        initialise (rate, samplesPerBlockExpected);

        if (initialised)
        {
            wantsMidiMessages = wantsMidiMessages || (pluginCanDo ("receiveVstMidiEvent") > 0) || isSynthPlugin();

            if (wantsMidiMessages)
                midiEventsToSend.ensureSize (256);
            else
                midiEventsToSend.freeEvents();

            incomingMidi.clear();

            dispatch (Vst2::effSetSampleRate, 0, 0, nullptr, (float) rate);
            dispatch (Vst2::effSetBlockSize, 0, jmax (16, samplesPerBlockExpected), nullptr, 0);

            if (supportsDoublePrecisionProcessing())
            {
                int32 vstPrecision = isUsingDoublePrecision() ? Vst2::kVstProcessPrecision64
                                                              : Vst2::kVstProcessPrecision32;

                dispatch (Vst2::effSetProcessPrecision, 0, (pointer_sized_int) vstPrecision, nullptr, 0);
            }

            auto maxChannels = jmax (1, jmax (vstEffect->numInputs, vstEffect->numOutputs));

            tmpBufferFloat .setSize (maxChannels, samplesPerBlockExpected);
            tmpBufferDouble.setSize (maxChannels, samplesPerBlockExpected);

            channelBufferFloat .calloc (static_cast<size_t> (maxChannels));
            channelBufferDouble.calloc (static_cast<size_t> (maxChannels));

            outOfPlaceBuffer.setSize (jmax (1, vstEffect->numOutputs), samplesPerBlockExpected);

            if (! isPowerOn)
                setPower (true);

            // dodgy hack to force some plugins to initialise the sample rate..
            if (! hasEditor())
            {
                if (auto* firstParam = getParameters()[0])
                {
                    auto old = firstParam->getValue();
                    firstParam->setValue ((old < 0.5f) ? 1.0f : 0.0f);
                    firstParam->setValue (old);
                }
            }

            dispatch (Vst2::effStartProcess, 0, 0, nullptr, 0);

            setLatencySamples (vstEffect->initialDelay);
        }
    }

    void releaseResources() override
    {
        if (initialised)
        {
            dispatch (Vst2::effStopProcess, 0, 0, nullptr, 0);
            setPower (false);
        }

        channelBufferFloat.free();
        tmpBufferFloat.setSize (0, 0);

        channelBufferDouble.free();
        tmpBufferDouble.setSize (0, 0);

        outOfPlaceBuffer.setSize (1, 1);
        incomingMidi.clear();

        midiEventsToSend.freeEvents();
    }

    void reset() override
    {
        if (isPowerOn)
        {
            setPower (false);
            setPower (true);
        }
    }

    //==============================================================================
    void processBlock (AudioBuffer<float>& buffer, MidiBuffer& midiMessages) override
    {
        jassert (! isUsingDoublePrecision());
        processAudio (buffer, midiMessages, tmpBufferFloat, channelBufferFloat, false);
    }

    void processBlock (AudioBuffer<double>& buffer, MidiBuffer& midiMessages) override
    {
        jassert (isUsingDoublePrecision());
        processAudio (buffer, midiMessages, tmpBufferDouble, channelBufferDouble, false);
    }

    void processBlockBypassed (AudioBuffer<float>& buffer, MidiBuffer& midiMessages) override
    {
        jassert (! isUsingDoublePrecision());
        processAudio (buffer, midiMessages, tmpBufferFloat, channelBufferFloat, true);
    }

    void processBlockBypassed (AudioBuffer<double>& buffer, MidiBuffer& midiMessages) override
    {
        jassert (isUsingDoublePrecision());
        processAudio (buffer, midiMessages, tmpBufferDouble, channelBufferDouble, true);
    }

    //==============================================================================
    bool supportsDoublePrecisionProcessing() const override
    {
        return ((vstEffect->flags & Vst2::effFlagsCanReplacing) != 0
             && (vstEffect->flags & Vst2::effFlagsCanDoubleReplacing) != 0);
    }

    AudioProcessorParameter* getBypassParameter() const override               { return vstSupportsBypass ? bypassParam.get() : nullptr; }

    //==============================================================================
    bool canAddBus (bool) const override                                       { return false; }
    bool canRemoveBus (bool) const override                                    { return false; }

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        auto numInputBuses  = getBusCount (true);
        auto numOutputBuses = getBusCount (false);

        // it's not possible to change layout if there are sidechains/aux buses
        if (numInputBuses > 1 || numOutputBuses > 1)
            return (layouts == getBusesLayout());

        return (layouts.getNumChannels (true,  0) <= vstEffect->numInputs
             && layouts.getNumChannels (false, 0) <= vstEffect->numOutputs);
    }

    //==============================================================================
   #if JUCE_IOS || JUCE_ANDROID
    bool hasEditor() const override                  { return false; }
   #else
    bool hasEditor() const override                  { return vstEffect != nullptr && (vstEffect->flags & Vst2::effFlagsHasEditor) != 0; }
   #endif

    AudioProcessorEditor* createEditor() override;

    //==============================================================================
    const String getInputChannelName (int index) const override
    {
        if (isValidChannel (index, true))
        {
            Vst2::VstPinProperties pinProps;
            if (dispatch (Vst2::effGetInputProperties, index, 0, &pinProps, 0.0f) != 0)
                return String (pinProps.label, sizeof (pinProps.label));
        }

        return {};
    }

    bool isInputChannelStereoPair (int index) const override
    {
        if (! isValidChannel (index, true))
            return false;

        Vst2::VstPinProperties pinProps;
        if (dispatch (Vst2::effGetInputProperties, index, 0, &pinProps, 0.0f) != 0)
            return (pinProps.flags & Vst2::kVstPinIsStereo) != 0;

        return true;
    }

    const String getOutputChannelName (int index) const override
    {
        if (isValidChannel (index, false))
        {
            Vst2::VstPinProperties pinProps;
            if (dispatch (Vst2::effGetOutputProperties, index, 0, &pinProps, 0.0f) != 0)
                return String (pinProps.label, sizeof (pinProps.label));
        }

        return {};
    }

    bool isOutputChannelStereoPair (int index) const override
    {
        if (! isValidChannel (index, false))
            return false;

        Vst2::VstPinProperties pinProps;
        if (dispatch (Vst2::effGetOutputProperties, index, 0, &pinProps, 0.0f) != 0)
            return (pinProps.flags & Vst2::kVstPinIsStereo) != 0;

        return true;
    }

    bool isValidChannel (int index, bool isInput) const noexcept
    {
        return isPositiveAndBelow (index, isInput ? getTotalNumInputChannels()
                                                  : getTotalNumOutputChannels());
    }

    //==============================================================================
    int getNumPrograms() override          { return vstEffect != nullptr ? jmax (0, vstEffect->numPrograms) : 0; }

    // NB: some plugs return negative numbers from this function.
    int getCurrentProgram() override       { return (int) dispatch (Vst2::effGetProgram, 0, 0, nullptr, 0); }

    void setCurrentProgram (int newIndex) override
    {
        if (getNumPrograms() > 0 && newIndex != getCurrentProgram())
            dispatch (Vst2::effSetProgram, 0, jlimit (0, getNumPrograms() - 1, newIndex), nullptr, 0);
    }

    const String getProgramName (int index) override
    {
        if (index >= 0)
        {
            if (index == getCurrentProgram())
                return getCurrentProgramName();

            if (vstEffect != nullptr)
            {
                char nm[264] = { 0 };

                if (dispatch (Vst2::effGetProgramNameIndexed, jlimit (0, getNumPrograms(), index), -1, nm, 0) != 0)
                    return String::fromUTF8 (nm).trim();
            }
        }

        return programNames [index];
    }

    void changeProgramName (int index, const String& newName) override
    {
        if (index >= 0 && index == getCurrentProgram())
        {
            if (getNumPrograms() > 0 && newName != getCurrentProgramName())
                dispatch (Vst2::effSetProgramName, 0, 0, (void*) newName.substring (0, 24).toRawUTF8(), 0.0f);
        }
        else
        {
            jassertfalse; // xxx not implemented!
        }
    }

    //==============================================================================
    void getStateInformation (MemoryBlock& mb) override                  { saveToFXBFile (mb, true); }
    void getCurrentProgramStateInformation (MemoryBlock& mb) override    { saveToFXBFile (mb, false); }

    void setStateInformation (const void* data, int size) override               { loadFromFXBFile (data, (size_t) size); }
    void setCurrentProgramStateInformation (const void* data, int size) override { loadFromFXBFile (data, (size_t) size); }

    //==============================================================================
    void timerCallback() override
    {
        if (dispatch (Vst2::effIdle, 0, 0, nullptr, 0) == 0)
            stopTimer();
    }

    void handleAsyncUpdate() override
    {
        // indicates that something about the plugin has changed..
        updateHostDisplay();
    }

    pointer_sized_int handleCallback (int32 opcode, int32 index, pointer_sized_int value, void* ptr, float opt)
    {
        switch (opcode)
        {
            case Vst2::audioMasterAutomate:
                if (auto* param = getParameters()[index])
                    param->sendValueChangedMessageToListeners (opt);
                else
                    jassertfalse; // Invalid parameter index!

                break;

            case Vst2::audioMasterProcessEvents:            handleMidiFromPlugin ((const Vst2::VstEvents*) ptr); break;
            case Vst2::audioMasterGetTime:                  return getVSTTime();
            case Vst2::audioMasterIdle:                     handleIdle(); break;
            case Vst2::audioMasterSizeWindow:               setWindowSize (index, (int) value); return 1;
            case Vst2::audioMasterUpdateDisplay:            triggerAsyncUpdate(); break;
            case Vst2::audioMasterIOChanged:                setLatencySamples (vstEffect->initialDelay); break;
            case Vst2::audioMasterNeedIdle:                 startTimer (50); break;

            case Vst2::audioMasterGetSampleRate:            return (pointer_sized_int) (getSampleRate() > 0 ? getSampleRate() : defaultVSTSampleRateValue);
            case Vst2::audioMasterGetBlockSize:             return (pointer_sized_int) (getBlockSize() > 0  ? getBlockSize()  : defaultVSTBlockSizeValue);
            case Vst2::audioMasterWantMidi:                 wantsMidiMessages = true; break;
            case Vst2::audioMasterGetDirectory:             return getVstDirectory();

            case Vst2::audioMasterTempoAt:                  return (pointer_sized_int) (extraFunctions != nullptr ? extraFunctions->getTempoAt ((int64) value) : 0);
            case Vst2::audioMasterGetAutomationState:       return (pointer_sized_int) (extraFunctions != nullptr ? extraFunctions->getAutomationState() : 0);

            case Vst2::audioMasterBeginEdit:
                if (auto* param = getParameters()[index])
                    param->beginChangeGesture();
                else
                    jassertfalse; // Invalid parameter index!

                break;

            case Vst2::audioMasterEndEdit:
                if (auto* param = getParameters()[index])
                    param->endChangeGesture();
                else
                    jassertfalse; // Invalid parameter index!

                break;

            case Vst2::audioMasterPinConnected:             return isValidChannel (index, value == 0) ? 0 : 1; // (yes, 0 = true)
            case Vst2::audioMasterGetCurrentProcessLevel:   return isNonRealtime() ? 4 : 0;

            // none of these are handled (yet)...
            case Vst2::audioMasterSetTime:
            case Vst2::audioMasterGetParameterQuantization:
            case Vst2::audioMasterGetInputLatency:
            case Vst2::audioMasterGetOutputLatency:
            case Vst2::audioMasterGetPreviousPlug:
            case Vst2::audioMasterGetNextPlug:
            case Vst2::audioMasterWillReplaceOrAccumulate:
            case Vst2::audioMasterOfflineStart:
            case Vst2::audioMasterOfflineRead:
            case Vst2::audioMasterOfflineWrite:
            case Vst2::audioMasterOfflineGetCurrentPass:
            case Vst2::audioMasterOfflineGetCurrentMetaPass:
            case Vst2::audioMasterGetOutputSpeakerArrangement:
            case Vst2::audioMasterVendorSpecific:
            case Vst2::audioMasterSetIcon:
            case Vst2::audioMasterGetLanguage:
            case Vst2::audioMasterOpenWindow:
            case Vst2::audioMasterCloseWindow:
                break;

            default:
                return handleGeneralCallback (opcode, index, value, ptr, opt);
        }

        return 0;
    }

    // handles non plugin-specific callbacks..
    static pointer_sized_int handleGeneralCallback (int32 opcode, int32 /*index*/, pointer_sized_int /*value*/, void* ptr, float /*opt*/)
    {
        switch (opcode)
        {
            case Vst2::audioMasterCanDo:                        return handleCanDo ((const char*) ptr);
            case Vst2::audioMasterVersion:                      return 2400;
            case Vst2::audioMasterCurrentId:                    return shellUIDToCreate;
            case Vst2::audioMasterGetNumAutomatableParameters:  return 0;
            case Vst2::audioMasterGetAutomationState:           return 1;
            case Vst2::audioMasterGetVendorVersion:             return 0x0101;

            case Vst2::audioMasterGetVendorString:
            case Vst2::audioMasterGetProductString:             return getHostName ((char*) ptr);

            case Vst2::audioMasterGetSampleRate:                return (pointer_sized_int) defaultVSTSampleRateValue;
            case Vst2::audioMasterGetBlockSize:                 return (pointer_sized_int) defaultVSTBlockSizeValue;
            case Vst2::audioMasterSetOutputSampleRate:          return 0;

            default:
                DBG ("*** Unhandled VST Callback: " + String ((int) opcode));
                break;
        }

        return 0;
    }

    //==============================================================================
    pointer_sized_int dispatch (int opcode, int index, pointer_sized_int value, void* const ptr, float opt) const
    {
        pointer_sized_int result = 0;

        if (vstEffect != nullptr)
        {
            const ScopedLock sl (lock);
            const IdleCallRecursionPreventer icrp;

            try
            {
               #if JUCE_MAC
                auto oldResFile = CurResFile();

                if (vstModule->resFileId != 0)
                    UseResFile (vstModule->resFileId);
               #endif

                result = vstEffect->dispatcher (vstEffect, opcode, index, value, ptr, opt);

               #if JUCE_MAC
                auto newResFile = CurResFile();

                if (newResFile != oldResFile)  // avoid confusing the parent app's resource file with the plug-in's
                {
                    vstModule->resFileId = newResFile;
                    UseResFile (oldResFile);
                }
               #endif
            }
            catch (...)
            {}
        }

        return result;
    }

    bool loadFromFXBFile (const void* const data, const size_t dataSize)
    {
        if (dataSize < 28)
            return false;

        auto set = (const fxSet*) data;

        if ((! compareMagic (set->chunkMagic, "CcnK")) || fxbSwap (set->version) > fxbVersionNum)
            return false;

        if (compareMagic (set->fxMagic, "FxBk"))
        {
            // bank of programs
            if (fxbSwap (set->numPrograms) >= 0)
            {
                auto oldProg = getCurrentProgram();
                auto numParams = fxbSwap (((const fxProgram*) (set->programs))->numParams);
                auto progLen = (int) sizeof (fxProgram) + (numParams - 1) * (int) sizeof (float);

                for (int i = 0; i < fxbSwap (set->numPrograms); ++i)
                {
                    if (i != oldProg)
                    {
                        auto prog = (const fxProgram*) (((const char*) (set->programs)) + i * progLen);

                        if (((const char*) prog) - ((const char*) set) >= (ssize_t) dataSize)
                            return false;

                        if (fxbSwap (set->numPrograms) > 0)
                            setCurrentProgram (i);

                        if (! restoreProgramSettings (prog))
                            return false;
                    }
                }

                if (fxbSwap (set->numPrograms) > 0)
                    setCurrentProgram (oldProg);

                auto prog = (const fxProgram*) (((const char*) (set->programs)) + oldProg * progLen);

                if (((const char*) prog) - ((const char*) set) >= (ssize_t) dataSize)
                    return false;

                if (! restoreProgramSettings (prog))
                    return false;
            }
        }
        else if (compareMagic (set->fxMagic, "FxCk"))
        {
            // single program
            auto prog = (const fxProgram*) data;

            if (! compareMagic (prog->chunkMagic, "CcnK"))
                return false;

            changeProgramName (getCurrentProgram(), prog->prgName);

            for (int i = 0; i < fxbSwap (prog->numParams); ++i)
                if (auto* param = getParameters()[i])
                    param->setValue (fxbSwapFloat (prog->params[i]));
        }
        else if (compareMagic (set->fxMagic, "FBCh"))
        {
            // non-preset chunk
            auto cset = (const fxChunkSet*) data;

            if ((size_t) fxbSwap (cset->chunkSize) + sizeof (fxChunkSet) - 8 > (size_t) dataSize)
                return false;

            setChunkData (cset->chunk, fxbSwap (cset->chunkSize), false);
        }
        else if (compareMagic (set->fxMagic, "FPCh"))
        {
            // preset chunk
            auto cset = (const fxProgramSet*) data;

            if ((size_t) fxbSwap (cset->chunkSize) + sizeof (fxProgramSet) - 8 > (size_t) dataSize)
                return false;

            setChunkData (cset->chunk, fxbSwap (cset->chunkSize), true);

            changeProgramName (getCurrentProgram(), cset->name);
        }
        else
        {
            return false;
        }

        return true;
    }

    bool saveToFXBFile (MemoryBlock& dest, bool isFXB, int maxSizeMB = 128)
    {
        auto numPrograms = getNumPrograms();
        auto numParams = getParameters().size();

        if (usesChunks())
        {
            MemoryBlock chunk;
            getChunkData (chunk, ! isFXB, maxSizeMB);

            if (isFXB)
            {
                auto totalLen = sizeof (fxChunkSet) + chunk.getSize() - 8;
                dest.setSize (totalLen, true);

                auto set = (fxChunkSet*) dest.getData();
                set->chunkMagic = fxbName ("CcnK");
                set->byteSize = 0;
                set->fxMagic = fxbName ("FBCh");
                set->version = fxbSwap (fxbVersionNum);
                set->fxID = fxbSwap (getUID());
                set->fxVersion = fxbSwap (getVersionNumber());
                set->numPrograms = fxbSwap (numPrograms);
                set->chunkSize = fxbSwap ((int32) chunk.getSize());

                chunk.copyTo (set->chunk, 0, chunk.getSize());
            }
            else
            {
                auto totalLen = sizeof (fxProgramSet) + chunk.getSize() - 8;
                dest.setSize (totalLen, true);

                auto set = (fxProgramSet*) dest.getData();
                set->chunkMagic = fxbName ("CcnK");
                set->byteSize = 0;
                set->fxMagic = fxbName ("FPCh");
                set->version = fxbSwap (fxbVersionNum);
                set->fxID = fxbSwap (getUID());
                set->fxVersion = fxbSwap (getVersionNumber());
                set->numPrograms = fxbSwap (numPrograms);
                set->chunkSize = fxbSwap ((int32) chunk.getSize());

                getCurrentProgramName().copyToUTF8 (set->name, sizeof (set->name) - 1);
                chunk.copyTo (set->chunk, 0, chunk.getSize());
            }
        }
        else
        {
            if (isFXB)
            {
                auto progLen = (int) sizeof (fxProgram) + (numParams - 1) * (int) sizeof (float);
                auto len = (size_t) (progLen * jmax (1, numPrograms)) + (sizeof (fxSet) - sizeof (fxProgram));
                dest.setSize (len, true);

                auto set = (fxSet*) dest.getData();
                set->chunkMagic = fxbName ("CcnK");
                set->byteSize = 0;
                set->fxMagic = fxbName ("FxBk");
                set->version = fxbSwap (fxbVersionNum);
                set->fxID = fxbSwap (getUID());
                set->fxVersion = fxbSwap (getVersionNumber());
                set->numPrograms = fxbSwap (numPrograms);

                MemoryBlock oldSettings;
                createTempParameterStore (oldSettings);

                auto oldProgram = getCurrentProgram();

                if (oldProgram >= 0)
                    setParamsInProgramBlock ((fxProgram*) (((char*) (set->programs)) + oldProgram * progLen));

                for (int i = 0; i < numPrograms; ++i)
                {
                    if (i != oldProgram)
                    {
                        setCurrentProgram (i);
                        setParamsInProgramBlock ((fxProgram*) (((char*) (set->programs)) + i * progLen));
                    }
                }

                if (oldProgram >= 0)
                    setCurrentProgram (oldProgram);

                restoreFromTempParameterStore (oldSettings);
            }
            else
            {
                dest.setSize ((size_t) ((numParams - 1) * (int) sizeof (float)) + sizeof (fxProgram), true);
                setParamsInProgramBlock ((fxProgram*) dest.getData());
            }
        }

        return true;
    }

    bool usesChunks() const noexcept        { return vstEffect != nullptr && (vstEffect->flags & Vst2::effFlagsProgramChunks) != 0; }

    bool getChunkData (MemoryBlock& mb, bool isPreset, int maxSizeMB) const
    {
        if (usesChunks())
        {
            void* data = nullptr;
            auto bytes = (size_t) dispatch (Vst2::effGetChunk, isPreset ? 1 : 0, 0, &data, 0.0f);

            if (data != nullptr && bytes <= (size_t) maxSizeMB * 1024 * 1024)
            {
                mb.setSize (bytes);
                mb.copyFrom (data, 0, bytes);

                return true;
            }
        }

        return false;
    }

    bool setChunkData (const void* data, const int size, bool isPreset)
    {
        if (size > 0 && usesChunks())
        {
            dispatch (Vst2::effSetChunk, isPreset ? 1 : 0, size, (void*) data, 0.0f);

            if (! isPreset)
                updateStoredProgramNames();

            return true;
        }

        return false;
    }

    Vst2::AEffect* vstEffect;
    ModuleHandle::Ptr vstModule;

    std::unique_ptr<VSTPluginFormat::ExtraFunctions> extraFunctions;
    bool usesCocoaNSView = false;

private:
    //==============================================================================
    struct VST2BypassParameter    : Parameter
    {
        VST2BypassParameter (VSTPluginInstance& effectToUse)
            : parent (effectToUse),
              vstOnStrings (TRANS("on"), TRANS("yes"), TRANS("true")),
              vstOffStrings (TRANS("off"), TRANS("no"), TRANS("false")),
              values (TRANS("Off"), TRANS("On"))
        {
        }

        void setValue (float newValue) override
        {
            currentValue = (newValue != 0.0f);

            if (parent.vstSupportsBypass)
                parent.dispatch (Vst2::effSetBypass, 0, currentValue ? 1 : 0, nullptr, 0.0f);
        }

        float getValueForText (const String& text) const override
        {
            String lowercaseText (text.toLowerCase());

            for (auto& testText : vstOnStrings)
                if (lowercaseText == testText)
                    return 1.0f;

            for (auto& testText : vstOffStrings)
                if (lowercaseText == testText)
                    return 0.0f;

            return text.getIntValue() != 0 ? 1.0f : 0.0f;
        }

        float getValue() const override                                     { return currentValue; }
        float getDefaultValue() const override                              { return 0.0f; }
        String getName (int /*maximumStringLength*/) const override         { return "Bypass"; }
        String getText (float value, int) const override                    { return (value != 0.0f ? TRANS("On") : TRANS("Off")); }
        bool isAutomatable() const override                                 { return true; }
        bool isDiscrete() const override                                    { return true; }
        bool isBoolean() const override                                     { return true; }
        int getNumSteps() const override                                    { return 2; }
        StringArray getAllValueStrings() const override                     { return values; }
        String getLabel() const override                                    { return {}; }

        VSTPluginInstance& parent;
        bool currentValue = false;
        StringArray vstOnStrings, vstOffStrings, values;
    };

    //==============================================================================
    String name;
    CriticalSection lock;
    std::atomic<bool> wantsMidiMessages { false };
    bool initialised = false;
    std::atomic<bool> isPowerOn { false };
    bool lastProcessBlockCallWasBypass = false, vstSupportsBypass = false;
    mutable StringArray programNames;
    AudioBuffer<float> outOfPlaceBuffer;

    CriticalSection midiInLock;
    MidiBuffer incomingMidi;
    VSTMidiEventList midiEventsToSend;
    Vst2::VstTimeInfo vstHostTime;

    AudioBuffer<float> tmpBufferFloat;
    HeapBlock<float*> channelBufferFloat;

    AudioBuffer<double> tmpBufferDouble;
    HeapBlock<double*> channelBufferDouble;
    std::unique_ptr<VST2BypassParameter> bypassParam;

    std::unique_ptr<VSTXMLInfo> xmlInfo;

    static pointer_sized_int handleCanDo (const char* name)
    {
        static const char* canDos[] = { "supplyIdle",
                                        "sendVstEvents",
                                        "sendVstMidiEvent",
                                        "sendVstTimeInfo",
                                        "receiveVstEvents",
                                        "receiveVstMidiEvent",
                                        "supportShell",
                                        "sizeWindow",
                                        "shellCategory" };

        for (int i = 0; i < numElementsInArray (canDos); ++i)
            if (strcmp (canDos[i], name) == 0)
                return 1;

        return 0;
    }

    static pointer_sized_int getHostName (char* name)
    {
        String hostName ("Juce VST Host");

        if (auto* app = JUCEApplicationBase::getInstance())
            hostName = app->getApplicationName();

        hostName.copyToUTF8 (name, (size_t) jmin (Vst2::kVstMaxVendorStrLen, Vst2::kVstMaxProductStrLen) - 1);
        return 1;
    }

    pointer_sized_int getVSTTime() noexcept
    {
       #if JUCE_MSVC
        #pragma warning (push)
        #pragma warning (disable: 4311)
       #endif

        return (pointer_sized_int) &vstHostTime;

       #if JUCE_MSVC
        #pragma warning (pop)
       #endif
    }

    void handleIdle()
    {
        if (insideVSTCallback == 0 && MessageManager::getInstance()->isThisTheMessageThread())
        {
            const IdleCallRecursionPreventer icrp;

           #if JUCE_MAC
            if (getActiveEditor() != nullptr)
                dispatch (Vst2::effEditIdle, 0, 0, nullptr, 0);
           #endif

            Timer::callPendingTimersSynchronously();
            handleUpdateNowIfNeeded();

            for (int i = ComponentPeer::getNumPeers(); --i >= 0;)
                if (auto* p = ComponentPeer::getPeer(i))
                    p->performAnyPendingRepaintsNow();
        }
    }

    void setWindowSize (int width, int height)
    {
        if (auto* ed = getActiveEditor())
        {
           #if JUCE_LINUX
            const MessageManagerLock mmLock;
           #endif

           #if ! JUCE_MAC
            if (auto* peer = ed->getTopLevelComponent()->getPeer())
            {
                auto scale = peer->getPlatformScaleFactor();
                ed->setSize (roundToInt (width / scale), roundToInt (height / scale));

                return;
            }
           #endif

            ed->setSize (width, height);
        }
    }

    //==============================================================================
    static Vst2::AEffect* constructEffect (const ModuleHandle::Ptr& module)
    {
        Vst2::AEffect* effect = nullptr;
        try
        {
            const IdleCallRecursionPreventer icrp;
            _fpreset();

            JUCE_VST_LOG ("Creating VST instance: " + module->pluginName);

           #if JUCE_MAC
            if (module->resFileId != 0)
                UseResFile (module->resFileId);
           #endif

            {
                JUCE_VST_WRAPPER_INVOKE_MAIN
            }

            if (effect != nullptr && effect->magic == 0x56737450 /* 'VstP' */)
            {
                jassert (effect->resvd2 == 0);
                jassert (effect->object != nullptr);

                _fpreset(); // some dodgy plugs mess around with this
            }
            else
            {
                effect = nullptr;
            }
        }
        catch (...)
        {}

        return effect;
    }

    static BusesProperties queryBusIO (Vst2::AEffect* effect)
    {
        BusesProperties returnValue;

        if (effect->numInputs == 0 && effect->numOutputs == 0)
            return returnValue;

        // Workaround for old broken JUCE plug-ins which would return an invalid
        // speaker arrangement if the host didn't ask for a specific arrangement
        // beforehand.
        // Check if the plug-in reports any default layouts. If it doesn't, then
        // try setting a default layout compatible with the number of pins this
        // plug-in is reporting.
        if (! pluginHasDefaultChannelLayouts (effect))
        {
            SpeakerMappings::VstSpeakerConfigurationHolder canonicalIn  (AudioChannelSet::canonicalChannelSet (effect->numInputs));
            SpeakerMappings::VstSpeakerConfigurationHolder canonicalOut (AudioChannelSet::canonicalChannelSet (effect->numOutputs));

            effect->dispatcher (effect, Vst2::effSetSpeakerArrangement, 0,
                                      (pointer_sized_int) &canonicalIn.get(), (void*) &canonicalOut.get(), 0.0f);
        }

        HeapBlock<Vst2::VstSpeakerArrangement> inArrBlock (1, true), outArrBlock (1, true);

        auto* inArr  = inArrBlock.get();
        auto* outArr = outArrBlock.get();

        if (! getSpeakerArrangementWrapper (effect, inArr, outArr))
            inArr = outArr = nullptr;

        for (int dir = 0; dir < 2; ++dir)
        {
            const bool isInput = (dir == 0);
            const int opcode = (isInput ? Vst2::effGetInputProperties : Vst2::effGetOutputProperties);
            const int maxChannels = (isInput ? effect->numInputs : effect->numOutputs);
            const Vst2::VstSpeakerArrangement* arr = (isInput ? inArr : outArr);
            bool busAdded = false;

            Vst2::VstPinProperties pinProps;
            AudioChannelSet layout;

            for (int ch = 0; ch < maxChannels; ch += layout.size())
            {
                if (effect->dispatcher (effect, opcode, ch, 0, &pinProps, 0.0f) == 0)
                    break;

                if ((pinProps.flags & Vst2::kVstPinUseSpeaker) != 0)
                {
                    layout = SpeakerMappings::vstArrangementTypeToChannelSet (pinProps.arrangementType, 0);

                    if (layout.isDisabled())
                        break;
                }
                else if (arr == nullptr)
                {
                    layout = ((pinProps.flags & Vst2::kVstPinIsStereo) != 0 ? AudioChannelSet::stereo() : AudioChannelSet::mono());
                }
                else
                    break;

                busAdded = true;
                returnValue.addBus (isInput, pinProps.label, layout, true);
            }

            // no buses?
            if (! busAdded && maxChannels > 0)
            {
                String busName = (isInput ? "Input" : "Output");

                if (effect->dispatcher (effect, opcode, 0, 0, &pinProps, 0.0f) != 0)
                    busName = pinProps.label;

                if (arr != nullptr)
                    layout = SpeakerMappings::vstArrangementTypeToChannelSet (*arr);
                else
                    layout = AudioChannelSet::canonicalChannelSet (maxChannels);

                returnValue.addBus (isInput, busName, layout, true);
            }
        }

        return returnValue;
    }

    static bool pluginHasDefaultChannelLayouts (Vst2::AEffect* effect)
    {
        HeapBlock<Vst2::VstSpeakerArrangement> inArrBlock (1, true), outArrBlock (1, true);

        auto* inArr  = inArrBlock.get();
        auto* outArr = outArrBlock.get();

        if (getSpeakerArrangementWrapper (effect, inArr, outArr))
            return true;

        for (int dir = 0; dir < 2; ++dir)
        {
            const bool isInput = (dir == 0);
            const int opcode = (isInput ? Vst2::effGetInputProperties : Vst2::effGetOutputProperties);
            const int maxChannels = (isInput ? effect->numInputs : effect->numOutputs);

            int channels = 1;

            for (int ch = 0; ch < maxChannels; ch += channels)
            {
                Vst2::VstPinProperties pinProps;

                if (effect->dispatcher (effect, opcode, ch, 0, &pinProps, 0.0f) == 0)
                    return false;

                if ((pinProps.flags & Vst2::kVstPinUseSpeaker) != 0)
                    return true;

                channels = (pinProps.flags & Vst2::kVstPinIsStereo) != 0 ? 2 : 1;
            }
        }

        return false;
    }

    static bool getSpeakerArrangementWrapper (Vst2::AEffect* effect,
                                              Vst2::VstSpeakerArrangement* inArr,
                                              Vst2::VstSpeakerArrangement* outArr)
    {
        // Workaround: unfortunately old JUCE VST-2 plug-ins had a bug and would crash if
        // you try to get the speaker arrangement when there are no input channels present.
        // Hopefully, one day (when there are no more old JUCE plug-ins around), we can
        // comment out the next two lines.
        if (effect->numInputs == 0)
            return false;

        return (effect->dispatcher (effect, Vst2::effGetSpeakerArrangement, 0,
                                          reinterpret_cast<pointer_sized_int> (&inArr), &outArr, 0.0f) != 0);
    }

    //==============================================================================
    template <typename FloatType>
    void processAudio (AudioBuffer<FloatType>& buffer, MidiBuffer& midiMessages,
                       AudioBuffer<FloatType>& tmpBuffer,
                       HeapBlock<FloatType*>& channelBuffer,
                       bool processBlockBypassedCalled)
    {
        if (vstSupportsBypass)
        {
            updateBypass (processBlockBypassedCalled);
        }
        else if (processBlockBypassedCalled)
        {
            // if this vst does not support bypass then we will have to do this ourselves
            AudioProcessor::processBlockBypassed (buffer, midiMessages);
            return;
        }

        auto numSamples  = buffer.getNumSamples();
        auto numChannels = buffer.getNumChannels();

        if (initialised)
        {
            if (auto* currentPlayHead = getPlayHead())
            {
                AudioPlayHead::CurrentPositionInfo position;

                if (currentPlayHead->getCurrentPosition (position))
                {

                    vstHostTime.samplePos          = (double) position.timeInSamples;
                    vstHostTime.tempo              = position.bpm;
                    vstHostTime.timeSigNumerator   = position.timeSigNumerator;
                    vstHostTime.timeSigDenominator = position.timeSigDenominator;
                    vstHostTime.ppqPos             = position.ppqPosition;
                    vstHostTime.barStartPos        = position.ppqPositionOfLastBarStart;
                    vstHostTime.flags |= Vst2::kVstTempoValid
                                           | Vst2::kVstTimeSigValid
                                           | Vst2::kVstPpqPosValid
                                           | Vst2::kVstBarsValid;

                    int32 newTransportFlags = 0;
                    if (position.isPlaying)     newTransportFlags |= Vst2::kVstTransportPlaying;
                    if (position.isRecording)   newTransportFlags |= Vst2::kVstTransportRecording;

                    if (newTransportFlags != (vstHostTime.flags & (Vst2::kVstTransportPlaying
                                                                   | Vst2::kVstTransportRecording)))
                        vstHostTime.flags = (vstHostTime.flags & ~(Vst2::kVstTransportPlaying | Vst2::kVstTransportRecording)) | newTransportFlags | Vst2::kVstTransportChanged;
                    else
                        vstHostTime.flags &= ~Vst2::kVstTransportChanged;

                    switch (position.frameRate)
                    {
                        case AudioPlayHead::fps24:       setHostTimeFrameRate (Vst2::kVstSmpte24fps, 24.0, position.timeInSeconds); break;
                        case AudioPlayHead::fps25:       setHostTimeFrameRate (Vst2::kVstSmpte25fps, 25.0, position.timeInSeconds); break;
                        case AudioPlayHead::fps30:       setHostTimeFrameRate (Vst2::kVstSmpte30fps, 30.0, position.timeInSeconds); break;
                        case AudioPlayHead::fps60:       setHostTimeFrameRate (Vst2::kVstSmpte60fps, 60.0, position.timeInSeconds); break;

                        case AudioPlayHead::fps23976:    setHostTimeFrameRateDrop (Vst2::kVstSmpte239fps,      24.0, position.timeInSeconds); break;
                        case AudioPlayHead::fps2997:     setHostTimeFrameRateDrop (Vst2::kVstSmpte2997fps,     30.0, position.timeInSeconds); break;
                        case AudioPlayHead::fps2997drop: setHostTimeFrameRateDrop (Vst2::kVstSmpte2997dfps, 30.0, position.timeInSeconds); break;
                        case AudioPlayHead::fps30drop:   setHostTimeFrameRateDrop (Vst2::kVstSmpte30dfps,   30.0, position.timeInSeconds); break;
                        case AudioPlayHead::fps60drop:   setHostTimeFrameRateDrop (Vst2::kVstSmpte599fps,      60.0, position.timeInSeconds); break;
                        default: break;
                    }

                    if (position.isLooping)
                    {
                        vstHostTime.cycleStartPos = position.ppqLoopStart;
                        vstHostTime.cycleEndPos   = position.ppqLoopEnd;
                        vstHostTime.flags |= (Vst2::kVstCyclePosValid | Vst2::kVstTransportCycleActive);
                    }
                    else
                    {
                        vstHostTime.flags &= ~(Vst2::kVstCyclePosValid | Vst2::kVstTransportCycleActive);
                    }
                }
            }

            vstHostTime.nanoSeconds = getVSTHostTimeNanoseconds();

            if (wantsMidiMessages)
            {
                midiEventsToSend.clear();
                midiEventsToSend.ensureSize (1);

                MidiBuffer::Iterator iter (midiMessages);
                const uint8* midiData;
                int numBytesOfMidiData, samplePosition;

                while (iter.getNextEvent (midiData, numBytesOfMidiData, samplePosition))
                    midiEventsToSend.addEvent (midiData, numBytesOfMidiData,
                                               jlimit (0, numSamples - 1, samplePosition));

                vstEffect->dispatcher (vstEffect, Vst2::effProcessEvents, 0, 0, midiEventsToSend.events, 0);
            }

            _clearfp();

            // always ensure that the buffer is at least as large as the maximum number of channels
            auto maxChannels = jmax (vstEffect->numInputs, vstEffect->numOutputs);
            auto channels = channelBuffer.get();

            if (numChannels < maxChannels)
            {
                if (numSamples > tmpBuffer.getNumSamples())
                    tmpBuffer.setSize (tmpBuffer.getNumChannels(), numSamples);

                tmpBuffer.clear();
            }

            for (int ch = 0; ch < maxChannels; ++ch)
                channels[ch] = (ch < numChannels ? buffer.getWritePointer (ch) : tmpBuffer.getWritePointer (ch));

            {
                AudioBuffer<FloatType> processBuffer (channels, maxChannels, numSamples);

                invokeProcessFunction (processBuffer, numSamples);
            }
        }
        else
        {
            // Not initialised, so just bypass..
            for (int i = getTotalNumOutputChannels(); --i >= 0;)
                buffer.clear (i, 0, buffer.getNumSamples());
        }

        {
            // copy any incoming midi..
            const ScopedLock sl (midiInLock);

            midiMessages.swapWith (incomingMidi);
            incomingMidi.clear();
        }
    }

    //==============================================================================
    inline void invokeProcessFunction (AudioBuffer<float>& buffer, int32 sampleFrames)
    {
        if ((vstEffect->flags & Vst2::effFlagsCanReplacing) != 0)
        {
            vstEffect->processReplacing (vstEffect, buffer.getArrayOfWritePointers(),
                                                    buffer.getArrayOfWritePointers(), sampleFrames);
        }
        else
        {
            outOfPlaceBuffer.setSize (vstEffect->numOutputs, sampleFrames);
            outOfPlaceBuffer.clear();

            vstEffect->process (vstEffect, buffer.getArrayOfWritePointers(),
                                           outOfPlaceBuffer.getArrayOfWritePointers(), sampleFrames);

            for (int i = vstEffect->numOutputs; --i >= 0;)
                buffer.copyFrom (i, 0, outOfPlaceBuffer.getReadPointer (i), sampleFrames);
        }
    }

    inline void invokeProcessFunction (AudioBuffer<double>& buffer, int32 sampleFrames)
    {
        vstEffect->processDoubleReplacing (vstEffect, buffer.getArrayOfWritePointers(),
                                                      buffer.getArrayOfWritePointers(), sampleFrames);
    }

    //==============================================================================
    void setHostTimeFrameRate (long frameRateIndex, double frameRate, double currentTime) noexcept
    {
        vstHostTime.flags |= Vst2::kVstSmpteValid;
        vstHostTime.smpteFrameRate   = (int32) frameRateIndex;
        vstHostTime.smpteOffset = (int32) (currentTime * 80.0 * frameRate + 0.5);
    }

    void setHostTimeFrameRateDrop (long frameRateIndex, double frameRate, double currentTime) noexcept
    {
        setHostTimeFrameRate (frameRateIndex, frameRate * 1000.0 / 1001.0, currentTime);
    }

    bool restoreProgramSettings (const fxProgram* const prog)
    {
        if (compareMagic (prog->chunkMagic, "CcnK")
             && compareMagic (prog->fxMagic, "FxCk"))
        {
            changeProgramName (getCurrentProgram(), prog->prgName);

            for (int i = 0; i < fxbSwap (prog->numParams); ++i)
                if (auto* param = getParameters()[i])
                    param->setValue (fxbSwapFloat (prog->params[i]));

            return true;
        }

        return false;
    }

    String getTextForOpcode (const int index, const int opcode) const
    {
        if (vstEffect == nullptr)
            return {};

        jassert (index >= 0 && index < vstEffect->numParams);
        char nm[256] = { 0 };
        dispatch (opcode, index, 0, nm, 0);
        return String::createStringFromData (nm, (int) sizeof (nm)).trim();
    }

    String getCurrentProgramName()
    {
        String progName;

        if (vstEffect != nullptr)
        {
            {
                char nm[256] = { 0 };
                dispatch (Vst2::effGetProgramName, 0, 0, nm, 0);
                progName = String::createStringFromData (nm, (int) sizeof (nm)).trim();
            }

            const int index = getCurrentProgram();

            if (index >= 0 && programNames[index].isEmpty())
            {
                while (programNames.size() < index)
                    programNames.add (String());

                programNames.set (index, progName);
            }
        }

        return progName;
    }

    void setParamsInProgramBlock (fxProgram* prog)
    {
        auto numParams = getParameters().size();

        prog->chunkMagic = fxbName ("CcnK");
        prog->byteSize = 0;
        prog->fxMagic = fxbName ("FxCk");
        prog->version = fxbSwap (fxbVersionNum);
        prog->fxID = fxbSwap (getUID());
        prog->fxVersion = fxbSwap (getVersionNumber());
        prog->numParams = fxbSwap (numParams);

        getCurrentProgramName().copyToUTF8 (prog->prgName, sizeof (prog->prgName) - 1);

        for (int i = 0; i < numParams; ++i)
            if (auto* param = getParameters()[i])
                prog->params[i] = fxbSwapFloat (param->getValue());
    }

    void updateStoredProgramNames()
    {
        if (vstEffect != nullptr && getNumPrograms() > 0)
        {
            char nm[256] = { 0 };

            // only do this if the plugin can't use indexed names..
            if (dispatch (Vst2::effGetProgramNameIndexed, 0, -1, nm, 0) == 0)
            {
                auto oldProgram = getCurrentProgram();
                MemoryBlock oldSettings;
                createTempParameterStore (oldSettings);

                for (int i = 0; i < getNumPrograms(); ++i)
                {
                    setCurrentProgram (i);
                    getCurrentProgramName();  // (this updates the list)
                }

                setCurrentProgram (oldProgram);
                restoreFromTempParameterStore (oldSettings);
            }
        }
    }

    void handleMidiFromPlugin (const Vst2::VstEvents* events)
    {
        if (events != nullptr)
        {
            const ScopedLock sl (midiInLock);
            VSTMidiEventList::addEventsToMidiBuffer (events, incomingMidi);
        }
    }

    //==============================================================================
    void createTempParameterStore (MemoryBlock& dest)
    {
        auto numParameters = getParameters().size();
        dest.setSize (64 + 4 * (size_t) numParameters);
        dest.fillWith (0);

        getCurrentProgramName().copyToUTF8 ((char*) dest.getData(), 63);

        auto p = (float*) (((char*) dest.getData()) + 64);

        for (int i = 0; i < numParameters; ++i)
            if (auto* param = getParameters()[i])
                p[i] = param->getValue();
    }

    void restoreFromTempParameterStore (const MemoryBlock& m)
    {
        changeProgramName (getCurrentProgram(), (const char*) m.getData());

        auto p = (float*) (((char*) m.getData()) + 64);
        auto numParameters = getParameters().size();

        for (int i = 0; i < numParameters; ++i)
            if (auto* param = getParameters()[i])
                param->setValue (p[i]);
    }

    pointer_sized_int getVstDirectory() const
    {
       #if JUCE_MAC
        return (pointer_sized_int) (void*) &vstModule->parentDirFSSpec;
       #else
        return (pointer_sized_int) (pointer_sized_uint) vstModule->fullParentDirectoryPathName.toRawUTF8();
       #endif
    }

    //==============================================================================
    int getVersionNumber() const noexcept   { return vstEffect != nullptr ? vstEffect->version : 0; }

    String getVersion() const
    {
        auto v = (unsigned int) dispatch (Vst2::effGetVendorVersion, 0, 0, nullptr, 0);

        String s;

        if (v == 0 || (int) v == -1)
            v = (unsigned int) getVersionNumber();

        if (v != 0)
        {
            // See yfede's post for the rational on this encoding
            // https://forum.juce.com/t/issues-with-version-integer-reported-by-vst2/23867/6

            unsigned int major = 0, minor = 0, bugfix = 0, build = 0;

            if (v < 10)            // Encoding A
            {
                major = v;
            }
            else if (v < 10000)    // Encoding B
            {
                major  = (v / 1000);
                minor  = (v % 1000) / 100;
                bugfix = (v % 100)  / 10;
                build  = (v % 10);
            }
            else if (v < 0x10000)  // Encoding C
            {
                major  = (v / 10000);
                minor  = (v % 10000) / 1000;
                bugfix = (v % 1000)  / 100;
                build  = (v % 100)   / 10;
            }
            else if (v < 0x650000) // Encoding D
            {
                major  = (v >> 16) & 0xff;
                minor  = (v >> 8)  & 0xff;
                bugfix = (v >> 0)  & 0xff;
            }
            else                  // Encoding E
            {
                major  = (v / 10000000);
                minor  = (v % 10000000) / 100000;
                bugfix = (v % 100000)   / 1000;
                build  = (v % 1000);
            }

            s << (int) major << '.' << (int) minor << '.' << (int) bugfix << '.' << (int) build;
        }

        return s;
    }

    const char* getCategory() const
    {
        switch (getVstCategory())
        {
            case Vst2::kPlugCategEffect:       return "Effect";
            case Vst2::kPlugCategSynth:        return "Synth";
            case Vst2::kPlugCategAnalysis:     return "Analysis";
            case Vst2::kPlugCategMastering:    return "Mastering";
            case Vst2::kPlugCategSpacializer:  return "Spacial";
            case Vst2::kPlugCategRoomFx:       return "Reverb";
            case Vst2::kPlugSurroundFx:        return "Surround";
            case Vst2::kPlugCategRestoration:  return "Restoration";
            case Vst2::kPlugCategGenerator:    return "Tone generation";
            default:                     break;
        }

        return nullptr;
    }

    void setPower (const bool on)
    {
        dispatch (Vst2::effMainsChanged, 0, on ? 1 : 0, nullptr, 0);
        isPowerOn = on;
    }

    //==============================================================================
    void updateBypass (bool processBlockBypassedCalled)
    {
        if (processBlockBypassedCalled)
        {
            if (bypassParam->getValue() == 0.0f || ! lastProcessBlockCallWasBypass)
                bypassParam->setValue (1.0f);
        }
        else
        {
            if (lastProcessBlockCallWasBypass)
                bypassParam->setValue (0.0f);
        }

        lastProcessBlockCallWasBypass = processBlockBypassedCalled;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VSTPluginInstance)
};

//==============================================================================
#if ! (JUCE_IOS || JUCE_ANDROID)
struct VSTPluginWindow;
static Array<VSTPluginWindow*> activeVSTWindows;

//==============================================================================
struct VSTPluginWindow   : public AudioProcessorEditor,
                          #if ! JUCE_MAC
                           private ComponentMovementWatcher,
                           private ComponentPeer::ScaleFactorListener,
                          #elif JUCE_MAC
                           private AsyncUpdater,
                          #endif
                           private Timer
{
public:
    VSTPluginWindow (VSTPluginInstance& plug)
        : AudioProcessorEditor (&plug),
         #if ! JUCE_MAC
          ComponentMovementWatcher (this),
         #endif
          plugin (plug)
    {
       #if JUCE_LINUX
        pluginWindow = None;
        display = XWindowSystem::getInstance()->displayRef();
        ignoreUnused (pluginRefusesToResize, alreadyInside);

       #elif JUCE_MAC
        ignoreUnused (recursiveResize, pluginRefusesToResize, alreadyInside);

        #if JUCE_SUPPORT_CARBON
        if (! plug.usesCocoaNSView)
        {
            carbonWrapper.reset (new CarbonWrapperComponent (*this));
            addAndMakeVisible (carbonWrapper.get());
        }
        else
        #endif
        {
            cocoaWrapper.reset (new AutoResizingNSViewComponentWithParent());
            addAndMakeVisible (cocoaWrapper.get());
            triggerAsyncUpdate();
        }
       #endif

        activeVSTWindows.add (this);

        Vst2::ERect* rect = nullptr;
        dispatch (Vst2::effEditGetRect, 0, 0, &rect, 0);

        if (rect != nullptr)
            setSize (rect->right - rect->left, rect->bottom - rect->top);
        else
            setSize (1, 1);

        setOpaque (true);
        setVisible (true);
    }

    ~VSTPluginWindow() override
    {
        closePluginWindow();

       #if JUCE_MAC
        #if JUCE_SUPPORT_CARBON
        carbonWrapper.reset();
        #endif
        cocoaWrapper.reset();
       #elif JUCE_LINUX
        display = XWindowSystem::getInstance()->displayUnref();
       #endif

        activeVSTWindows.removeFirstMatchingValue (this);
        plugin.editorBeingDeleted (this);

       #if ! JUCE_MAC
        for (int i = 0; i < ComponentPeer::getNumPeers(); ++i)
            if (auto* peer = ComponentPeer::getPeer (i))
                peer->removeScaleFactorListener (this);
       #endif
    }

    //==============================================================================
   #if JUCE_MAC
    void paint (Graphics& g) override
    {
        g.fillAll (Colours::black);
    }
    
    void handleAsyncUpdate() override
    {
        // At that point we should have a valid parent view pointer!
        jassert(cocoaWrapper->getParentView() != nullptr);
        
        visibilityChanged();
    }

    void visibilityChanged() override
    {
        if (cocoaWrapper->getParentView() != nullptr)
        {
            if (isVisible())
                openPluginWindow ((NSView*)cocoaWrapper->getParentView());
            else
                closePluginWindow();
        }
    }

    void childBoundsChanged (Component*) override
    {
        if (cocoaWrapper != nullptr)
        {
            auto w = cocoaWrapper->getWidth();
            auto h = cocoaWrapper->getHeight();

            if (w != getWidth() || h != getHeight())
                setSize (w, h);
        }
    }
   #else
    void paint (Graphics& g) override
    {
        if (isOpen)
        {
           #if JUCE_LINUX
            if (pluginWindow != 0)
            {
                auto clip = g.getClipBounds();

                XClearArea (display, pluginWindow, clip.getX(), clip.getY(),
                            static_cast<unsigned int> (clip.getWidth()), static_cast<unsigned int> (clip.getHeight()), True);
            }
           #endif
        }
        else
        {
            g.fillAll (Colours::black);
        }
    }

    void componentMovedOrResized (bool /*wasMoved*/, bool /*wasResized*/) override
    {
        if (recursiveResize)
            return;

        auto* topComp = getTopLevelComponent();

        if (topComp->getPeer() != nullptr)
        {
            auto pos = (topComp->getLocalPoint (this, Point<int>()) * nativeScaleFactor).roundToInt();

            recursiveResize = true;

           #if JUCE_WINDOWS
            if (pluginHWND != 0)
                MoveWindow (pluginHWND, pos.getX(), pos.getY(), roundToInt (getWidth() * nativeScaleFactor), roundToInt (getHeight() * nativeScaleFactor), TRUE);
           #elif JUCE_LINUX
            if (pluginWindow != 0)
            {
                XMoveResizeWindow (display, pluginWindow,
                                   pos.getX(), pos.getY(),
                                   static_cast<unsigned int> (roundToInt (getWidth()  * nativeScaleFactor)),
                                   static_cast<unsigned int> (roundToInt (getHeight() * nativeScaleFactor)));

                XMapRaised (display, pluginWindow);
                XFlush (display);
            }
           #endif

            recursiveResize = false;
        }
    }

    using ComponentMovementWatcher::componentMovedOrResized;

    void componentVisibilityChanged() override
    {
        if (isShowing())
            openPluginWindow();
        else if (! shouldAvoidDeletingWindow())
            closePluginWindow();

        if (auto* peer = getTopLevelComponent()->getPeer())
            setScaleFactorAndDispatchMessage (peer->getPlatformScaleFactor());

       #if JUCE_LINUX
        MessageManager::callAsync ([this] { componentMovedOrResized (true, true); });
       #else
        componentMovedOrResized (true, true);
       #endif
    }

    using ComponentMovementWatcher::componentVisibilityChanged;

    void componentPeerChanged() override
    {
        closePluginWindow();
        openPluginWindow();

        if (auto* peer = getTopLevelComponent()->getPeer())
            peer->addScaleFactorListener (this);

        componentMovedOrResized (true, true);
    }

    void nativeScaleFactorChanged (double newScaleFactor) override
    {
        setScaleFactorAndDispatchMessage (newScaleFactor);

       #if JUCE_WINDOWS
        Vst2::ERect* rect = nullptr;
        dispatch (Vst2::effEditGetRect, 0, 0, &rect, 0);

        if (! isWindowSizeCorrectForPlugin (roundToInt ((rect->right - rect->left) / nativeScaleFactor),
                                            roundToInt ((rect->bottom - rect->top) / nativeScaleFactor)))
            return;
       #endif

        componentMovedOrResized (true, true);
    }

    void setScaleFactorAndDispatchMessage (double newScaleFactor)
    {
        if (approximatelyEqual ((float) newScaleFactor, nativeScaleFactor))
            return;

        nativeScaleFactor = (float) newScaleFactor;

        if (pluginRespondsToDPIChanges)
            dispatch (Vst2::effVendorSpecific,
                      JUCE_MULTICHAR_CONSTANT ('P', 'r', 'e', 'S'),
                      JUCE_MULTICHAR_CONSTANT ('A', 'e', 'C', 's'),
                      nullptr, nativeScaleFactor);
    }
   #endif

    //==============================================================================
    bool keyStateChanged (bool) override                 { return pluginWantsKeys; }
    bool keyPressed (const juce::KeyPress&) override     { return pluginWantsKeys; }

    //==============================================================================
    void timerCallback() override
    {
        if (isShowing())
        {
           #if JUCE_WINDOWS
            if (--sizeCheckCount <= 0)
            {
                sizeCheckCount = 10;
                checkPluginWindowSize();
            }
           #endif

            static bool reentrantGuard = false;

            if (! reentrantGuard)
            {
                reentrantGuard = true;
                plugin.dispatch (Vst2::effEditIdle, 0, 0, nullptr, 0);
                reentrantGuard = false;
            }

           #if JUCE_LINUX
            if (pluginWindow == 0)
            {
                updatePluginWindowHandle();

                if (pluginWindow != 0)
                    componentMovedOrResized (true, true);
            }
           #endif
        }
    }

    //==============================================================================
    void mouseDown (const MouseEvent& e) override
    {
        ignoreUnused (e);

       #if JUCE_WINDOWS || JUCE_LINUX
        toFront (true);
       #endif
    }

    void broughtToFront() override
    {
        activeVSTWindows.removeFirstMatchingValue (this);
        activeVSTWindows.add (this);

       #if JUCE_MAC
        dispatch (Vst2::effEditTop, 0, 0, nullptr, 0);
       #endif
    }

private:
    VSTPluginInstance& plugin;
    bool isOpen = false, recursiveResize = false;
    bool pluginWantsKeys = false, pluginRefusesToResize = false, alreadyInside = false;

   #if ! JUCE_MAC
    bool pluginRespondsToDPIChanges = false;
   #endif

   #if ! JUCE_MAC
    float nativeScaleFactor = 1.0f;
    #if JUCE_WINDOWS
     HWND pluginHWND = {};
     void* originalWndProc = {};
     int sizeCheckCount = 0;
    #elif JUCE_LINUX
     ::Display* display;
     Window pluginWindow;
    #endif
   #endif

    // This is a workaround for old Mackie plugins that crash if their
    // window is deleted more than once.
    bool shouldAvoidDeletingWindow() const
    {
        return plugin.getPluginDescription()
                .manufacturerName.containsIgnoreCase ("Loud Technologies");
    }

    // This is an old workaround for some plugins that need a repaint when their
    // windows are first created, but it breaks some Izotope plugins..
    bool shouldRepaintCarbonWindowWhenCreated()
    {
        return ! plugin.getName().containsIgnoreCase ("izotope");
    }

    //==============================================================================
   #if JUCE_MAC
    void openPluginWindow (void* parentWindow)
    {
        if (isOpen || parentWindow == nullptr)
            return;

        isOpen = true;

        Vst2::ERect* rect = nullptr;
        dispatch (Vst2::effEditGetRect, 0, 0, &rect, 0);
        dispatch (Vst2::effEditOpen, 0, 0, parentWindow, 0);

        // do this before and after like in the steinberg example
        dispatch (Vst2::effEditGetRect, 0, 0, &rect, 0);
        dispatch (Vst2::effGetProgram, 0, 0, nullptr, 0); // also in steinberg code

        // Install keyboard hooks
        pluginWantsKeys = (dispatch (Vst2::effKeysRequired, 0, 0, nullptr, 0) == 0);

        // double-check it's not too tiny
        int w = 250, h = 150;

        if (rect != nullptr)
        {
            w = rect->right - rect->left;
            h = rect->bottom - rect->top;

            if (w == 0 || h == 0)
            {
                w = 250;
                h = 150;
            }
        }

        w = jmax (w, 32);
        h = jmax (h, 32);

        setSize (w, h);

        startTimer (18 + juce::Random::getSystemRandom().nextInt (5));
        repaint();
    }
   #else
    void openPluginWindow()
    {
        if (isOpen || getWindowHandle() == nullptr)
            return;

        JUCE_VST_LOG ("Opening VST UI: " + plugin.getName());
        isOpen = true;

        pluginRespondsToDPIChanges = plugin.pluginCanDo ("supportsViewDpiScaling") > 0;

        if (auto* peer = getTopLevelComponent()->getPeer())
            setScaleFactorAndDispatchMessage (peer->getPlatformScaleFactor());

       #if JUCE_WINDOWS && JUCE_WIN_PER_MONITOR_DPI_AWARE
        std::unique_ptr<ScopedDPIAwarenessDisabler> dpiDisabler;

        if (! pluginRespondsToDPIChanges)
            dpiDisabler.reset (new ScopedDPIAwarenessDisabler());
       #endif

        Vst2::ERect* rect = nullptr;
        dispatch (Vst2::effEditGetRect, 0, 0, &rect, 0);

       #if JUCE_WINDOWS && JUCE_WIN_PER_MONITOR_DPI_AWARE
        // some plug-ins are fussy about this
        dpiDisabler.reset (nullptr);
       #endif

        dispatch (Vst2::effEditOpen, 0, 0, getWindowHandle(), 0);

       #if JUCE_WINDOWS && JUCE_WIN_PER_MONITOR_DPI_AWARE
        if (! pluginRespondsToDPIChanges)
            dpiDisabler.reset (new ScopedDPIAwarenessDisabler());
       #endif

        // do this before and after like in the steinberg example
        dispatch (Vst2::effEditGetRect, 0, 0, &rect, 0);
        dispatch (Vst2::effGetProgram, 0, 0, nullptr, 0); // also in steinberg code

        // Install keyboard hooks
        pluginWantsKeys = (dispatch (Vst2::effKeysRequired, 0, 0, nullptr, 0) == 0);

       #if JUCE_WINDOWS
        originalWndProc = 0;
        pluginHWND = GetWindow ((HWND) getWindowHandle(), GW_CHILD);

        if (pluginHWND == 0)
        {
            isOpen = false;
            setSize (300, 150);
            return;
        }

        #pragma warning (push)
        #pragma warning (disable: 4244)

        if (! pluginWantsKeys)
        {
            originalWndProc = (void*) GetWindowLongPtr (pluginHWND, GWLP_WNDPROC);
            SetWindowLongPtr (pluginHWND, GWLP_WNDPROC, (LONG_PTR) vstHookWndProc);
        }

        #pragma warning (pop)

       #if JUCE_WIN_PER_MONITOR_DPI_AWARE
        dpiDisabler.reset (nullptr);
       #endif

        RECT r;
        GetWindowRect (pluginHWND, &r);
        auto w = (int) (r.right - r.left);
        auto h = (int) (r.bottom - r.top);

        if (rect != nullptr)
        {
            auto rw = rect->right - rect->left;
            auto rh = rect->bottom - rect->top;

            if ((rw > 50 && rh > 50 && rw < 2000 && rh < 2000 && (! isWithin (w, rw, 2) || ! isWithin (h, rh, 2)))
                || ((w == 0 && rw > 0) || (h == 0 && rh > 0)))
            {
                // very dodgy logic to decide which size is right.
                if (std::abs (rw - w) > 350 || std::abs (rh - h) > 350)
                {
                    SetWindowPos (pluginHWND, 0,
                                  0, 0, roundToInt (rw * nativeScaleFactor), roundToInt (rh * nativeScaleFactor),
                                  SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);

                    GetWindowRect (pluginHWND, &r);

                    w = r.right - r.left;
                    h = r.bottom - r.top;

                    pluginRefusesToResize = (w != rw) || (h != rh);

                    w = rw;
                    h = rh;
                }
            }
        }
       #elif JUCE_LINUX
        updatePluginWindowHandle();

        int w = 250, h = 150;

        if (rect != nullptr)
        {
            w = rect->right - rect->left;
            h = rect->bottom - rect->top;

            if (w == 0 || h == 0)
            {
                w = 250;
                h = 150;
            }
        }

        if (pluginWindow != 0)
            XMapRaised (display, pluginWindow);
       #endif

        w = roundToInt (w / nativeScaleFactor);
        h = roundToInt (h / nativeScaleFactor);

        // double-check it's not too tiny
        w = jmax (w, 32);
        h = jmax (h, 32);

        setSize (w, h);

       #if JUCE_WINDOWS
        checkPluginWindowSize();
       #endif

        startTimer (18 + juce::Random::getSystemRandom().nextInt (5));
        repaint();
    }
   #endif

    //==============================================================================
    void closePluginWindow()
    {
        if (isOpen)
        {
            // You shouldn't end up hitting this assertion unless the host is trying to do GUI
            // cleanup on a non-GUI thread.. If it does that, bad things could happen in here..
            JUCE_ASSERT_MESSAGE_MANAGER_IS_LOCKED

            JUCE_VST_LOG ("Closing VST UI: " + plugin.getName());
            isOpen = false;
            dispatch (Vst2::effEditClose, 0, 0, nullptr, 0);
            stopTimer();

           #if JUCE_WINDOWS
            #pragma warning (push)
            #pragma warning (disable: 4244)
            if (originalWndProc != 0 && pluginHWND != 0 && IsWindow (pluginHWND))
                SetWindowLongPtr (pluginHWND, GWLP_WNDPROC, (LONG_PTR) originalWndProc);
            #pragma warning (pop)

            originalWndProc = 0;
            pluginHWND = 0;
           #elif JUCE_LINUX
            pluginWindow = 0;
           #endif
        }
    }

    //==============================================================================
    pointer_sized_int dispatch (const int opcode, const int index, const int value, void* const ptr, float opt)
    {
        return plugin.dispatch (opcode, index, value, ptr, opt);
    }

    //==============================================================================
   #if JUCE_WINDOWS
    bool willCauseRecursiveResize (int w, int h)
    {
        auto newScreenBounds = Rectangle<int> (w, h).withPosition (getScreenPosition());

        return Desktop::getInstance().getDisplays().findDisplayForRect (newScreenBounds).scale != nativeScaleFactor;
    }

    bool isWindowSizeCorrectForPlugin (int w, int h)
    {
        if (! isShowing() || pluginRefusesToResize)
            return true;

        return (isWithin (w, getWidth(), 5) && isWithin (h, getHeight(), 5));
    }

    void checkPluginWindowSize()
    {
       #if JUCE_WIN_PER_MONITOR_DPI_AWARE
        if (! pluginRespondsToDPIChanges)
        {
            Vst2::ERect* rect = nullptr;
            dispatch (Vst2::effEditGetRect, 0, 0, &rect, 0);

            auto w = roundToInt ((rect->right - rect->left) / nativeScaleFactor);
            auto h = roundToInt ((rect->bottom - rect->top) / nativeScaleFactor);

            if (! isWindowSizeCorrectForPlugin (w, h))
            {
                // If plug-in isn't DPI aware then we need to resize our window, but this may cause a recursive resize
                // so add a check
                if (! willCauseRecursiveResize (w, h))
                    setSize (w, h);

                sizeCheckCount = 0;
            }
        }
       #else
        {
            RECT r;
            GetWindowRect (pluginHWND, &r);

            auto w = r.right - r.left;
            auto h = r.bottom - r.top;

            if (isShowing() && w > 0 && h > 0
                && (w != getWidth() || h != getHeight())
                && !pluginRefusesToResize)
            {
                setSize (w, h);
                sizeCheckCount = 0;
            }
        }
       #endif
    }

    // hooks to get keyboard events from VST windows..
    static LRESULT CALLBACK vstHookWndProc (HWND hW, UINT message, WPARAM wParam, LPARAM lParam)
    {
        for (int i = activeVSTWindows.size(); --i >= 0;)
        {
            Component::SafePointer<VSTPluginWindow> w (activeVSTWindows[i]);

            if (w != nullptr && w->pluginHWND == hW)
            {
                if (message == WM_CHAR
                    || message == WM_KEYDOWN
                    || message == WM_SYSKEYDOWN
                    || message == WM_KEYUP
                    || message == WM_SYSKEYUP
                    || message == WM_APPCOMMAND)
                {
                    SendMessage ((HWND) w->getTopLevelComponent()->getWindowHandle(),
                                 message, wParam, lParam);
                }

                if (w != nullptr) // (may have been deleted in SendMessage callback)
                    return CallWindowProc ((WNDPROC) w->originalWndProc,
                                           (HWND) w->pluginHWND,
                                           message, wParam, lParam);
            }
        }

        return DefWindowProc (hW, message, wParam, lParam);
    }
   #endif

   #if JUCE_LINUX
    void updatePluginWindowHandle()
    {
        pluginWindow = getChildWindow ((Window) getWindowHandle());
    }
   #endif

    //==============================================================================
  #if JUCE_MAC
   #if JUCE_SUPPORT_CARBON
    struct CarbonWrapperComponent   : public CarbonViewWrapperComponent
    {
        CarbonWrapperComponent (VSTPluginWindow& w)  : owner (w)
        {
            keepPluginWindowWhenHidden = w.shouldAvoidDeletingWindow();
            setRepaintsChildHIViewWhenCreated (w.shouldRepaintCarbonWindowWhenCreated());
        }

        ~CarbonWrapperComponent()
        {
            deleteWindow();
        }

        HIViewRef attachView (WindowRef windowRef, HIViewRef /*rootView*/) override
        {
            owner.openPluginWindow (windowRef);
            return {};
        }

        void removeView (HIViewRef) override
        {
            if (owner.isOpen)
            {
                owner.isOpen = false;
                owner.dispatch (Vst2::effEditClose, 0, 0, 0, 0);
                owner.dispatch (Vst2::effEditSleep, 0, 0, 0, 0);
            }
        }

        bool getEmbeddedViewSize (int& w, int& h) override
        {
            Vst2::ERect* rect = nullptr;
            owner.dispatch (Vst2::effEditGetRect, 0, 0, &rect, 0);
            w = rect->right - rect->left;
            h = rect->bottom - rect->top;
            return true;
        }

        void handleMouseDown (int x, int y) override
        {
            if (! alreadyInside)
            {
                alreadyInside = true;
                getTopLevelComponent()->toFront (true);
                owner.dispatch (Vst2::effEditMouse, x, y, 0, 0);
                alreadyInside = false;
            }
            else
            {
                PostEvent (::mouseDown, 0);
            }
        }

        void handlePaint() override
        {
            if (auto* peer = getPeer())
            {
                auto pos = peer->globalToLocal (getScreenPosition());
                Vst2::ERect r;
                r.left   = (int16) pos.getX();
                r.top    = (int16) pos.getY();
                r.right  = (int16) (r.left + getWidth());
                r.bottom = (int16) (r.top + getHeight());

                owner.dispatch (Vst2::effEditDraw, 0, 0, &r, 0);
            }
        }

    private:
        VSTPluginWindow& owner;
        bool alreadyInside = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CarbonWrapperComponent)
    };

    friend struct CarbonWrapperComponent;
    std::unique_ptr<CarbonWrapperComponent> carbonWrapper;
   #endif

    std::unique_ptr<AutoResizingNSViewComponentWithParent> cocoaWrapper;

    void resized() override
    {
       #if JUCE_SUPPORT_CARBON
        if (carbonWrapper != nullptr)
            carbonWrapper->setSize (getWidth(), getHeight());
       #endif

        if (cocoaWrapper != nullptr)
            cocoaWrapper->setSize (getWidth(), getHeight());
    }
  #endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VSTPluginWindow)
};
#endif
#if JUCE_MSVC
 #pragma warning (pop)
#endif

//==============================================================================
AudioProcessorEditor* VSTPluginInstance::createEditor()
{
   #if JUCE_IOS || JUCE_ANDROID
    return nullptr;
   #else
    return hasEditor() ? new VSTPluginWindow (*this)
                       : nullptr;
   #endif
}

//==============================================================================
// entry point for all callbacks from the plugin
static pointer_sized_int VSTCALLBACK audioMaster (Vst2::AEffect* effect, int32 opcode, int32 index, pointer_sized_int value, void* ptr, float opt)
{
    if (effect != nullptr)
        if (auto* instance = (VSTPluginInstance*) (effect->resvd2))
            return instance->handleCallback (opcode, index, value, ptr, opt);

    return VSTPluginInstance::handleGeneralCallback (opcode, index, value, ptr, opt);
}

//==============================================================================
VSTPluginFormat::VSTPluginFormat() {}
VSTPluginFormat::~VSTPluginFormat() {}

static std::unique_ptr<VSTPluginInstance> createAndUpdateDesc (VSTPluginFormat& format, PluginDescription& desc)
{
    if (auto p = format.createInstanceFromDescription (desc, 44100.0, 512))
    {
        if (auto instance = dynamic_cast<VSTPluginInstance*> (p.release()))
        {
           #if JUCE_MAC
            if (instance->vstModule->resFileId != 0)
                UseResFile (instance->vstModule->resFileId);
           #endif

            instance->fillInPluginDescription (desc);
            return std::unique_ptr<VSTPluginInstance> (instance);
        }

        jassertfalse;
    }

    return {};
}

void VSTPluginFormat::findAllTypesForFile (OwnedArray<PluginDescription>& results,
                                           const String& fileOrIdentifier)
{
    if (! fileMightContainThisPluginType (fileOrIdentifier))
        return;

    PluginDescription desc;
    desc.fileOrIdentifier = fileOrIdentifier;
    desc.uid = 0;

    auto instance = createAndUpdateDesc (*this, desc);

    if (instance == nullptr)
        return;

    if (instance->getVstCategory() != Vst2::kPlugCategShell)
    {
        // Normal plugin...
        results.add (new PluginDescription (desc));

        instance->dispatch (Vst2::effOpen, 0, 0, nullptr, 0);
    }
    else
    {
        // It's a shell plugin, so iterate all the subtypes...
        for (;;)
        {
            char shellEffectName [256] = { 0 };
            auto uid = (int) instance->dispatch (Vst2::effShellGetNextPlugin, 0, 0, shellEffectName, 0);

            if (uid == 0)
                break;

            desc.uid = uid;
            desc.name = shellEffectName;

            aboutToScanVSTShellPlugin (desc);

            std::unique_ptr<VSTPluginInstance> shellInstance (createAndUpdateDesc (*this, desc));

            if (shellInstance != nullptr)
            {
                jassert (desc.uid == uid);
                desc.hasSharedContainer = true;
                desc.name = shellEffectName;

                if (! arrayContainsPlugin (results, desc))
                    results.add (new PluginDescription (desc));
            }
        }
    }
}

void VSTPluginFormat::createPluginInstance (const PluginDescription& desc,
                                            double sampleRate, int blockSize,
                                            PluginCreationCallback callback)
{
    std::unique_ptr<VSTPluginInstance> result;

    if (fileMightContainThisPluginType (desc.fileOrIdentifier))
    {
        File file (desc.fileOrIdentifier);

        auto previousWorkingDirectory = File::getCurrentWorkingDirectory();
        file.getParentDirectory().setAsCurrentWorkingDirectory();

        if (auto module = ModuleHandle::findOrCreateModule (file))
        {
            shellUIDToCreate = desc.uid;

            result.reset (VSTPluginInstance::create (module, sampleRate, blockSize));

            if (result != nullptr && ! result->initialiseEffect (sampleRate, blockSize))
                result.reset();
        }

        previousWorkingDirectory.setAsCurrentWorkingDirectory();
    }

    String errorMsg;

    if (result == nullptr)
        errorMsg = TRANS ("Unable to load XXX plug-in file").replace ("XXX", "VST-2");

    callback (std::move (result), errorMsg);
}

bool VSTPluginFormat::requiresUnblockedMessageThreadDuringCreation (const PluginDescription&) const
{
    return false;
}

bool VSTPluginFormat::fileMightContainThisPluginType (const String& fileOrIdentifier)
{
    auto f = File::createFileWithoutCheckingPath (fileOrIdentifier);

  #if JUCE_MAC || JUCE_IOS
    return f.isDirectory() && f.hasFileExtension (".vst");
  #elif JUCE_WINDOWS
    return f.existsAsFile() && f.hasFileExtension (".dll");
  #elif JUCE_LINUX || JUCE_ANDROID
    return f.existsAsFile() && f.hasFileExtension (".so");
  #endif
}

String VSTPluginFormat::getNameOfPluginFromIdentifier (const String& fileOrIdentifier)
{
    return fileOrIdentifier;
}

bool VSTPluginFormat::pluginNeedsRescanning (const PluginDescription& desc)
{
    return File (desc.fileOrIdentifier).getLastModificationTime() != desc.lastFileModTime;
}

bool VSTPluginFormat::doesPluginStillExist (const PluginDescription& desc)
{
    return File (desc.fileOrIdentifier).exists();
}

StringArray VSTPluginFormat::searchPathsForPlugins (const FileSearchPath& directoriesToSearch, const bool recursive, bool)
{
    StringArray results;

    for (int j = 0; j < directoriesToSearch.getNumPaths(); ++j)
        recursiveFileSearch (results, directoriesToSearch [j], recursive);

    return results;
}

void VSTPluginFormat::recursiveFileSearch (StringArray& results, const File& dir, const bool recursive)
{
    // avoid allowing the dir iterator to be recursive, because we want to avoid letting it delve inside
    // .component or .vst directories.
    DirectoryIterator iter (dir, false, "*", File::findFilesAndDirectories);

    while (iter.next())
    {
        auto f = iter.getFile();
        bool isPlugin = false;

        if (fileMightContainThisPluginType (f.getFullPathName()))
        {
            isPlugin = true;
            results.add (f.getFullPathName());
        }

        if (recursive && (! isPlugin) && f.isDirectory())
            recursiveFileSearch (results, f, true);
    }
}

FileSearchPath VSTPluginFormat::getDefaultLocationsToSearch()
{
   #if JUCE_MAC
    return FileSearchPath ("~/Library/Audio/Plug-Ins/VST;/Library/Audio/Plug-Ins/VST");
   #elif JUCE_LINUX || JUCE_ANDROID
    return FileSearchPath (SystemStats::getEnvironmentVariable ("VST_PATH",
                                                                "/usr/lib/vst;/usr/local/lib/vst;~/.vst")
                             .replace (":", ";"));
   #elif JUCE_WINDOWS
    auto programFiles = File::getSpecialLocation (File::globalApplicationsDirectory).getFullPathName();

    FileSearchPath paths;
    paths.add (WindowsRegistry::getValue ("HKEY_LOCAL_MACHINE\\Software\\VST\\VSTPluginsPath"));
    paths.addIfNotAlreadyThere (programFiles + "\\Steinberg\\VstPlugins");
    paths.removeNonExistentPaths();
    paths.addIfNotAlreadyThere (programFiles + "\\VstPlugins");
    paths.removeRedundantPaths();
    return paths;
   #elif JUCE_IOS
    // on iOS you can only load plug-ins inside the hosts bundle folder
    CFURLRef relativePluginDir = CFBundleCopyBuiltInPlugInsURL (CFBundleGetMainBundle());
    CFURLRef pluginDir = CFURLCopyAbsoluteURL (relativePluginDir);
    CFRelease (relativePluginDir);

    CFStringRef path = CFURLCopyFileSystemPath (pluginDir, kCFURLPOSIXPathStyle);
    CFRelease (pluginDir);

    FileSearchPath retval (String (CFStringGetCStringPtr (path, kCFStringEncodingUTF8)));
    CFRelease (path);

    return retval;
   #endif
}

const XmlElement* VSTPluginFormat::getVSTXML (AudioPluginInstance* plugin)
{
    if (auto* vst = dynamic_cast<VSTPluginInstance*> (plugin))
        if (vst->vstModule != nullptr)
            return vst->vstModule->vstXml.get();

    return nullptr;
}

bool VSTPluginFormat::loadFromFXBFile (AudioPluginInstance* plugin, const void* data, size_t dataSize)
{
    if (auto* vst = dynamic_cast<VSTPluginInstance*> (plugin))
        return vst->loadFromFXBFile (data, dataSize);

    return false;
}

bool VSTPluginFormat::saveToFXBFile (AudioPluginInstance* plugin, MemoryBlock& dest, bool asFXB)
{
    if (auto* vst = dynamic_cast<VSTPluginInstance*> (plugin))
        return vst->saveToFXBFile (dest, asFXB);

    return false;
}

bool VSTPluginFormat::getChunkData (AudioPluginInstance* plugin, MemoryBlock& result, bool isPreset)
{
    if (auto* vst = dynamic_cast<VSTPluginInstance*> (plugin))
        return vst->getChunkData (result, isPreset, 128);

    return false;
}

bool VSTPluginFormat::setChunkData (AudioPluginInstance* plugin, const void* data, int size, bool isPreset)
{
    if (auto* vst = dynamic_cast<VSTPluginInstance*> (plugin))
        return vst->setChunkData (data, size, isPreset);

    return false;
}

AudioPluginInstance* VSTPluginFormat::createCustomVSTFromMainCall (void* entryPointFunction,
                                                                   double initialSampleRate, int initialBufferSize)
{
    ModuleHandle::Ptr module = new ModuleHandle (File(), (MainCall) entryPointFunction);

    if (module->open())
    {
        std::unique_ptr<VSTPluginInstance> result (VSTPluginInstance::create (module, initialSampleRate, initialBufferSize));

        if (result != nullptr)
            if (result->initialiseEffect (initialSampleRate, initialBufferSize))
                return result.release();
    }

    return nullptr;
}

void VSTPluginFormat::setExtraFunctions (AudioPluginInstance* plugin, ExtraFunctions* functions)
{
    std::unique_ptr<ExtraFunctions> f (functions);

    if (auto* vst = dynamic_cast<VSTPluginInstance*> (plugin))
        std::swap (vst->extraFunctions, f);
}

AudioPluginInstance* VSTPluginFormat::getPluginInstanceFromVstEffectInterface (void* aEffect)
{
    if (auto* vstAEffect = reinterpret_cast<Vst2::AEffect*> (aEffect))
        if (auto* instanceVST = reinterpret_cast<VSTPluginInstance*> (vstAEffect->resvd2))
            return dynamic_cast<AudioPluginInstance*> (instanceVST);

    return nullptr;
}

pointer_sized_int JUCE_CALLTYPE VSTPluginFormat::dispatcher (AudioPluginInstance* plugin, int32 opcode, int32 index, pointer_sized_int value, void* ptr, float opt)
{
    if (auto* vst = dynamic_cast<VSTPluginInstance*> (plugin))
        return vst->dispatch (opcode, index, value, ptr, opt);

    return {};
}

void VSTPluginFormat::aboutToScanVSTShellPlugin (const PluginDescription&) {}

} // namespace juce

#endif
