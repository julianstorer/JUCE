/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2020 - Raw Material Software Limited

   JUCE is an open source library subject to commercial or open-source
   licensing.

   By using JUCE, you agree to the terms of both the JUCE 6 End-User License
   Agreement and JUCE Privacy Policy (both effective as of the 16th June 2020).

   End User License Agreement: www.juce.com/juce-6-licence
   Privacy Policy: www.juce.com/juce-privacy-policy

   Or: You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   JUCE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

  ==============================================================================
*/

#include <juce_core/system/juce_TargetPlatform.h>
#include <juce_core/system/juce_CompilerWarnings.h>

//==============================================================================
#if JucePlugin_Build_VST3 && (JUCE_MAC || JUCE_WINDOWS || JUCE_LINUX || JUCE_BSD)

#if JUCE_PLUGINHOST_VST3
 #if JUCE_MAC
  #include <CoreFoundation/CoreFoundation.h>
 #endif
 #undef JUCE_VST3HEADERS_INCLUDE_HEADERS_ONLY
 #define JUCE_VST3HEADERS_INCLUDE_HEADERS_ONLY 1
#endif

#include <juce_audio_processors/format_types/juce_VST3Headers.h>

#undef JUCE_VST3HEADERS_INCLUDE_HEADERS_ONLY
#define JUCE_GUI_BASICS_INCLUDE_XHEADERS 1

#include "../utility/juce_CheckSettingMacros.h"
#include "../utility/juce_IncludeSystemHeaders.h"
#include "../utility/juce_IncludeModuleHeaders.h"
#include "../utility/juce_WindowsHooks.h"
#include "../utility/juce_FakeMouseMoveGenerator.h"
#include "../utility/juce_LinuxMessageThread.h"
#include <juce_audio_processors/format_types/juce_LegacyAudioParameter.cpp>
#include <juce_audio_processors/format_types/juce_VST3Common.h>

#ifndef JUCE_VST3_CAN_REPLACE_VST2
 #define JUCE_VST3_CAN_REPLACE_VST2 1
#endif

#if JUCE_VST3_CAN_REPLACE_VST2

 #if ! JUCE_MSVC
  #define __cdecl
 #endif

 namespace Vst2
 {
 #include "pluginterfaces/vst2.x/vstfxstore.h"
 }

#endif

#ifndef JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS
 #if JucePlugin_WantsMidiInput
  #define JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS 1
 #endif
#endif

#if JUCE_LINUX || JUCE_BSD
 #include <unordered_map>

 std::vector<std::pair<int, std::function<void (int)>>> getFdReadCallbacks();
#endif

#if JUCE_MAC
 #include <juce_core/native/juce_mac_CFHelpers.h>
#endif

namespace juce
{

using namespace Steinberg;

//==============================================================================
#if JUCE_MAC
 extern void initialiseMacVST();

 #if ! JUCE_64BIT
  extern void updateEditorCompBoundsVST (Component*);
 #endif

 extern JUCE_API void* attachComponentToWindowRefVST (Component*, void* parentWindowOrView, bool isNSView);
 extern JUCE_API void detachComponentFromWindowRefVST (Component*, void* nsWindow, bool isNSView);
#endif

#if JUCE_WINDOWS && JUCE_WIN_PER_MONITOR_DPI_AWARE
 extern JUCE_API double getScaleFactorForWindow (HWND);
#endif

//==============================================================================
#if JUCE_LINUX || JUCE_BSD

class EventHandler final  : public Steinberg::Linux::IEventHandler
{
public:
    EventHandler() = default;

    ~EventHandler()
    {
        jassert (hostRunLoops.size() == 0);

        if (! messageThread->isRunning())
            messageThread->start();
    }

    JUCE_DECLARE_VST3_COM_REF_METHODS

    tresult PLUGIN_API queryInterface (const TUID targetIID, void** obj) override
    {
        TEST_FOR_AND_RETURN_IF_VALID (targetIID, Steinberg::Linux::IEventHandler)

        *obj = nullptr;
        return kNoInterface;
    }

    void PLUGIN_API onFDIsSet (Steinberg::Linux::FileDescriptor fd) override
    {
        updateCurrentMessageThread();

        auto it = fdCallbackMap.find (fd);

        if (it != fdCallbackMap.end())
            it->second (fd);
    }

    //==============================================================================
    void registerHandlerForFrame (IPlugFrame* plugFrame)
    {
        if (auto* runLoop = getRunLoopFromFrame (plugFrame))
        {
            if (hostRunLoops.contains (runLoop))
                runLoop->unregisterEventHandler (this);

            hostRunLoops.add (runLoop);

            fdCallbackMap.clear();

            for (auto& cb : getFdReadCallbacks())
            {
                fdCallbackMap[cb.first] = cb.second;
                runLoop->registerEventHandler (this, cb.first);
            }

            updateCurrentMessageThread();
        }
    }

    void unregisterHandlerForFrame (IPlugFrame* plugFrame)
    {
        if (auto* runLoop = getRunLoopFromFrame (plugFrame))
        {
            hostRunLoops.remove (runLoop);

            if (! hostRunLoops.contains (runLoop))
                runLoop->unregisterEventHandler (this);
        }
    }

private:
    //=============================================================================
    class HostRunLoopInterfaces
    {
    public:
        HostRunLoopInterfaces() = default;

        void add (Steinberg::Linux::IRunLoop* runLoop)
        {
            if (auto* refCountedRunLoop = find (runLoop))
            {
                ++(refCountedRunLoop->refCount);
                return;
            }

            runLoops.push_back ({ runLoop, 1 });
        }

        void remove (Steinberg::Linux::IRunLoop* runLoop)
        {
            if (auto* refCountedRunLoop = find (runLoop))
                if (--(refCountedRunLoop->refCount) == 0)
                    runLoops.erase (std::find (runLoops.begin(), runLoops.end(), runLoop));
        }

        size_t size() const noexcept                         { return runLoops.size(); }
        bool contains (Steinberg::Linux::IRunLoop* runLoop)  { return find (runLoop) != nullptr; }

    private:
        struct RefCountedRunLoop
        {
            Steinberg::Linux::IRunLoop* runLoop = nullptr;
            int refCount = 0;

            bool operator== (const Steinberg::Linux::IRunLoop* other) const noexcept { return runLoop == other; }
        };

        RefCountedRunLoop* find (const Steinberg::Linux::IRunLoop* runLoop)
        {
            auto iter = std::find (runLoops.begin(), runLoops.end(), runLoop);

            if (iter != runLoops.end())
                return &(*iter);

            return nullptr;
        }

        std::vector<RefCountedRunLoop> runLoops;

        JUCE_DECLARE_NON_MOVEABLE (HostRunLoopInterfaces)
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HostRunLoopInterfaces)
    };

    static Steinberg::Linux::IRunLoop* getRunLoopFromFrame (IPlugFrame* plugFrame)
    {
        Steinberg::Linux::IRunLoop* runLoop = nullptr;

        if (plugFrame != nullptr)
            plugFrame->queryInterface (Steinberg::Linux::IRunLoop::iid, (void**) &runLoop);

        jassert (runLoop != nullptr);
        return runLoop;
    }

    void updateCurrentMessageThread()
    {
        if (! MessageManager::getInstance()->isThisTheMessageThread())
        {
            if (messageThread->isRunning())
                messageThread->stop();

            MessageManager::getInstance()->setCurrentThreadAsMessageThread();
        }
    }

    SharedResourcePointer<MessageThread> messageThread;

    std::atomic<int> refCount { 1 };

    HostRunLoopInterfaces hostRunLoops;
    std::unordered_map<int, std::function<void (int)>> fdCallbackMap;

    //==============================================================================
    JUCE_DECLARE_NON_MOVEABLE (EventHandler)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EventHandler)
};

#endif

//==============================================================================
class ScopedThreadLocalBooleanSetter
{
public:
    explicit ScopedThreadLocalBooleanSetter (ThreadLocalValue<bool>& ref)
        : toSet (ref)
    {
        jassert (! toSet.get());
        toSet = true;
    }

    ~ScopedThreadLocalBooleanSetter() noexcept { toSet = false; }

private:
    ThreadLocalValue<bool>& toSet;
};

//==============================================================================
class JuceAudioProcessor   : public Vst::IUnitInfo
{
public:
    explicit JuceAudioProcessor (AudioProcessor* source) noexcept
        : audioProcessor (source)
    {
        setupParameters();
    }

    virtual ~JuceAudioProcessor() = default;

    AudioProcessor* get() const noexcept      { return audioProcessor.get(); }

    JUCE_DECLARE_VST3_COM_QUERY_METHODS
    JUCE_DECLARE_VST3_COM_REF_METHODS

    //==============================================================================
    enum InternalParameters
    {
        paramPreset               = 0x70727374, // 'prst'
        paramMidiControllerOffset = 0x6d636d00, // 'mdm*'
        paramBypass               = 0x62797073  // 'byps'
    };

    //==============================================================================
    Steinberg::int32 PLUGIN_API getUnitCount() override
    {
        return parameterGroups.size() + 1;
    }

    tresult PLUGIN_API getUnitInfo (Steinberg::int32 unitIndex, Vst::UnitInfo& info) override
    {
        if (unitIndex == 0)
        {
            info.id             = Vst::kRootUnitId;
            info.parentUnitId   = Vst::kNoParentUnitId;
            info.programListId  = Vst::kNoProgramListId;

            toString128 (info.name, TRANS("Root Unit"));

            return kResultTrue;
        }

        if (auto* group = parameterGroups[unitIndex - 1])
        {
            info.id             = JuceAudioProcessor::getUnitID (group);
            info.parentUnitId   = JuceAudioProcessor::getUnitID (group->getParent());
            info.programListId  = Vst::kNoProgramListId;

            toString128 (info.name, group->getName());

            return kResultTrue;
        }

        return kResultFalse;
    }

    Steinberg::int32 PLUGIN_API getProgramListCount() override
    {
        if (audioProcessor->getNumPrograms() > 0)
            return 1;

        return 0;
    }

    tresult PLUGIN_API getProgramListInfo (Steinberg::int32 listIndex, Vst::ProgramListInfo& info) override
    {
        if (listIndex == 0)
        {
            info.id = static_cast<Vst::ProgramListID> (programParamID);
            info.programCount = static_cast<Steinberg::int32> (audioProcessor->getNumPrograms());

            toString128 (info.name, TRANS("Factory Presets"));

            return kResultTrue;
        }

        jassertfalse;
        zerostruct (info);
        return kResultFalse;
    }

    tresult PLUGIN_API getProgramName (Vst::ProgramListID listId, Steinberg::int32 programIndex, Vst::String128 name) override
    {
        if (listId == static_cast<Vst::ProgramListID> (programParamID)
            && isPositiveAndBelow ((int) programIndex, audioProcessor->getNumPrograms()))
        {
            toString128 (name, audioProcessor->getProgramName ((int) programIndex));
            return kResultTrue;
        }

        jassertfalse;
        toString128 (name, juce::String());
        return kResultFalse;
    }

    tresult PLUGIN_API getProgramInfo (Vst::ProgramListID, Steinberg::int32, Vst::CString, Vst::String128) override             { return kNotImplemented; }
    tresult PLUGIN_API hasProgramPitchNames (Vst::ProgramListID, Steinberg::int32) override                                     { return kNotImplemented; }
    tresult PLUGIN_API getProgramPitchName (Vst::ProgramListID, Steinberg::int32, Steinberg::int16, Vst::String128) override    { return kNotImplemented; }
    tresult PLUGIN_API selectUnit (Vst::UnitID) override                                                                        { return kNotImplemented; }
    tresult PLUGIN_API setUnitProgramData (Steinberg::int32, Steinberg::int32, IBStream*) override                              { return kNotImplemented; }
    Vst::UnitID PLUGIN_API getSelectedUnit() override                                                                           { return Vst::kRootUnitId; }

    tresult PLUGIN_API getUnitByBus (Vst::MediaType, Vst::BusDirection, Steinberg::int32, Steinberg::int32, Vst::UnitID& unitId) override
    {
        zerostruct (unitId);
        return kNotImplemented;
    }

    //==============================================================================
    inline Vst::ParamID getVSTParamIDForIndex (int paramIndex) const noexcept
    {
       #if JUCE_FORCE_USE_LEGACY_PARAM_IDS
        return static_cast<Vst::ParamID> (paramIndex);
       #else
        return vstParamIDs.getReference (paramIndex);
       #endif
    }

    AudioProcessorParameter* getParamForVSTParamID (Vst::ParamID paramID) const noexcept
    {
        return paramMap[static_cast<int32> (paramID)];
    }

    AudioProcessorParameter* getBypassParameter() const noexcept
    {
        return getParamForVSTParamID (bypassParamID);
    }

    AudioProcessorParameter* getProgramParameter() const noexcept
    {
        return getParamForVSTParamID (programParamID);
    }

    static Vst::UnitID getUnitID (const AudioProcessorParameterGroup* group)
    {
        if (group == nullptr || group->getParent() == nullptr)
            return Vst::kRootUnitId;

        // From the VST3 docs (also applicable to unit IDs!):
        // Up to 2^31 parameters can be exported with id range [0, 2147483648]
        // (the range [2147483649, 429496729] is reserved for host application).
        auto unitID = group->getID().hashCode() & 0x7fffffff;

        // If you hit this assertion then your group ID is hashing to a value
        // reserved by the VST3 SDK. Please use a different group ID.
        jassert (unitID != Vst::kRootUnitId);

        return unitID;
    }

    const Array<Vst::ParamID>& getParamIDs() const noexcept { return vstParamIDs; }
    Vst::ParamID getBypassParamID()          const noexcept { return bypassParamID; }
    Vst::ParamID getProgramParamID()         const noexcept { return programParamID; }
    bool isBypassRegularParameter()          const noexcept { return bypassIsRegularParameter; }

    void setParameterValue (Steinberg::int32 paramIndex, float value)
    {
        cachedParamValues.set (paramIndex, value);
    }

    template <typename Callback>
    void forAllChangedParameters (Callback&& callback)
    {
        cachedParamValues.ifSet ([&] (Steinberg::int32 index, float value)
        {
            callback (cachedParamValues.getParamID (index), value);
        });
    }

    bool isUsingManagedParameters() const noexcept    { return juceParameters.isUsingManagedParameters(); }

    //==============================================================================
    static const FUID iid;

private:
    //==============================================================================
    bool isBypassPartOfRegularParemeters() const
    {
        int n = juceParameters.getNumParameters();

        if (auto* bypassParam = audioProcessor->getBypassParameter())
            for (int i = 0; i < n; ++i)
                if (juceParameters.getParamForIndex (i) == bypassParam)
                    return true;

        return false;
    }

    void setupParameters()
    {
        parameterGroups = audioProcessor->getParameterTree().getSubgroups (true);

       #if JUCE_DEBUG
        auto allGroups = parameterGroups;
        allGroups.add (&audioProcessor->getParameterTree());
        std::unordered_set<Vst::UnitID> unitIDs;

        for (auto* group : allGroups)
        {
            auto insertResult = unitIDs.insert (getUnitID (group));

            // If you hit this assertion then either a group ID is not unique or
            // you are very unlucky and a hashed group ID is not unique
            jassert (insertResult.second);
        }
       #endif

       #if JUCE_FORCE_USE_LEGACY_PARAM_IDS
        const bool forceLegacyParamIDs = true;
       #else
        const bool forceLegacyParamIDs = false;
       #endif

        juceParameters.update (*audioProcessor, forceLegacyParamIDs);
        auto numParameters = juceParameters.getNumParameters();

        bool vst3WrapperProvidedBypassParam = false;
        auto* bypassParameter = audioProcessor->getBypassParameter();

        if (bypassParameter == nullptr)
        {
            vst3WrapperProvidedBypassParam = true;
            ownedBypassParameter.reset (new AudioParameterBool ("byps", "Bypass", false, {}, {}, {}));
            bypassParameter = ownedBypassParameter.get();
        }

        // if the bypass parameter is not part of the exported parameters that the plug-in supports
        // then add it to the end of the list as VST3 requires the bypass parameter to be exported!
        bypassIsRegularParameter = isBypassPartOfRegularParemeters();

        if (! bypassIsRegularParameter)
            juceParameters.params.add (bypassParameter);

        int i = 0;
        for (auto* juceParam : juceParameters.params)
        {
            bool isBypassParameter = (juceParam == bypassParameter);

            Vst::ParamID vstParamID = forceLegacyParamIDs ? static_cast<Vst::ParamID> (i++)
                                                          : generateVSTParamIDForParam (juceParam);

            if (isBypassParameter)
            {
                // we need to remain backward compatible with the old bypass id
                if (vst3WrapperProvidedBypassParam)
                    vstParamID = static_cast<Vst::ParamID> ((isUsingManagedParameters() && ! forceLegacyParamIDs) ? paramBypass : numParameters);

                bypassParamID = vstParamID;
            }

            vstParamIDs.add (vstParamID);
            paramMap.set (static_cast<int32> (vstParamID), juceParam);
        }

        auto numPrograms = audioProcessor->getNumPrograms();

        if (numPrograms > 1)
        {
            ownedProgramParameter = std::make_unique<AudioParameterInt> ("juceProgramParameter", "Program",
                                                                         0, numPrograms - 1,
                                                                         audioProcessor->getCurrentProgram());

            juceParameters.params.add (ownedProgramParameter.get());

            if (forceLegacyParamIDs)
                programParamID = static_cast<Vst::ParamID> (i++);

            vstParamIDs.add (programParamID);
            paramMap.set (static_cast<int32> (programParamID), ownedProgramParameter.get());
        }

        cachedParamValues = CachedParamValues { { vstParamIDs.begin(), vstParamIDs.end() } };
    }

    Vst::ParamID generateVSTParamIDForParam (AudioProcessorParameter* param)
    {
        auto juceParamID = LegacyAudioParameter::getParamID (param, false);

      #if JUCE_FORCE_USE_LEGACY_PARAM_IDS
        return static_cast<Vst::ParamID> (juceParamID.getIntValue());
      #else
        auto paramHash = static_cast<Vst::ParamID> (juceParamID.hashCode());

       #if JUCE_USE_STUDIO_ONE_COMPATIBLE_PARAMETERS
        // studio one doesn't like negative parameters
        paramHash &= ~(((Vst::ParamID) 1) << (sizeof (Vst::ParamID) * 8 - 1));
       #endif

        return paramHash;
      #endif
    }

    //==============================================================================
    Array<Vst::ParamID> vstParamIDs;
    CachedParamValues cachedParamValues;
    Vst::ParamID bypassParamID = 0, programParamID = static_cast<Vst::ParamID> (paramPreset);
    bool bypassIsRegularParameter = false;

    //==============================================================================
    std::atomic<int> refCount { 0 };
    std::unique_ptr<AudioProcessor> audioProcessor;

    //==============================================================================
    LegacyAudioParametersWrapper juceParameters;
    HashMap<int32, AudioProcessorParameter*> paramMap;
    std::unique_ptr<AudioProcessorParameter> ownedBypassParameter, ownedProgramParameter;
    Array<const AudioProcessorParameterGroup*> parameterGroups;

    JuceAudioProcessor() = delete;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JuceAudioProcessor)
};

class JuceVST3Component;

static ThreadLocalValue<bool> inParameterChangedCallback;

//==============================================================================
class JuceVST3EditController : public Vst::EditController,
                               public Vst::IMidiMapping,
                               public Vst::IUnitInfo,
                               public Vst::ChannelContext::IInfoListener,
                               public AudioProcessorListener,
                               private ComponentRestarter::Listener
{
public:
    JuceVST3EditController (Vst::IHostApplication* host)
    {
        if (host != nullptr)
            host->queryInterface (FUnknown::iid, (void**) &hostContext);
    }

    //==============================================================================
    static const FUID iid;

    //==============================================================================
    JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Winconsistent-missing-override")

    REFCOUNT_METHODS (ComponentBase)

    JUCE_END_IGNORE_WARNINGS_GCC_LIKE

    tresult PLUGIN_API queryInterface (const TUID targetIID, void** obj) override
    {
        TEST_FOR_AND_RETURN_IF_VALID (targetIID, FObject)
        TEST_FOR_AND_RETURN_IF_VALID (targetIID, JuceVST3EditController)
        TEST_FOR_AND_RETURN_IF_VALID (targetIID, Vst::IEditController)
        TEST_FOR_AND_RETURN_IF_VALID (targetIID, Vst::IEditController2)
        TEST_FOR_AND_RETURN_IF_VALID (targetIID, Vst::IConnectionPoint)
        TEST_FOR_AND_RETURN_IF_VALID (targetIID, Vst::IMidiMapping)
        TEST_FOR_AND_RETURN_IF_VALID (targetIID, Vst::IUnitInfo)
        TEST_FOR_AND_RETURN_IF_VALID (targetIID, Vst::ChannelContext::IInfoListener)
        TEST_FOR_COMMON_BASE_AND_RETURN_IF_VALID (targetIID, IPluginBase, Vst::IEditController)
        TEST_FOR_COMMON_BASE_AND_RETURN_IF_VALID (targetIID, IDependent, Vst::IEditController)
        TEST_FOR_COMMON_BASE_AND_RETURN_IF_VALID (targetIID, FUnknown, Vst::IEditController)

        if (doUIDsMatch (targetIID, JuceAudioProcessor::iid))
        {
            audioProcessor->addRef();
            *obj = audioProcessor;
            return kResultOk;
        }

        *obj = nullptr;
        return kNoInterface;
    }

    //==============================================================================
    tresult PLUGIN_API initialize (FUnknown* context) override
    {
        if (hostContext != context)
            hostContext = context;

        return kResultTrue;
    }

    tresult PLUGIN_API terminate() override
    {
        if (auto* pluginInstance = getPluginInstance())
            pluginInstance->removeListener (this);

        audioProcessor = nullptr;

        return EditController::terminate();
    }

    //==============================================================================
    struct Param  : public Vst::Parameter
    {
        Param (JuceVST3EditController& editController, AudioProcessorParameter& p,
               Vst::ParamID vstParamID, Vst::UnitID vstUnitID,
               bool isBypassParameter)
            : owner (editController), param (p)
        {
            info.id = vstParamID;
            info.unitId = vstUnitID;

            updateParameterInfo();

            info.stepCount = (Steinberg::int32) 0;

           #if ! JUCE_FORCE_LEGACY_PARAMETER_AUTOMATION_TYPE
            if (param.isDiscrete())
           #endif
            {
                const int numSteps = param.getNumSteps();
                info.stepCount = (Steinberg::int32) (numSteps > 0 && numSteps < 0x7fffffff ? numSteps - 1 : 0);
            }

            info.defaultNormalizedValue = param.getDefaultValue();
            jassert (info.defaultNormalizedValue >= 0 && info.defaultNormalizedValue <= 1.0f);

            // Is this a meter?
            if ((((unsigned int) param.getCategory() & 0xffff0000) >> 16) == 2)
                info.flags = Vst::ParameterInfo::kIsReadOnly;
            else
                info.flags = param.isAutomatable() ? Vst::ParameterInfo::kCanAutomate : 0;

            if (isBypassParameter)
                info.flags |= Vst::ParameterInfo::kIsBypass;

            valueNormalized = info.defaultNormalizedValue;
        }

        bool updateParameterInfo()
        {
            auto updateParamIfChanged = [] (Vst::String128& paramToUpdate, const String& newValue)
            {
                if (juce::toString (paramToUpdate) == newValue)
                    return false;

                toString128 (paramToUpdate, newValue);
                return true;
            };

            auto anyUpdated = updateParamIfChanged (info.title,      param.getName (128));
            anyUpdated     |= updateParamIfChanged (info.shortTitle, param.getName (8));
            anyUpdated     |= updateParamIfChanged (info.units,      param.getLabel());

            return anyUpdated;
        }

        bool setNormalized (Vst::ParamValue v) override
        {
            v = jlimit (0.0, 1.0, v);

            if (v != valueNormalized)
            {
                valueNormalized = v;

                // Only update the AudioProcessor here if we're not playing,
                // otherwise we get parallel streams of parameter value updates
                // during playback
                if (! owner.vst3IsPlaying)
                {
                    auto value = static_cast<float> (v);

                    param.setValue (value);

                    ScopedThreadLocalBooleanSetter scope { inParameterChangedCallback };
                    param.sendValueChangedMessageToListeners (value);
                }

                changed();
                return true;
            }

            return false;
        }

        void toString (Vst::ParamValue value, Vst::String128 result) const override
        {
            if (LegacyAudioParameter::isLegacy (&param))
                // remain backward-compatible with old JUCE code
                toString128 (result, param.getCurrentValueAsText());
            else
                toString128 (result, param.getText ((float) value, 128));
        }

        bool fromString (const Vst::TChar* text, Vst::ParamValue& outValueNormalized) const override
        {
            if (! LegacyAudioParameter::isLegacy (&param))
            {
                outValueNormalized = param.getValueForText (getStringFromVstTChars (text));
                return true;
            }

            return false;
        }

        static String getStringFromVstTChars (const Vst::TChar* text)
        {
            return juce::String (juce::CharPointer_UTF16 (reinterpret_cast<const juce::CharPointer_UTF16::CharType*> (text)));
        }

        Vst::ParamValue toPlain (Vst::ParamValue v) const override       { return v; }
        Vst::ParamValue toNormalized (Vst::ParamValue v) const override  { return v; }

    private:
        JuceVST3EditController& owner;
        AudioProcessorParameter& param;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Param)
    };

    //==============================================================================
    struct ProgramChangeParameter  : public Vst::Parameter
    {
        ProgramChangeParameter (AudioProcessor& p, Vst::ParamID vstParamID)
            : owner (p)
        {
            jassert (owner.getNumPrograms() > 1);

            info.id = vstParamID;
            toString128 (info.title, "Program");
            toString128 (info.shortTitle, "Program");
            toString128 (info.units, "");
            info.stepCount = owner.getNumPrograms() - 1;
            info.defaultNormalizedValue = static_cast<Vst::ParamValue> (owner.getCurrentProgram())
                                            / static_cast<Vst::ParamValue> (info.stepCount);
            info.unitId = Vst::kRootUnitId;
            info.flags = Vst::ParameterInfo::kIsProgramChange | Vst::ParameterInfo::kCanAutomate;
        }

        ~ProgramChangeParameter() override = default;

        bool setNormalized (Vst::ParamValue v) override
        {
            auto programValue = roundToInt (toPlain (v));

            if (isPositiveAndBelow (programValue, owner.getNumPrograms()))
            {
                if (programValue != owner.getCurrentProgram())
                    owner.setCurrentProgram (programValue);

                if (valueNormalized != v)
                {
                    valueNormalized = v;
                    changed();

                    return true;
                }
            }

            return false;
        }

        void toString (Vst::ParamValue value, Vst::String128 result) const override
        {
            toString128 (result, owner.getProgramName (roundToInt (value * info.stepCount)));
        }

        bool fromString (const Vst::TChar* text, Vst::ParamValue& outValueNormalized) const override
        {
            auto paramValueString = getStringFromVstTChars (text);
            auto n = owner.getNumPrograms();

            for (int i = 0; i < n; ++i)
            {
                if (paramValueString == owner.getProgramName (i))
                {
                    outValueNormalized = static_cast<Vst::ParamValue> (i) / info.stepCount;
                    return true;
                }
            }

            return false;
        }

        static String getStringFromVstTChars (const Vst::TChar* text)
        {
            return String (CharPointer_UTF16 (reinterpret_cast<const CharPointer_UTF16::CharType*> (text)));
        }

        Vst::ParamValue toPlain (Vst::ParamValue v) const override       { return v * info.stepCount; }
        Vst::ParamValue toNormalized (Vst::ParamValue v) const override  { return v / info.stepCount; }

    private:
        AudioProcessor& owner;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProgramChangeParameter)
    };

    //==============================================================================
    tresult PLUGIN_API setChannelContextInfos (Vst::IAttributeList* list) override
    {
        if (auto* instance = getPluginInstance())
        {
            if (list != nullptr)
            {
                AudioProcessor::TrackProperties trackProperties;

                {
                    Vst::String128 channelName;
                    if (list->getString (Vst::ChannelContext::kChannelNameKey, channelName, sizeof (channelName)) == kResultTrue)
                        trackProperties.name = toString (channelName);
                }

                {
                    int64 colour;
                    if (list->getInt (Vst::ChannelContext::kChannelColorKey, colour) == kResultTrue)
                        trackProperties.colour = Colour (Vst::ChannelContext::GetRed ((uint32) colour),  Vst::ChannelContext::GetGreen ((uint32) colour),
                                                         Vst::ChannelContext::GetBlue ((uint32) colour), Vst::ChannelContext::GetAlpha ((uint32) colour));
                }



                if (MessageManager::getInstance()->isThisTheMessageThread())
                    instance->updateTrackProperties (trackProperties);
                else
                    MessageManager::callAsync ([trackProperties, instance]
                                               { instance->updateTrackProperties (trackProperties); });
            }
        }

        return kResultOk;
    }

    //==============================================================================
    tresult PLUGIN_API setComponentState (IBStream* stream) override
    {
        if (! MessageManager::existsAndIsCurrentThread())
       #if JUCE_LINUX || JUCE_BSD
        {
            tresult result = kResultOk;
            WaitableEvent finishedEvent;

            MessageManager::callAsync ([&]
            {
                result = setComponentState (stream);
                finishedEvent.signal();
            });

            finishedEvent.wait();
            return result;
        }
       #else
        // As an IEditController member, the host should only call this from the message thread.
        jassertfalse;
       #endif

        if (auto* pluginInstance = getPluginInstance())
        {
            for (auto vstParamId : audioProcessor->getParamIDs())
            {
                auto paramValue = [&]
                {
                    if (vstParamId == audioProcessor->getProgramParamID())
                        return EditController::plainParamToNormalized (audioProcessor->getProgramParamID(),
                                                                       pluginInstance->getCurrentProgram());

                    return (double) audioProcessor->getParamForVSTParamID (vstParamId)->getValue();
                }();

                setParamNormalized (vstParamId, paramValue);
            }
        }

        if (auto* handler = getComponentHandler())
            handler->restartComponent (Vst::kParamValuesChanged);

        return Vst::EditController::setComponentState (stream);
    }

    void setAudioProcessor (JuceAudioProcessor* audioProc)
    {
        if (audioProcessor != audioProc)
            installAudioProcessor (audioProc);
    }

    tresult PLUGIN_API connect (IConnectionPoint* other) override
    {
        if (other != nullptr && audioProcessor == nullptr)
        {
            auto result = ComponentBase::connect (other);

            if (! audioProcessor.loadFrom (other))
                sendIntMessage ("JuceVST3EditController", (Steinberg::int64) (pointer_sized_int) this);
            else
                installAudioProcessor (audioProcessor);

            return result;
        }

        jassertfalse;
        return kResultFalse;
    }

    //==============================================================================
    tresult PLUGIN_API getMidiControllerAssignment (Steinberg::int32 /*busIndex*/, Steinberg::int16 channel,
                                                    Vst::CtrlNumber midiControllerNumber, Vst::ParamID& resultID) override
    {
       #if JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS
        resultID = midiControllerToParameter[channel][midiControllerNumber];
        return kResultTrue; // Returning false makes some hosts stop asking for further MIDI Controller Assignments
       #else
        ignoreUnused (channel, midiControllerNumber, resultID);
        return kResultFalse;
       #endif
    }

    // Converts an incoming parameter index to a MIDI controller:
    bool getMidiControllerForParameter (Vst::ParamID index, int& channel, int& ctrlNumber)
    {
        auto mappedIndex = static_cast<int> (index - parameterToMidiControllerOffset);

        if (isPositiveAndBelow (mappedIndex, numElementsInArray (parameterToMidiController)))
        {
            auto& mc = parameterToMidiController[mappedIndex];

            if (mc.channel != -1 && mc.ctrlNumber != -1)
            {
                channel = jlimit (1, 16, mc.channel + 1);
                ctrlNumber = mc.ctrlNumber;
                return true;
            }
        }

        return false;
    }

    inline bool isMidiControllerParamID (Vst::ParamID paramID) const noexcept
    {
        return (paramID >= parameterToMidiControllerOffset
                    && isPositiveAndBelow (paramID - parameterToMidiControllerOffset,
                                           static_cast<Vst::ParamID> (numElementsInArray (parameterToMidiController))));
    }

    //==============================================================================
    Steinberg::int32 PLUGIN_API getUnitCount() override
    {
        if (audioProcessor != nullptr)
            return audioProcessor->getUnitCount();

        jassertfalse;
        return 1;
    }

    tresult PLUGIN_API getUnitInfo (Steinberg::int32 unitIndex, Vst::UnitInfo& info) override
    {
        if (audioProcessor != nullptr)
            return audioProcessor->getUnitInfo (unitIndex, info);

        if (unitIndex == 0)
        {
            info.id             = Vst::kRootUnitId;
            info.parentUnitId   = Vst::kNoParentUnitId;
            info.programListId  = Vst::kNoProgramListId;

            toString128 (info.name, TRANS("Root Unit"));

            return kResultTrue;
        }

        jassertfalse;
        zerostruct (info);
        return kResultFalse;
    }

    Steinberg::int32 PLUGIN_API getProgramListCount() override
    {
        if (audioProcessor != nullptr)
            return audioProcessor->getProgramListCount();

        jassertfalse;
        return 0;
    }

    tresult PLUGIN_API getProgramListInfo (Steinberg::int32 listIndex, Vst::ProgramListInfo& info) override
    {
        if (audioProcessor != nullptr)
            return audioProcessor->getProgramListInfo (listIndex, info);

        jassertfalse;
        zerostruct (info);
        return kResultFalse;
    }

    tresult PLUGIN_API getProgramName (Vst::ProgramListID listId, Steinberg::int32 programIndex, Vst::String128 name) override
    {
        if (audioProcessor != nullptr)
            return audioProcessor->getProgramName (listId, programIndex, name);

        jassertfalse;
        toString128 (name, juce::String());
        return kResultFalse;
    }

    tresult PLUGIN_API getProgramInfo (Vst::ProgramListID listId, Steinberg::int32 programIndex,
                                       Vst::CString attributeId, Vst::String128 attributeValue) override
    {
        if (audioProcessor != nullptr)
            return audioProcessor->getProgramInfo (listId, programIndex, attributeId, attributeValue);

        jassertfalse;
        return kResultFalse;
    }

    tresult PLUGIN_API hasProgramPitchNames (Vst::ProgramListID listId, Steinberg::int32 programIndex) override
    {
        if (audioProcessor != nullptr)
            return audioProcessor->hasProgramPitchNames (listId, programIndex);

        jassertfalse;
        return kResultFalse;
    }

    tresult PLUGIN_API getProgramPitchName (Vst::ProgramListID listId, Steinberg::int32 programIndex,
                                            Steinberg::int16 midiPitch, Vst::String128 name) override
    {
        if (audioProcessor != nullptr)
            return audioProcessor->getProgramPitchName (listId, programIndex, midiPitch, name);

        jassertfalse;
        return kResultFalse;
    }

    tresult PLUGIN_API selectUnit (Vst::UnitID unitId) override
    {
        if (audioProcessor != nullptr)
            return audioProcessor->selectUnit (unitId);

        jassertfalse;
        return kResultFalse;
    }

    tresult PLUGIN_API setUnitProgramData (Steinberg::int32 listOrUnitId, Steinberg::int32 programIndex,
                                           Steinberg::IBStream* data) override
    {
        if (audioProcessor != nullptr)
            return audioProcessor->setUnitProgramData (listOrUnitId, programIndex, data);

        jassertfalse;
        return kResultFalse;
    }

    Vst::UnitID PLUGIN_API getSelectedUnit() override
    {
        if (audioProcessor != nullptr)
            return audioProcessor->getSelectedUnit();

        jassertfalse;
        return kResultFalse;
    }

    tresult PLUGIN_API getUnitByBus (Vst::MediaType type, Vst::BusDirection dir, Steinberg::int32 busIndex,
                                     Steinberg::int32 channel, Vst::UnitID& unitId) override
    {
        if (audioProcessor != nullptr)
            return audioProcessor->getUnitByBus (type, dir, busIndex, channel, unitId);

        jassertfalse;
        return kResultFalse;
    }

    //==============================================================================
    IPlugView* PLUGIN_API createView (const char* name) override
    {
        if (auto* pluginInstance = getPluginInstance())
        {
            const auto mayCreateEditor = pluginInstance->hasEditor()
                                      && name != nullptr
                                      && std::strcmp (name, Vst::ViewType::kEditor) == 0
                                      && (pluginInstance->getActiveEditor() == nullptr || getHostType().isAdobeAudition());

            if (mayCreateEditor)
                return new JuceVST3Editor (*this, *pluginInstance);
        }

        return nullptr;
    }

    //==============================================================================
    void beginGesture (Vst::ParamID vstParamId)
    {
        if (MessageManager::getInstance()->isThisTheMessageThread())
            beginEdit (vstParamId);
    }

    void endGesture (Vst::ParamID vstParamId)
    {
        if (MessageManager::getInstance()->isThisTheMessageThread())
            endEdit (vstParamId);
    }

    void paramChanged (Steinberg::int32 parameterIndex, Vst::ParamID vstParamId, double newValue)
    {
        if (inParameterChangedCallback.get())
            return;

        if (MessageManager::getInstance()->isThisTheMessageThread())
        {
            // NB: Cubase has problems if performEdit is called without setParamNormalized
            EditController::setParamNormalized (vstParamId, newValue);
            performEdit (vstParamId, newValue);
        }
        else
        {
            audioProcessor->setParameterValue (parameterIndex, (float) newValue);
        }
    }

    //==============================================================================
    void audioProcessorParameterChangeGestureBegin (AudioProcessor*, int index) override
    {
        beginGesture (audioProcessor->getVSTParamIDForIndex (index));
    }

    void audioProcessorParameterChangeGestureEnd (AudioProcessor*, int index) override
    {
        endGesture (audioProcessor->getVSTParamIDForIndex (index));
    }

    void audioProcessorParameterChanged (AudioProcessor*, int index, float newValue) override
    {
        paramChanged (index, audioProcessor->getVSTParamIDForIndex (index), newValue);
    }

    void audioProcessorChanged (AudioProcessor*, const ChangeDetails& details) override
    {
        int32 flags = 0;

        if (details.parameterInfoChanged)
        {
            for (int32 i = 0; i < parameters.getParameterCount(); ++i)
                if (auto* param = dynamic_cast<Param*> (parameters.getParameterByIndex (i)))
                    if (param->updateParameterInfo() && (flags & Vst::kParamTitlesChanged) == 0)
                        flags |= Vst::kParamTitlesChanged;
        }

        if (auto* pluginInstance = getPluginInstance())
        {
            if (details.programChanged)
            {
                if (auto* programParameter = audioProcessor->getProgramParameter())
                {
                    const auto programParameterIndex = programParameter->getParameterIndex();
                    const auto programParameterId = audioProcessor->getProgramParamID();
                    const auto currentProgram = pluginInstance->getCurrentProgram();
                    const auto paramValue = roundToInt (EditController::normalizedParamToPlain (programParameterId,
                                                                                                EditController::getParamNormalized (programParameterId)));

                    if (currentProgram != paramValue)
                    {
                        beginGesture (programParameterId);
                        paramChanged (programParameterIndex,
                                      programParameterId,
                                      EditController::plainParamToNormalized (programParameterId, currentProgram));
                        endGesture (programParameterId);

                        flags |= Vst::kParamValuesChanged;
                    }
                }
            }

            auto latencySamples = pluginInstance->getLatencySamples();

            if (details.latencyChanged && latencySamples != lastLatencySamples)
            {
                flags |= Vst::kLatencyChanged;
                lastLatencySamples = latencySamples;
            }
        }

        if (! inSetupProcessing)
            componentRestarter.restart (flags);
    }

    //==============================================================================
    AudioProcessor* getPluginInstance() const noexcept
    {
        if (audioProcessor != nullptr)
            return audioProcessor->get();

        return nullptr;
    }

private:
    friend class JuceVST3Component;
    friend struct Param;

    //==============================================================================
    VSTComSmartPtr<JuceAudioProcessor> audioProcessor;

    struct MidiController
    {
        int channel = -1, ctrlNumber = -1;
    };

    ComponentRestarter componentRestarter { *this };

    enum { numMIDIChannels = 16 };
    Vst::ParamID parameterToMidiControllerOffset;
    MidiController parameterToMidiController[(int) numMIDIChannels * (int) Vst::kCountCtrlNumber];
    Vst::ParamID midiControllerToParameter[numMIDIChannels][Vst::kCountCtrlNumber];

    void restartComponentOnMessageThread (int32 flags) override
    {
        if (auto* handler = componentHandler)
            handler->restartComponent (flags);
    }

    //==============================================================================
    struct OwnedParameterListener  : public AudioProcessorParameter::Listener
    {
        OwnedParameterListener (JuceVST3EditController& editController,
                                AudioProcessorParameter& juceParameter,
                                Vst::ParamID paramID)
            : owner (editController),
              vstParamID (paramID)
        {
            juceParameter.addListener (this);
        }

        void parameterValueChanged (int index, float newValue) override
        {
            owner.paramChanged (index, vstParamID, newValue);
        }

        void parameterGestureChanged (int, bool gestureIsStarting) override
        {
            if (gestureIsStarting)
                owner.beginGesture (vstParamID);
            else
                owner.endGesture (vstParamID);
        }

        JuceVST3EditController& owner;
        Vst::ParamID vstParamID;
    };

    std::vector<std::unique_ptr<OwnedParameterListener>> ownedParameterListeners;

    //==============================================================================
    std::atomic<bool> vst3IsPlaying     { false },
                      inSetupProcessing { false };

    int lastLatencySamples = 0;

   #if ! JUCE_MAC
    float lastScaleFactorReceived = 1.0f;
   #endif

    void installAudioProcessor (const VSTComSmartPtr<JuceAudioProcessor>& newAudioProcessor)
    {
        audioProcessor = newAudioProcessor;

        if (auto* pluginInstance = getPluginInstance())
        {
            lastLatencySamples = pluginInstance->getLatencySamples();

            pluginInstance->addListener (this);

            // as the bypass is not part of the regular parameters we need to listen for it explicitly
            if (! audioProcessor->isBypassRegularParameter())
                ownedParameterListeners.push_back (std::make_unique<OwnedParameterListener> (*this,
                                                                                             *audioProcessor->getBypassParameter(),
                                                                                             audioProcessor->getBypassParamID()));

            if (parameters.getParameterCount() <= 0)
            {
                auto n = audioProcessor->getParamIDs().size();

                for (int i = 0; i < n; ++i)
                {
                    auto vstParamID = audioProcessor->getVSTParamIDForIndex (i);

                    if (vstParamID == audioProcessor->getProgramParamID())
                        continue;

                    auto* juceParam = audioProcessor->getParamForVSTParamID (vstParamID);
                    auto* parameterGroup = pluginInstance->getParameterTree().getGroupsForParameter (juceParam).getLast();
                    auto unitID = JuceAudioProcessor::getUnitID (parameterGroup);

                    parameters.addParameter (new Param (*this, *juceParam, vstParamID, unitID,
                                                        (vstParamID == audioProcessor->getBypassParamID())));
                }

                if (auto* programParam = audioProcessor->getProgramParameter())
                {
                    ownedParameterListeners.push_back (std::make_unique<OwnedParameterListener> (*this,
                                                                                                 *programParam,
                                                                                                 audioProcessor->getProgramParamID()));

                    parameters.addParameter (new ProgramChangeParameter (*pluginInstance, audioProcessor->getProgramParamID()));
                }
            }

           #if JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS
            parameterToMidiControllerOffset = static_cast<Vst::ParamID> (audioProcessor->isUsingManagedParameters() ? JuceAudioProcessor::paramMidiControllerOffset
                                                                                                                    : parameters.getParameterCount());

            initialiseMidiControllerMappings();
           #endif

            audioProcessorChanged (pluginInstance, ChangeDetails().withParameterInfoChanged (true));
        }
    }

    void initialiseMidiControllerMappings()
    {
        for (int c = 0, p = 0; c < numMIDIChannels; ++c)
        {
            for (int i = 0; i < Vst::kCountCtrlNumber; ++i, ++p)
            {
                midiControllerToParameter[c][i] = static_cast<Vst::ParamID> (p) + parameterToMidiControllerOffset;
                parameterToMidiController[p].channel = c;
                parameterToMidiController[p].ctrlNumber = i;

                parameters.addParameter (new Vst::Parameter (toString ("MIDI CC " + String (c) + "|" + String (i)),
                                         static_cast<Vst::ParamID> (p) + parameterToMidiControllerOffset, nullptr, 0, 0,
                                         0, Vst::kRootUnitId));
            }
        }
    }

    void sendIntMessage (const char* idTag, const Steinberg::int64 value)
    {
        jassert (hostContext != nullptr);

        if (auto* message = allocateMessage())
        {
            const FReleaser releaser (message);
            message->setMessageID (idTag);
            message->getAttributes()->setInt (idTag, value);
            sendMessage (message);
        }
    }

    //==============================================================================
    class JuceVST3Editor  : public Vst::EditorView,
                            public Steinberg::IPlugViewContentScaleSupport,
                            private Timer
    {
    public:
        JuceVST3Editor (JuceVST3EditController& ec, AudioProcessor& p)
          : Vst::EditorView (&ec, nullptr),
            owner (&ec),
            pluginInstance (p)
        {
            createContentWrapperComponentIfNeeded();

           #if JUCE_MAC
            if (getHostType().type == PluginHostType::SteinbergCubase10)
                cubase10Workaround.reset (new Cubase10WindowResizeWorkaround (*this));
           #else
            if (! approximatelyEqual (editorScaleFactor, ec.lastScaleFactorReceived))
                setContentScaleFactor (ec.lastScaleFactorReceived);
           #endif
        }

        tresult PLUGIN_API queryInterface (const TUID targetIID, void** obj) override
        {
            TEST_FOR_AND_RETURN_IF_VALID (targetIID, Steinberg::IPlugViewContentScaleSupport)
            return Vst::EditorView::queryInterface (targetIID, obj);
        }

        REFCOUNT_METHODS (Vst::EditorView)

        //==============================================================================
        tresult PLUGIN_API isPlatformTypeSupported (FIDString type) override
        {
            if (type != nullptr && pluginInstance.hasEditor())
            {
               #if JUCE_WINDOWS
                if (strcmp (type, kPlatformTypeHWND) == 0)
               #elif JUCE_MAC
                if (strcmp (type, kPlatformTypeNSView) == 0 || strcmp (type, kPlatformTypeHIView) == 0)
               #elif JUCE_LINUX || JUCE_BSD
                if (strcmp (type, kPlatformTypeX11EmbedWindowID) == 0)
               #endif
                    return kResultTrue;
            }

            return kResultFalse;
        }

        tresult PLUGIN_API attached (void* parent, FIDString type) override
        {
            if (parent == nullptr || isPlatformTypeSupported (type) == kResultFalse)
                return kResultFalse;

           #if JUCE_LINUX || JUCE_BSD
            eventHandler->registerHandlerForFrame (plugFrame);
           #endif

            systemWindow = parent;

            createContentWrapperComponentIfNeeded();

           #if JUCE_WINDOWS || JUCE_LINUX || JUCE_BSD
            component->setOpaque (true);
            component->addToDesktop (0, (void*) systemWindow);
            component->setVisible (true);

            #if JUCE_WINDOWS && JUCE_WIN_PER_MONITOR_DPI_AWARE
             component->checkHostWindowScaleFactor();
             component->startTimer (500);
            #endif

           #else
            isNSView = (strcmp (type, kPlatformTypeNSView) == 0);
            macHostWindow = juce::attachComponentToWindowRefVST (component.get(), parent, isNSView);
           #endif

            component->resizeHostWindow();
            attachedToParent();

            // Life's too short to faff around with wave lab
            if (getHostType().isWavelab())
                startTimer (200);

            return kResultTrue;
        }

        tresult PLUGIN_API removed() override
        {
            if (component != nullptr)
            {
               #if JUCE_WINDOWS
                component->removeFromDesktop();
               #elif JUCE_MAC
                if (macHostWindow != nullptr)
                {
                    juce::detachComponentFromWindowRefVST (component.get(), macHostWindow, isNSView);
                    macHostWindow = nullptr;
                }
               #endif

                component = nullptr;
            }

           #if JUCE_LINUX || JUCE_BSD
            eventHandler->unregisterHandlerForFrame (plugFrame);
           #endif

            return CPluginView::removed();
        }

        tresult PLUGIN_API onSize (ViewRect* newSize) override
        {
            if (newSize != nullptr)
            {
                rect = convertFromHostBounds (*newSize);

                if (component != nullptr)
                {
                    component->setSize (rect.getWidth(), rect.getHeight());

                   #if JUCE_MAC
                    if (cubase10Workaround != nullptr)
                    {
                        cubase10Workaround->triggerAsyncUpdate();
                    }
                    else
                   #endif
                    {
                        if (auto* peer = component->getPeer())
                            peer->updateBounds();
                    }
                }

                return kResultTrue;
            }

            jassertfalse;
            return kResultFalse;
        }

        tresult PLUGIN_API getSize (ViewRect* size) override
        {
           #if JUCE_WINDOWS && JUCE_WIN_PER_MONITOR_DPI_AWARE
            if (getHostType().isAbletonLive() && systemWindow == nullptr)
                return kResultFalse;
           #endif

            if (size != nullptr && component != nullptr)
            {
                auto editorBounds = component->getSizeToContainChild();

                *size = convertToHostBounds ({ 0, 0, editorBounds.getWidth(), editorBounds.getHeight() });
                return kResultTrue;
            }

            return kResultFalse;
        }

        tresult PLUGIN_API canResize() override
        {
            if (component != nullptr)
                if (auto* editor = component->pluginEditor.get())
                    if (editor->isResizable())
                        return kResultTrue;

            return kResultFalse;
        }

        tresult PLUGIN_API checkSizeConstraint (ViewRect* rectToCheck) override
        {
            if (rectToCheck != nullptr && component != nullptr)
            {
                if (auto* editor = component->pluginEditor.get())
                {
                    if (auto* constrainer = editor->getConstrainer())
                    {
                        *rectToCheck = convertFromHostBounds (*rectToCheck);

                        auto editorBounds = editor->getLocalArea (component.get(),
                                                                  Rectangle<int>::leftTopRightBottom (rectToCheck->left, rectToCheck->top,
                                                                                                      rectToCheck->right, rectToCheck->bottom).toFloat());

                        auto minW = (float) constrainer->getMinimumWidth();
                        auto maxW = (float) constrainer->getMaximumWidth();
                        auto minH = (float) constrainer->getMinimumHeight();
                        auto maxH = (float) constrainer->getMaximumHeight();

                        auto width  = jlimit (minW, maxW, editorBounds.getWidth());
                        auto height = jlimit (minH, maxH, editorBounds.getHeight());

                        auto aspectRatio = (float) constrainer->getFixedAspectRatio();

                        if (aspectRatio != 0.0)
                        {
                            bool adjustWidth = (width / height > aspectRatio);

                            if (getHostType().type == PluginHostType::SteinbergCubase9)
                            {
                                auto currentEditorBounds = editor->getBounds().toFloat();

                                if (currentEditorBounds.getWidth() == width && currentEditorBounds.getHeight() != height)
                                    adjustWidth = true;
                                else if (currentEditorBounds.getHeight() == height && currentEditorBounds.getWidth() != width)
                                    adjustWidth = false;
                            }

                            if (adjustWidth)
                            {
                                width = height * aspectRatio;

                                if (width > maxW || width < minW)
                                {
                                    width = jlimit (minW, maxW, width);
                                    height = width / aspectRatio;
                                }
                            }
                            else
                            {
                                height = width / aspectRatio;

                                if (height > maxH || height < minH)
                                {
                                    height = jlimit (minH, maxH, height);
                                    width = height * aspectRatio;
                                }
                            }
                        }

                        auto constrainedRect = component->getLocalArea (editor, Rectangle<float> (width, height))
                                                  .getSmallestIntegerContainer();

                        rectToCheck->right  = rectToCheck->left + roundToInt (constrainedRect.getWidth());
                        rectToCheck->bottom = rectToCheck->top  + roundToInt (constrainedRect.getHeight());

                        *rectToCheck = convertToHostBounds (*rectToCheck);
                    }
                }

                return kResultTrue;
            }

            jassertfalse;
            return kResultFalse;
        }

        tresult PLUGIN_API setContentScaleFactor (Steinberg::IPlugViewContentScaleSupport::ScaleFactor factor) override
        {
           #if ! JUCE_MAC
            if (! approximatelyEqual ((float) factor, editorScaleFactor))
            {
               #if JUCE_WINDOWS && JUCE_WIN_PER_MONITOR_DPI_AWARE
                // Cubase 10 only sends integer scale factors, so correct this for fractional scales
                if (getHostType().type == PluginHostType::SteinbergCubase10)
                {
                    auto hostWindowScale = (Steinberg::IPlugViewContentScaleSupport::ScaleFactor) getScaleFactorForWindow ((HWND) systemWindow);

                    if (hostWindowScale > 0.0 && ! approximatelyEqual (factor, hostWindowScale))
                        factor = hostWindowScale;
                }
               #endif

                editorScaleFactor = (float) factor;

                if (owner != nullptr)
                    owner->lastScaleFactorReceived = editorScaleFactor;

                if (component != nullptr)
                    component->setEditorScaleFactor (editorScaleFactor);
            }

            return kResultTrue;
           #else
            ignoreUnused (factor);
            return kResultFalse;
           #endif
        }

    private:
        void timerCallback() override
        {
            stopTimer();

            ViewRect viewRect;
            getSize (&viewRect);
            onSize (&viewRect);
        }

        static ViewRect convertToHostBounds (ViewRect pluginRect)
        {
            auto desktopScale = Desktop::getInstance().getGlobalScaleFactor();

            if (approximatelyEqual (desktopScale, 1.0f))
                return pluginRect;

            return { roundToInt ((float) pluginRect.left   * desktopScale),
                     roundToInt ((float) pluginRect.top    * desktopScale),
                     roundToInt ((float) pluginRect.right  * desktopScale),
                     roundToInt ((float) pluginRect.bottom * desktopScale) };
        }

        static ViewRect convertFromHostBounds (ViewRect hostRect)
        {
            auto desktopScale = Desktop::getInstance().getGlobalScaleFactor();

            if (approximatelyEqual (desktopScale, 1.0f))
                return hostRect;

            return { roundToInt ((float) hostRect.left   / desktopScale),
                     roundToInt ((float) hostRect.top    / desktopScale),
                     roundToInt ((float) hostRect.right  / desktopScale),
                     roundToInt ((float) hostRect.bottom / desktopScale) };
        }

        //==============================================================================
        struct ContentWrapperComponent  : public Component
                                       #if JUCE_WINDOWS && JUCE_WIN_PER_MONITOR_DPI_AWARE
                                        , public Timer
                                       #endif
        {
            ContentWrapperComponent (JuceVST3Editor& editor)  : owner (editor)
            {
                setOpaque (true);
                setBroughtToFrontOnMouseClick (true);

                ignoreUnused (fakeMouseGenerator);
            }

            ~ContentWrapperComponent() override
            {
                if (pluginEditor != nullptr)
                {
                    PopupMenu::dismissAllActiveMenus();
                    pluginEditor->processor.editorBeingDeleted (pluginEditor.get());
                }
            }

            void createEditor (AudioProcessor& plugin)
            {
                pluginEditor.reset (plugin.createEditorIfNeeded());

                if (pluginEditor != nullptr)
                {
                    addAndMakeVisible (pluginEditor.get());
                    pluginEditor->setTopLeftPosition (0, 0);

                    lastBounds = getSizeToContainChild();

                    {
                        const ScopedValueSetter<bool> resizingParentSetter (resizingParent, true);
                        setBounds (lastBounds);
                    }

                    resizeHostWindow();
                }
                else
                {
                    // if hasEditor() returns true then createEditorIfNeeded has to return a valid editor
                    jassertfalse;
                }
            }

            void paint (Graphics& g) override
            {
                g.fillAll (Colours::black);
            }

            juce::Rectangle<int> getSizeToContainChild()
            {
                if (pluginEditor != nullptr)
                    return getLocalArea (pluginEditor.get(), pluginEditor->getLocalBounds());

                return {};
            }

            void childBoundsChanged (Component*) override
            {
                if (resizingChild)
                    return;

                auto newBounds = getSizeToContainChild();

                if (newBounds != lastBounds)
                {
                    resizeHostWindow();

                   #if JUCE_LINUX || JUCE_BSD
                    if (getHostType().isBitwigStudio())
                        repaint();
                   #endif

                    lastBounds = newBounds;
                }
            }

            void resized() override
            {
                if (pluginEditor != nullptr)
                {
                    if (! resizingParent)
                    {
                        auto newBounds = getLocalBounds();

                        {
                            const ScopedValueSetter<bool> resizingChildSetter (resizingChild, true);
                            pluginEditor->setBounds (pluginEditor->getLocalArea (this, newBounds).withPosition (0, 0));
                        }

                        lastBounds = newBounds;
                    }
                }
            }

            void parentSizeChanged() override
            {
                if (pluginEditor != nullptr)
                {
                    resizeHostWindow();
                    pluginEditor->repaint();
                }
            }

            void resizeHostWindow()
            {
                if (pluginEditor != nullptr)
                {
                    if (owner.plugFrame != nullptr)
                    {
                        auto editorBounds = getSizeToContainChild();
                        auto newSize = convertToHostBounds ({ 0, 0, editorBounds.getWidth(), editorBounds.getHeight() });

                        {
                            const ScopedValueSetter<bool> resizingParentSetter (resizingParent, true);
                            owner.plugFrame->resizeView (&owner, &newSize);
                        }

                        auto host = getHostType();

                       #if JUCE_MAC
                        if (host.isWavelab() || host.isReaper())
                       #else
                        if (host.isWavelab() || host.isAbletonLive() || host.isBitwigStudio())
                       #endif
                            setBounds (editorBounds.withPosition (0, 0));
                    }
                }
            }

            void setEditorScaleFactor (float scale)
            {
                if (pluginEditor != nullptr)
                {
                    auto prevEditorBounds = pluginEditor->getLocalArea (this, lastBounds);

                    {
                        const ScopedValueSetter<bool> resizingChildSetter (resizingChild, true);

                        pluginEditor->setScaleFactor (scale);
                        pluginEditor->setBounds (prevEditorBounds.withPosition (0, 0));
                    }

                    lastBounds = getSizeToContainChild();

                    resizeHostWindow();
                    repaint();
                }
            }

           #if JUCE_WINDOWS && JUCE_WIN_PER_MONITOR_DPI_AWARE
            void checkHostWindowScaleFactor()
            {
                auto hostWindowScale = (float) getScaleFactorForWindow ((HWND) owner.systemWindow);

                if (hostWindowScale > 0.0 && ! approximatelyEqual (hostWindowScale, owner.editorScaleFactor))
                    owner.setContentScaleFactor (hostWindowScale);
            }

            void timerCallback() override
            {
                checkHostWindowScaleFactor();
            }
           #endif

            std::unique_ptr<AudioProcessorEditor> pluginEditor;

        private:
            JuceVST3Editor& owner;
            FakeMouseMoveGenerator fakeMouseGenerator;
            Rectangle<int> lastBounds;
            bool resizingChild = false, resizingParent = false;

            JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ContentWrapperComponent)
        };

        void createContentWrapperComponentIfNeeded()
        {
            if (component == nullptr)
            {
               #if JUCE_LINUX || JUCE_BSD
                const MessageManagerLock mmLock;
               #endif

                component.reset (new ContentWrapperComponent (*this));
                component->createEditor (pluginInstance);
            }
        }

        //==============================================================================
        ScopedJuceInitialiser_GUI libraryInitialiser;

       #if JUCE_LINUX || JUCE_BSD
        SharedResourcePointer<MessageThread> messageThread;
        SharedResourcePointer<EventHandler> eventHandler;
       #endif

        VSTComSmartPtr<JuceVST3EditController> owner;
        AudioProcessor& pluginInstance;

       #if JUCE_LINUX || JUCE_BSD
        struct MessageManagerLockedDeleter
        {
            template <typename ObjectType>
            void operator() (ObjectType* object) const noexcept
            {
                const MessageManagerLock mmLock;
                delete object;
            }
        };

        std::unique_ptr<ContentWrapperComponent, MessageManagerLockedDeleter> component;
       #else
        std::unique_ptr<ContentWrapperComponent> component;
       #endif

        friend struct ContentWrapperComponent;

       #if JUCE_MAC
        void* macHostWindow = nullptr;
        bool isNSView = false;

        // On macOS Cubase 10 resizes the host window after calling onSize() resulting in the peer
        // bounds being a step behind the plug-in. Calling updateBounds() asynchronously seems to fix things...
        struct Cubase10WindowResizeWorkaround  : public AsyncUpdater
        {
            Cubase10WindowResizeWorkaround (JuceVST3Editor& o)  : owner (o) {}

            void handleAsyncUpdate() override
            {
                if (owner.component != nullptr)
                    if (auto* peer = owner.component->getPeer())
                        peer->updateBounds();
            }

            JuceVST3Editor& owner;
        };

        std::unique_ptr<Cubase10WindowResizeWorkaround> cubase10Workaround;
       #else
        float editorScaleFactor = 1.0f;

        #if JUCE_WINDOWS
         WindowsHooks hooks;
        #endif

       #endif

        //==============================================================================
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JuceVST3Editor)
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JuceVST3EditController)
};

namespace
{
    template <typename FloatType> struct AudioBusPointerHelper {};
    template <> struct AudioBusPointerHelper<float>  { static float**  impl (Vst::AudioBusBuffers& data) noexcept { return data.channelBuffers32; } };
    template <> struct AudioBusPointerHelper<double> { static double** impl (Vst::AudioBusBuffers& data) noexcept { return data.channelBuffers64; } };

    template <typename FloatType> struct ChooseBufferHelper {};
    template <> struct ChooseBufferHelper<float>  { static AudioBuffer<float>&  impl (AudioBuffer<float>& f, AudioBuffer<double>&  ) noexcept { return f; } };
    template <> struct ChooseBufferHelper<double> { static AudioBuffer<double>& impl (AudioBuffer<float>&  , AudioBuffer<double>& d) noexcept { return d; } };
}


//==============================================================================
class JuceVST3Component : public Vst::IComponent,
                          public Vst::IAudioProcessor,
                          public Vst::IUnitInfo,
                          public Vst::IConnectionPoint,
                          public AudioPlayHead
{
public:
    JuceVST3Component (Vst::IHostApplication* h)
      : pluginInstance (createPluginFilterOfType (AudioProcessor::wrapperType_VST3)),
        host (h)
    {
        inParameterChangedCallback = false;

       #ifdef JucePlugin_PreferredChannelConfigurations
        short configs[][2] = { JucePlugin_PreferredChannelConfigurations };
        const int numConfigs = sizeof (configs) / sizeof (short[2]);

        ignoreUnused (numConfigs);
        jassert (numConfigs > 0 && (configs[0][0] > 0 || configs[0][1] > 0));

        pluginInstance->setPlayConfigDetails (configs[0][0], configs[0][1], 44100.0, 1024);
       #endif

        // VST-3 requires your default layout to be non-discrete!
        // For example, your default layout must be mono, stereo, quadrophonic
        // and not AudioChannelSet::discreteChannels (2) etc.
        jassert (checkBusFormatsAreNotDiscrete());

        comPluginInstance = VSTComSmartPtr<JuceAudioProcessor> { new JuceAudioProcessor (pluginInstance) };

        zerostruct (processContext);

        processSetup.maxSamplesPerBlock = 1024;
        processSetup.processMode = Vst::kRealtime;
        processSetup.sampleRate = 44100.0;
        processSetup.symbolicSampleSize = Vst::kSample32;

        pluginInstance->setPlayHead (this);
    }

    ~JuceVST3Component() override
    {
        if (juceVST3EditController != nullptr)
            juceVST3EditController->vst3IsPlaying = false;

        if (pluginInstance != nullptr)
            if (pluginInstance->getPlayHead() == this)
                pluginInstance->setPlayHead (nullptr);
    }

    //==============================================================================
    AudioProcessor& getPluginInstance() const noexcept { return *pluginInstance; }

    //==============================================================================
    static const FUID iid;

    JUCE_DECLARE_VST3_COM_REF_METHODS

    tresult PLUGIN_API queryInterface (const TUID targetIID, void** obj) override
    {
        TEST_FOR_AND_RETURN_IF_VALID (targetIID, IPluginBase)
        TEST_FOR_AND_RETURN_IF_VALID (targetIID, JuceVST3Component)
        TEST_FOR_AND_RETURN_IF_VALID (targetIID, Vst::IComponent)
        TEST_FOR_AND_RETURN_IF_VALID (targetIID, Vst::IAudioProcessor)
        TEST_FOR_AND_RETURN_IF_VALID (targetIID, Vst::IUnitInfo)
        TEST_FOR_AND_RETURN_IF_VALID (targetIID, Vst::IConnectionPoint)
        TEST_FOR_COMMON_BASE_AND_RETURN_IF_VALID (targetIID, FUnknown, Vst::IComponent)

        if (doUIDsMatch (targetIID, JuceAudioProcessor::iid))
        {
            comPluginInstance->addRef();
            *obj = comPluginInstance;
            return kResultOk;
        }

        *obj = nullptr;
        return kNoInterface;
    }

    //==============================================================================
    tresult PLUGIN_API initialize (FUnknown* hostContext) override
    {
        if (host != hostContext)
            host.loadFrom (hostContext);

        processContext.sampleRate = processSetup.sampleRate;
        preparePlugin (processSetup.sampleRate, (int) processSetup.maxSamplesPerBlock);

        return kResultTrue;
    }

    tresult PLUGIN_API terminate() override
    {
        getPluginInstance().releaseResources();
        return kResultTrue;
    }

    //==============================================================================
    tresult PLUGIN_API connect (IConnectionPoint* other) override
    {
        if (other != nullptr && juceVST3EditController == nullptr)
            juceVST3EditController.loadFrom (other);

        return kResultTrue;
    }

    tresult PLUGIN_API disconnect (IConnectionPoint*) override
    {
        if (juceVST3EditController != nullptr)
            juceVST3EditController->vst3IsPlaying = false;

        juceVST3EditController = {};
        return kResultTrue;
    }

    tresult PLUGIN_API notify (Vst::IMessage* message) override
    {
        if (message != nullptr && juceVST3EditController == nullptr)
        {
            Steinberg::int64 value = 0;

            if (message->getAttributes()->getInt ("JuceVST3EditController", value) == kResultTrue)
            {
                juceVST3EditController = VSTComSmartPtr<JuceVST3EditController> { (JuceVST3EditController*) (pointer_sized_int) value };

                if (juceVST3EditController != nullptr)
                    juceVST3EditController->setAudioProcessor (comPluginInstance);
                else
                    jassertfalse;
            }
        }

        return kResultTrue;
    }

    tresult PLUGIN_API getControllerClassId (TUID classID) override
    {
        memcpy (classID, JuceVST3EditController::iid, sizeof (TUID));
        return kResultTrue;
    }

    //==============================================================================
    tresult PLUGIN_API setActive (TBool state) override
    {
        if (! state)
        {
            getPluginInstance().releaseResources();

            deallocateChannelListAndBuffers (channelListFloat,  emptyBufferFloat);
            deallocateChannelListAndBuffers (channelListDouble, emptyBufferDouble);
        }
        else
        {
            auto sampleRate = getPluginInstance().getSampleRate();
            auto bufferSize = getPluginInstance().getBlockSize();

            sampleRate = processSetup.sampleRate > 0.0
                            ? processSetup.sampleRate
                            : sampleRate;

            bufferSize = processSetup.maxSamplesPerBlock > 0
                            ? (int) processSetup.maxSamplesPerBlock
                            : bufferSize;

            allocateChannelListAndBuffers (channelListFloat,  emptyBufferFloat);
            allocateChannelListAndBuffers (channelListDouble, emptyBufferDouble);

            preparePlugin (sampleRate, bufferSize);
        }

        return kResultOk;
    }

    tresult PLUGIN_API setIoMode (Vst::IoMode) override                                 { return kNotImplemented; }
    tresult PLUGIN_API getRoutingInfo (Vst::RoutingInfo&, Vst::RoutingInfo&) override   { return kNotImplemented; }

    //==============================================================================
    bool isBypassed()
    {
        if (auto* bypassParam = comPluginInstance->getBypassParameter())
            return (bypassParam->getValue() != 0.0f);

        return false;
    }

    void setBypassed (bool shouldBeBypassed)
    {
        if (auto* bypassParam = comPluginInstance->getBypassParameter())
        {
            auto floatValue = (shouldBeBypassed ? 1.0f : 0.0f);
            bypassParam->setValue (floatValue);

            ScopedThreadLocalBooleanSetter scope { inParameterChangedCallback };
            bypassParam->sendValueChangedMessageToListeners (floatValue);
        }
    }

    //==============================================================================
    void writeJucePrivateStateInformation (MemoryOutputStream& out)
    {
        if (pluginInstance->getBypassParameter() == nullptr)
        {
            ValueTree privateData (kJucePrivateDataIdentifier);

            // for now we only store the bypass value
            privateData.setProperty ("Bypass", var (isBypassed()), nullptr);
            privateData.writeToStream (out);
        }
    }

    void setJucePrivateStateInformation (const void* data, int sizeInBytes)
    {
        if (pluginInstance->getBypassParameter() == nullptr)
        {
            if (comPluginInstance->getBypassParameter() != nullptr)
            {
                auto privateData = ValueTree::readFromData (data, static_cast<size_t> (sizeInBytes));
                setBypassed (static_cast<bool> (privateData.getProperty ("Bypass", var (false))));
            }
        }
    }

    void getStateInformation (MemoryBlock& destData)
    {
        pluginInstance->getStateInformation (destData);

        // With bypass support, JUCE now needs to store private state data.
        // Put this at the end of the plug-in state and add a few null characters
        // so that plug-ins built with older versions of JUCE will hopefully ignore
        // this data. Additionally, we need to add some sort of magic identifier
        // at the very end of the private data so that JUCE has some sort of
        // way to figure out if the data was stored with a newer JUCE version.
        MemoryOutputStream extraData;

        extraData.writeInt64 (0);
        writeJucePrivateStateInformation (extraData);
        auto privateDataSize = (int64) (extraData.getDataSize() - sizeof (int64));
        extraData.writeInt64 (privateDataSize);
        extraData << kJucePrivateDataIdentifier;

        // write magic string
        destData.append (extraData.getData(), extraData.getDataSize());
    }

    void setStateInformation (const void* data, int sizeAsInt)
    {
        auto size = (uint64) sizeAsInt;

        // Check if this data was written with a newer JUCE version
        // and if it has the JUCE private data magic code at the end
        auto jucePrivDataIdentifierSize = std::strlen (kJucePrivateDataIdentifier);

        if ((size_t) size >= jucePrivDataIdentifierSize + sizeof (int64))
        {
            auto buffer = static_cast<const char*> (data);

            String magic (CharPointer_UTF8 (buffer + size - jucePrivDataIdentifierSize),
                          CharPointer_UTF8 (buffer + size));

            if (magic == kJucePrivateDataIdentifier)
            {
                // found a JUCE private data section
                uint64 privateDataSize;

                std::memcpy (&privateDataSize,
                             buffer + ((size_t) size - jucePrivDataIdentifierSize - sizeof (uint64)),
                             sizeof (uint64));

                privateDataSize = ByteOrder::swapIfBigEndian (privateDataSize);
                size -= privateDataSize + jucePrivDataIdentifierSize + sizeof (uint64);

                if (privateDataSize > 0)
                    setJucePrivateStateInformation (buffer + size, static_cast<int> (privateDataSize));

                size -= sizeof (uint64);
            }
        }

        if (size > 0)
            pluginInstance->setStateInformation (data, static_cast<int> (size));
    }

    //==============================================================================
   #if JUCE_VST3_CAN_REPLACE_VST2
    bool loadVST2VstWBlock (const char* data, int size)
    {
        jassert (ByteOrder::bigEndianInt ("VstW") == htonl ((uint32) readUnaligned<int32> (data)));
        jassert (1 == htonl ((uint32) readUnaligned<int32> (data + 8))); // version should be 1 according to Steinberg's docs

        auto headerLen = (int) htonl ((uint32) readUnaligned<int32> (data + 4)) + 8;
        return loadVST2CcnKBlock (data + headerLen, size - headerLen);
    }

    bool loadVST2CcnKBlock (const char* data, int size)
    {
        auto* bank = reinterpret_cast<const Vst2::fxBank*> (data);

        jassert (ByteOrder::bigEndianInt ("CcnK") == htonl ((uint32) bank->chunkMagic));
        jassert (ByteOrder::bigEndianInt ("FBCh") == htonl ((uint32) bank->fxMagic));
        jassert (htonl ((uint32) bank->version) == 1 || htonl ((uint32) bank->version) == 2);
        jassert (JucePlugin_VSTUniqueID == htonl ((uint32) bank->fxID));

        setStateInformation (bank->content.data.chunk,
                             jmin ((int) (size - (bank->content.data.chunk - data)),
                                   (int) htonl ((uint32) bank->content.data.size)));
        return true;
    }

    bool loadVST3PresetFile (const char* data, int size)
    {
        if (size < 48)
            return false;

        // At offset 4 there's a little-endian version number which seems to typically be 1
        // At offset 8 there's 32 bytes the SDK calls "ASCII-encoded class id"
        auto chunkListOffset = (int) ByteOrder::littleEndianInt (data + 40);
        jassert (memcmp (data + chunkListOffset, "List", 4) == 0);
        auto entryCount = (int) ByteOrder::littleEndianInt (data + chunkListOffset + 4);
        jassert (entryCount > 0);

        for (int i = 0; i < entryCount; ++i)
        {
            auto entryOffset = chunkListOffset + 8 + 20 * i;

            if (entryOffset + 20 > size)
                return false;

            if (memcmp (data + entryOffset, "Comp", 4) == 0)
            {
                // "Comp" entries seem to contain the data.
                auto chunkOffset = ByteOrder::littleEndianInt64 (data + entryOffset + 4);
                auto chunkSize   = ByteOrder::littleEndianInt64 (data + entryOffset + 12);

                if (static_cast<uint64> (chunkOffset + chunkSize) > static_cast<uint64> (size))
                {
                    jassertfalse;
                    return false;
                }

                loadVST2VstWBlock (data + chunkOffset, (int) chunkSize);
            }
        }

        return true;
    }

    bool loadVST2CompatibleState (const char* data, int size)
    {
        if (size < 4)
            return false;

        auto header = htonl ((uint32) readUnaligned<int32> (data));

        if (header == ByteOrder::bigEndianInt ("VstW"))
            return loadVST2VstWBlock (data, size);

        if (header == ByteOrder::bigEndianInt ("CcnK"))
            return loadVST2CcnKBlock (data, size);

        if (memcmp (data, "VST3", 4) == 0)
        {
            // In Cubase 5, when loading VST3 .vstpreset files,
            // we get the whole content of the files to load.
            // In Cubase 7 we get just the contents within and
            // we go directly to the loadVST2VstW codepath instead.
            return loadVST3PresetFile (data, size);
        }

        return false;
    }
   #endif

    void loadStateData (const void* data, int size)
    {
       #if JUCE_VST3_CAN_REPLACE_VST2
        if (loadVST2CompatibleState ((const char*) data, size))
            return;
       #endif
        setStateInformation (data, size);
    }

    bool readFromMemoryStream (IBStream* state)
    {
        FUnknownPtr<ISizeableStream> s (state);
        Steinberg::int64 size = 0;

        if (s != nullptr
             && s->getStreamSize (size) == kResultOk
             && size > 0
             && size < 1024 * 1024 * 100) // (some hosts seem to return junk for the size)
        {
            MemoryBlock block (static_cast<size_t> (size));

            // turns out that Cubase 9 might give you the incorrect stream size :-(
            Steinberg::int32 bytesRead = 1;
            int len;

            for (len = 0; bytesRead > 0 && len < static_cast<int> (block.getSize()); len += bytesRead)
                if (state->read (block.getData(), static_cast<int32> (block.getSize()), &bytesRead) != kResultOk)
                    break;

            if (len == 0)
                return false;

            block.setSize (static_cast<size_t> (len));

            // Adobe Audition CS6 hack to avoid trying to use corrupted streams:
            if (getHostType().isAdobeAudition())
                if (block.getSize() >= 5 && memcmp (block.getData(), "VC2!E", 5) == 0)
                    return false;

            loadStateData (block.getData(), (int) block.getSize());
            return true;
        }

        return false;
    }

    bool readFromUnknownStream (IBStream* state)
    {
        MemoryOutputStream allData;

        {
            const size_t bytesPerBlock = 4096;
            HeapBlock<char> buffer (bytesPerBlock);

            for (;;)
            {
                Steinberg::int32 bytesRead = 0;
                auto status = state->read (buffer, (Steinberg::int32) bytesPerBlock, &bytesRead);

                if (bytesRead <= 0 || (status != kResultTrue && ! getHostType().isWavelab()))
                    break;

                allData.write (buffer, static_cast<size_t> (bytesRead));
            }
        }

        const size_t dataSize = allData.getDataSize();

        if (dataSize <= 0 || dataSize >= 0x7fffffff)
            return false;

        loadStateData (allData.getData(), (int) dataSize);
        return true;
    }

    tresult PLUGIN_API setState (IBStream* state) override
    {
        if (state == nullptr)
            return kInvalidArgument;

        FUnknownPtr<IBStream> stateRefHolder (state); // just in case the caller hasn't properly ref-counted the stream object

        if (state->seek (0, IBStream::kIBSeekSet, nullptr) == kResultTrue)
        {
            if (! getHostType().isFruityLoops() && readFromMemoryStream (state))
                return kResultTrue;

            if (readFromUnknownStream (state))
                return kResultTrue;
        }

        return kResultFalse;
    }

   #if JUCE_VST3_CAN_REPLACE_VST2
    static tresult writeVST2Header (IBStream* state, bool bypassed)
    {
        auto writeVST2IntToState = [state] (uint32 n)
        {
            auto t = (int32) htonl (n);
            return state->write (&t, 4);
        };

        auto status = writeVST2IntToState (ByteOrder::bigEndianInt ("VstW"));

        if (status == kResultOk) status = writeVST2IntToState (8); // header size
        if (status == kResultOk) status = writeVST2IntToState (1); // version
        if (status == kResultOk) status = writeVST2IntToState (bypassed ? 1 : 0); // bypass

        return status;
    }
   #endif

    tresult PLUGIN_API getState (IBStream* state) override
    {
       if (state == nullptr)
           return kInvalidArgument;

        MemoryBlock mem;
        getStateInformation (mem);

      #if JUCE_VST3_CAN_REPLACE_VST2
        tresult status = writeVST2Header (state, isBypassed());

        if (status != kResultOk)
            return status;

        const int bankBlockSize = 160;
        Vst2::fxBank bank;

        zerostruct (bank);
        bank.chunkMagic         = (int32) htonl (ByteOrder::bigEndianInt ("CcnK"));
        bank.byteSize           = (int32) htonl (bankBlockSize - 8 + (unsigned int) mem.getSize());
        bank.fxMagic            = (int32) htonl (ByteOrder::bigEndianInt ("FBCh"));
        bank.version            = (int32) htonl (2);
        bank.fxID               = (int32) htonl (JucePlugin_VSTUniqueID);
        bank.fxVersion          = (int32) htonl (JucePlugin_VersionCode);
        bank.content.data.size  = (int32) htonl ((unsigned int) mem.getSize());

        status = state->write (&bank, bankBlockSize);

        if (status != kResultOk)
            return status;
       #endif

        return state->write (mem.getData(), (Steinberg::int32) mem.getSize());
    }

    //==============================================================================
    Steinberg::int32 PLUGIN_API getUnitCount() override                                                                         { return comPluginInstance->getUnitCount(); }
    tresult PLUGIN_API getUnitInfo (Steinberg::int32 unitIndex, Vst::UnitInfo& info) override                                   { return comPluginInstance->getUnitInfo (unitIndex, info); }
    Steinberg::int32 PLUGIN_API getProgramListCount() override                                                                  { return comPluginInstance->getProgramListCount(); }
    tresult PLUGIN_API getProgramListInfo (Steinberg::int32 listIndex, Vst::ProgramListInfo& info) override                     { return comPluginInstance->getProgramListInfo (listIndex, info); }
    tresult PLUGIN_API getProgramName (Vst::ProgramListID listId, Steinberg::int32 programIndex, Vst::String128 name) override  { return comPluginInstance->getProgramName (listId, programIndex, name); }
    tresult PLUGIN_API getProgramInfo (Vst::ProgramListID listId, Steinberg::int32 programIndex,
                                       Vst::CString attributeId, Vst::String128 attributeValue) override                        { return comPluginInstance->getProgramInfo (listId, programIndex, attributeId, attributeValue); }
    tresult PLUGIN_API hasProgramPitchNames (Vst::ProgramListID listId, Steinberg::int32 programIndex) override                 { return comPluginInstance->hasProgramPitchNames (listId, programIndex); }
    tresult PLUGIN_API getProgramPitchName (Vst::ProgramListID listId, Steinberg::int32 programIndex,
                                            Steinberg::int16 midiPitch, Vst::String128 name) override                           { return comPluginInstance->getProgramPitchName (listId, programIndex, midiPitch, name); }
    tresult PLUGIN_API selectUnit (Vst::UnitID unitId) override                                                                 { return comPluginInstance->selectUnit (unitId); }
    tresult PLUGIN_API setUnitProgramData (Steinberg::int32 listOrUnitId, Steinberg::int32 programIndex,
                                           Steinberg::IBStream* data) override                                                  { return comPluginInstance->setUnitProgramData (listOrUnitId, programIndex, data); }
    Vst::UnitID PLUGIN_API getSelectedUnit() override                                                                           { return comPluginInstance->getSelectedUnit(); }
    tresult PLUGIN_API getUnitByBus (Vst::MediaType type, Vst::BusDirection dir, Steinberg::int32 busIndex,
                                     Steinberg::int32 channel, Vst::UnitID& unitId) override                                    { return comPluginInstance->getUnitByBus (type, dir, busIndex, channel, unitId); }

    //==============================================================================
    bool getCurrentPosition (CurrentPositionInfo& info) override
    {
        info.timeInSamples              = jmax ((juce::int64) 0, processContext.projectTimeSamples);
        info.timeInSeconds              = static_cast<double> (info.timeInSamples) / processContext.sampleRate;
        info.bpm                        = jmax (1.0, processContext.tempo);
        info.timeSigNumerator           = jmax (1, (int) processContext.timeSigNumerator);
        info.timeSigDenominator         = jmax (1, (int) processContext.timeSigDenominator);
        info.ppqPositionOfLastBarStart  = processContext.barPositionMusic;
        info.ppqPosition                = processContext.projectTimeMusic;
        info.ppqLoopStart               = processContext.cycleStartMusic;
        info.ppqLoopEnd                 = processContext.cycleEndMusic;
        info.isRecording                = (processContext.state & Vst::ProcessContext::kRecording) != 0;
        info.isPlaying                  = (processContext.state & Vst::ProcessContext::kPlaying) != 0;
        info.isLooping                  = (processContext.state & Vst::ProcessContext::kCycleActive) != 0;
        info.editOriginTime             = 0.0;
        info.frameRate                  = AudioPlayHead::fpsUnknown;

        if ((processContext.state & Vst::ProcessContext::kSmpteValid) != 0)
        {
            switch (processContext.frameRate.framesPerSecond)
            {
                case 24:
                {
                    if ((processContext.frameRate.flags & Vst::FrameRate::kPullDownRate) != 0)
                        info.frameRate = AudioPlayHead::fps23976;
                    else
                        info.frameRate = AudioPlayHead::fps24;
                }
                break;
                case 25: info.frameRate = AudioPlayHead::fps25; break;
                case 29: info.frameRate = AudioPlayHead::fps30drop; break;

                case 30:
                {
                    if ((processContext.frameRate.flags & Vst::FrameRate::kDropRate) != 0)
                        info.frameRate = AudioPlayHead::fps30drop;
                    else
                        info.frameRate = AudioPlayHead::fps30;
                }
                break;

                default: break;
            }
        }

        return true;
    }

    //==============================================================================
    int getNumAudioBuses (bool isInput) const
    {
        int busCount = pluginInstance->getBusCount (isInput);

      #ifdef JucePlugin_PreferredChannelConfigurations
        short configs[][2] = {JucePlugin_PreferredChannelConfigurations};
        const int numConfigs = sizeof (configs) / sizeof (short[2]);

        bool hasOnlyZeroChannels = true;

        for (int i = 0; i < numConfigs && hasOnlyZeroChannels == true; ++i)
            if (configs[i][isInput ? 0 : 1] != 0)
                hasOnlyZeroChannels = false;

        busCount = jmin (busCount, hasOnlyZeroChannels ? 0 : 1);
       #endif

        return busCount;
    }

    //==============================================================================
    Steinberg::int32 PLUGIN_API getBusCount (Vst::MediaType type, Vst::BusDirection dir) override
    {
        if (type == Vst::kAudio)
            return getNumAudioBuses (dir == Vst::kInput);

        if (type == Vst::kEvent)
        {
           #if JucePlugin_WantsMidiInput
            if (dir == Vst::kInput)
                return 1;
           #endif

           #if JucePlugin_ProducesMidiOutput
            if (dir == Vst::kOutput)
                return 1;
           #endif
        }

        return 0;
    }

    tresult PLUGIN_API getBusInfo (Vst::MediaType type, Vst::BusDirection dir,
                                   Steinberg::int32 index, Vst::BusInfo& info) override
    {
        if (type == Vst::kAudio)
        {
            if (index < 0 || index >= getNumAudioBuses (dir == Vst::kInput))
                return kResultFalse;

            if (auto* bus = pluginInstance->getBus (dir == Vst::kInput, index))
            {
                info.mediaType = Vst::kAudio;
                info.direction = dir;
                info.channelCount = bus->getLastEnabledLayout().size();
                toString128 (info.name, bus->getName());

               #if JucePlugin_IsSynth
                info.busType = (dir == Vst::kInput && index > 0 ? Vst::kAux : Vst::kMain);
               #else
                info.busType = (index == 0 ? Vst::kMain : Vst::kAux);
               #endif

                info.flags = (bus->isEnabledByDefault()) ? Vst::BusInfo::kDefaultActive : 0;
                return kResultTrue;
            }
        }

        if (type == Vst::kEvent)
        {
            info.flags = Vst::BusInfo::kDefaultActive;

           #if JucePlugin_WantsMidiInput
            if (dir == Vst::kInput && index == 0)
            {
                info.mediaType = Vst::kEvent;
                info.direction = dir;

               #ifdef JucePlugin_VSTNumMidiInputs
                info.channelCount = JucePlugin_VSTNumMidiInputs;
               #else
                info.channelCount = 16;
               #endif

                toString128 (info.name, TRANS("MIDI Input"));
                info.busType = Vst::kMain;
                return kResultTrue;
            }
           #endif

           #if JucePlugin_ProducesMidiOutput
            if (dir == Vst::kOutput && index == 0)
            {
                info.mediaType = Vst::kEvent;
                info.direction = dir;

               #ifdef JucePlugin_VSTNumMidiOutputs
                info.channelCount = JucePlugin_VSTNumMidiOutputs;
               #else
                info.channelCount = 16;
               #endif

                toString128 (info.name, TRANS("MIDI Output"));
                info.busType = Vst::kMain;
                return kResultTrue;
            }
           #endif
        }

        zerostruct (info);
        return kResultFalse;
    }

    tresult PLUGIN_API activateBus (Vst::MediaType type, Vst::BusDirection dir, Steinberg::int32 index, TBool state) override
    {
        if (type == Vst::kEvent)
        {
           #if JucePlugin_WantsMidiInput
            if (index == 0 && dir == Vst::kInput)
            {
                isMidiInputBusEnabled = (state != 0);
                return kResultTrue;
            }
           #endif

           #if JucePlugin_ProducesMidiOutput
            if (index == 0 && dir == Vst::kOutput)
            {
                isMidiOutputBusEnabled = (state != 0);
                return kResultTrue;
            }
           #endif

            return kResultFalse;
        }

        if (type == Vst::kAudio)
        {
            if (index < 0 || index >= getNumAudioBuses (dir == Vst::kInput))
                return kResultFalse;

            if (auto* bus = pluginInstance->getBus (dir == Vst::kInput, index))
            {
               #ifdef JucePlugin_PreferredChannelConfigurations
                auto newLayout = pluginInstance->getBusesLayout();
                auto targetLayout = (state != 0 ? bus->getLastEnabledLayout() : AudioChannelSet::disabled());

                (dir == Vst::kInput ? newLayout.inputBuses : newLayout.outputBuses).getReference (index) = targetLayout;

                short configs[][2] = { JucePlugin_PreferredChannelConfigurations };
                auto compLayout = pluginInstance->getNextBestLayoutInLayoutList (newLayout, configs);

                if ((dir == Vst::kInput ? compLayout.inputBuses : compLayout.outputBuses).getReference (index) != targetLayout)
                    return kResultFalse;
               #endif

                return bus->enable (state != 0) ? kResultTrue : kResultFalse;
            }
        }

        return kResultFalse;
    }

    bool checkBusFormatsAreNotDiscrete()
    {
        auto numInputBuses  = pluginInstance->getBusCount (true);
        auto numOutputBuses = pluginInstance->getBusCount (false);

        for (int i = 0; i < numInputBuses; ++i)
        {
            auto layout = pluginInstance->getChannelLayoutOfBus (true,  i);

            if (layout.isDiscreteLayout() && ! layout.isDisabled())
                return false;
        }

        for (int i = 0; i < numOutputBuses; ++i)
        {
            auto layout = pluginInstance->getChannelLayoutOfBus (false,  i);

            if (layout.isDiscreteLayout() && ! layout.isDisabled())
                return false;
        }

        return true;
    }

    tresult PLUGIN_API setBusArrangements (Vst::SpeakerArrangement* inputs, Steinberg::int32 numIns,
                                           Vst::SpeakerArrangement* outputs, Steinberg::int32 numOuts) override
    {
        auto numInputBuses  = pluginInstance->getBusCount (true);
        auto numOutputBuses = pluginInstance->getBusCount (false);

        if (numIns > numInputBuses || numOuts > numOutputBuses)
            return false;

        auto requested = pluginInstance->getBusesLayout();

        for (int i = 0; i < numIns; ++i)
            requested.getChannelSet (true,  i) = getChannelSetForSpeakerArrangement (inputs[i]);

        for (int i = 0; i < numOuts; ++i)
            requested.getChannelSet (false, i) = getChannelSetForSpeakerArrangement (outputs[i]);

       #ifdef JucePlugin_PreferredChannelConfigurations
        short configs[][2] = { JucePlugin_PreferredChannelConfigurations };
        if (! AudioProcessor::containsLayout (requested, configs))
            return kResultFalse;
       #endif

        return pluginInstance->setBusesLayoutWithoutEnabling (requested) ? kResultTrue : kResultFalse;
    }

    tresult PLUGIN_API getBusArrangement (Vst::BusDirection dir, Steinberg::int32 index, Vst::SpeakerArrangement& arr) override
    {
        if (auto* bus = pluginInstance->getBus (dir == Vst::kInput, index))
        {
            arr = getVst3SpeakerArrangement (bus->getLastEnabledLayout());
            return kResultTrue;
        }

        return kResultFalse;
    }

    //==============================================================================
    tresult PLUGIN_API canProcessSampleSize (Steinberg::int32 symbolicSampleSize) override
    {
        return (symbolicSampleSize == Vst::kSample32
                 || (getPluginInstance().supportsDoublePrecisionProcessing()
                       && symbolicSampleSize == Vst::kSample64)) ? kResultTrue : kResultFalse;
    }

    Steinberg::uint32 PLUGIN_API getLatencySamples() override
    {
        return (Steinberg::uint32) jmax (0, getPluginInstance().getLatencySamples());
    }

    tresult PLUGIN_API setupProcessing (Vst::ProcessSetup& newSetup) override
    {
        ScopedInSetupProcessingSetter inSetupProcessingSetter (juceVST3EditController);

        if (canProcessSampleSize (newSetup.symbolicSampleSize) != kResultTrue)
            return kResultFalse;

        processSetup = newSetup;
        processContext.sampleRate = processSetup.sampleRate;

        getPluginInstance().setProcessingPrecision (newSetup.symbolicSampleSize == Vst::kSample64
                                                        ? AudioProcessor::doublePrecision
                                                        : AudioProcessor::singlePrecision);
        getPluginInstance().setNonRealtime (newSetup.processMode == Vst::kOffline);

        preparePlugin (processSetup.sampleRate, processSetup.maxSamplesPerBlock);

        return kResultTrue;
    }

    tresult PLUGIN_API setProcessing (TBool state) override
    {
        if (! state)
            getPluginInstance().reset();

        return kResultTrue;
    }

    Steinberg::uint32 PLUGIN_API getTailSamples() override
    {
        auto tailLengthSeconds = getPluginInstance().getTailLengthSeconds();

        if (tailLengthSeconds <= 0.0 || processSetup.sampleRate <= 0.0)
            return Vst::kNoTail;

        if (tailLengthSeconds == std::numeric_limits<double>::infinity())
            return Vst::kInfiniteTail;

        return (Steinberg::uint32) roundToIntAccurate (tailLengthSeconds * processSetup.sampleRate);
    }

    //==============================================================================
    void processParameterChanges (Vst::IParameterChanges& paramChanges)
    {
        jassert (pluginInstance != nullptr);

        auto numParamsChanged = paramChanges.getParameterCount();

        for (Steinberg::int32 i = 0; i < numParamsChanged; ++i)
        {
            if (auto* paramQueue = paramChanges.getParameterData (i))
            {
                auto numPoints = paramQueue->getPointCount();

                Steinberg::int32 offsetSamples = 0;
                double value = 0.0;

                if (paramQueue->getPoint (numPoints - 1, offsetSamples, value) == kResultTrue)
                {
                    auto vstParamID = paramQueue->getParameterId();

                   #if JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS
                    if (juceVST3EditController != nullptr && juceVST3EditController->isMidiControllerParamID (vstParamID))
                        addParameterChangeToMidiBuffer (offsetSamples, vstParamID, value);
                    else
                   #endif
                    {
                        auto floatValue = static_cast<float> (value);

                        if (auto* param = comPluginInstance->getParamForVSTParamID (vstParamID))
                        {
                            param->setValue (floatValue);

                            ScopedThreadLocalBooleanSetter scope { inParameterChangedCallback };
                            param->sendValueChangedMessageToListeners (floatValue);
                        }
                    }
                }
            }
        }
    }

    void addParameterChangeToMidiBuffer (const Steinberg::int32 offsetSamples, const Vst::ParamID id, const double value)
    {
        // If the parameter is mapped to a MIDI CC message then insert it into the midiBuffer.
        int channel, ctrlNumber;

        if (juceVST3EditController->getMidiControllerForParameter (id, channel, ctrlNumber))
        {
            if (ctrlNumber == Vst::kAfterTouch)
                midiBuffer.addEvent (MidiMessage::channelPressureChange (channel,
                                                                         jlimit (0, 127, (int) (value * 128.0))), offsetSamples);
            else if (ctrlNumber == Vst::kPitchBend)
                midiBuffer.addEvent (MidiMessage::pitchWheel (channel,
                                                              jlimit (0, 0x3fff, (int) (value * 0x4000))), offsetSamples);
            else
                midiBuffer.addEvent (MidiMessage::controllerEvent (channel,
                                                                   jlimit (0, 127, ctrlNumber),
                                                                   jlimit (0, 127, (int) (value * 128.0))), offsetSamples);
        }
    }

    tresult PLUGIN_API process (Vst::ProcessData& data) override
    {
        if (pluginInstance == nullptr)
            return kResultFalse;

        if ((processSetup.symbolicSampleSize == Vst::kSample64) != pluginInstance->isUsingDoublePrecision())
            return kResultFalse;

        if (data.processContext != nullptr)
        {
            processContext = *data.processContext;

            if (juceVST3EditController != nullptr)
                juceVST3EditController->vst3IsPlaying = (processContext.state & Vst::ProcessContext::kPlaying) != 0;
        }
        else
        {
            zerostruct (processContext);

            if (juceVST3EditController != nullptr)
                juceVST3EditController->vst3IsPlaying = false;
        }

        midiBuffer.clear();

        if (data.inputParameterChanges != nullptr)
            processParameterChanges (*data.inputParameterChanges);

       #if JucePlugin_WantsMidiInput
        if (isMidiInputBusEnabled && data.inputEvents != nullptr)
            MidiEventList::toMidiBuffer (midiBuffer, *data.inputEvents);
       #endif

        if (getHostType().isWavelab())
        {
            const int numInputChans  = (data.inputs  != nullptr && data.inputs[0].channelBuffers32 != nullptr)  ? (int) data.inputs[0].numChannels  : 0;
            const int numOutputChans = (data.outputs != nullptr && data.outputs[0].channelBuffers32 != nullptr) ? (int) data.outputs[0].numChannels : 0;

            if ((pluginInstance->getTotalNumInputChannels() + pluginInstance->getTotalNumOutputChannels()) > 0
                 && (numInputChans + numOutputChans) == 0)
                return kResultFalse;
        }

        if      (processSetup.symbolicSampleSize == Vst::kSample32) processAudio<float>  (data, channelListFloat);
        else if (processSetup.symbolicSampleSize == Vst::kSample64) processAudio<double> (data, channelListDouble);
        else jassertfalse;

       #if JucePlugin_ProducesMidiOutput
        if (isMidiOutputBusEnabled && data.outputEvents != nullptr)
            MidiEventList::pluginToHostEventList (*data.outputEvents, midiBuffer);
       #endif

        return kResultTrue;
    }

private:
    //==============================================================================
    struct ScopedInSetupProcessingSetter
    {
        ScopedInSetupProcessingSetter (JuceVST3EditController* c)
            : controller (c)
        {
            if (controller != nullptr)
                controller->inSetupProcessing = true;
        }

        ~ScopedInSetupProcessingSetter()
        {
            if (controller != nullptr)
                controller->inSetupProcessing = false;
        }

    private:
        JuceVST3EditController* controller = nullptr;
    };

    //==============================================================================
    template <typename FloatType>
    void processAudio (Vst::ProcessData& data, Array<FloatType*>& channelList)
    {
        int totalInputChans = 0, totalOutputChans = 0;
        bool tmpBufferNeedsClearing = false;

        auto plugInInputChannels  = pluginInstance->getTotalNumInputChannels();
        auto plugInOutputChannels = pluginInstance->getTotalNumOutputChannels();

        // Wavelab workaround: wave-lab lies on the number of inputs/outputs so re-count here
        const auto countValidChannels = [] (Vst::AudioBusBuffers* buffers, int32 num)
        {
            return int (std::distance (buffers, std::find_if (buffers, buffers + num, [] (Vst::AudioBusBuffers& buf)
            {
                return getPointerForAudioBus<FloatType> (buf) == nullptr && buf.numChannels > 0;
            })));
        };

        const auto vstInputs  = countValidChannels (data.inputs,  data.numInputs);
        const auto vstOutputs = countValidChannels (data.outputs, data.numOutputs);

        {
            auto n = jmax (vstOutputs, getNumAudioBuses (false));

            for (int bus = 0; bus < n && totalOutputChans < plugInOutputChannels; ++bus)
            {
                if (auto* busObject = pluginInstance->getBus (false, bus))
                    if (! busObject->isEnabled())
                        continue;

                if (bus < vstOutputs)
                {
                    if (auto** const busChannels = getPointerForAudioBus<FloatType> (data.outputs[bus]))
                    {
                        auto numChans = jmin ((int) data.outputs[bus].numChannels, plugInOutputChannels - totalOutputChans);

                        for (int i = 0; i < numChans; ++i)
                        {
                            if (auto dst = busChannels[i])
                            {
                                if (totalOutputChans >= plugInInputChannels)
                                    FloatVectorOperations::clear (dst, (int) data.numSamples);

                                channelList.set (totalOutputChans++, busChannels[i]);
                            }
                        }
                    }
                }
                else
                {
                    const int numChans = jmin (pluginInstance->getChannelCountOfBus (false, bus), plugInOutputChannels - totalOutputChans);

                    for (int i = 0; i < numChans; ++i)
                    {
                        if (auto* tmpBuffer = getTmpBufferForChannel<FloatType> (totalOutputChans, data.numSamples))\
                        {
                            tmpBufferNeedsClearing = true;
                            channelList.set (totalOutputChans++, tmpBuffer);
                        }
                        else
                            return;
                    }
                }
            }
        }

        {
            auto n = jmax (vstInputs, getNumAudioBuses (true));

            for (int bus = 0; bus < n && totalInputChans < plugInInputChannels; ++bus)
            {
                if (auto* busObject = pluginInstance->getBus (true, bus))
                    if (! busObject->isEnabled())
                        continue;

                if (bus < vstInputs)
                {
                    if (auto** const busChannels = getPointerForAudioBus<FloatType> (data.inputs[bus]))
                    {
                        const int numChans = jmin ((int) data.inputs[bus].numChannels, plugInInputChannels - totalInputChans);

                        for (int i = 0; i < numChans; ++i)
                        {
                            if (busChannels[i] != nullptr)
                            {
                                if (totalInputChans >= totalOutputChans)
                                    channelList.set (totalInputChans, busChannels[i]);
                                else
                                {
                                    auto* dst = channelList.getReference (totalInputChans);
                                    auto* src = busChannels[i];

                                    if (dst != src)
                                        FloatVectorOperations::copy (dst, src, (int) data.numSamples);
                                }
                            }

                            ++totalInputChans;
                        }
                    }
                }
                else
                {
                    auto numChans = jmin (pluginInstance->getChannelCountOfBus (true, bus), plugInInputChannels - totalInputChans);

                    for (int i = 0; i < numChans; ++i)
                    {
                        if (auto* tmpBuffer = getTmpBufferForChannel<FloatType> (totalInputChans, data.numSamples))
                        {
                            tmpBufferNeedsClearing = true;
                            channelList.set (totalInputChans++, tmpBuffer);
                        }
                        else
                            return;
                    }
                }
            }
        }

        if (tmpBufferNeedsClearing)
            ChooseBufferHelper<FloatType>::impl (emptyBufferFloat, emptyBufferDouble).clear();

        AudioBuffer<FloatType> buffer;

        if (int totalChans = jmax (totalOutputChans, totalInputChans))
            buffer.setDataToReferTo (channelList.getRawDataPointer(), totalChans, (int) data.numSamples);

        {
            const ScopedLock sl (pluginInstance->getCallbackLock());

            pluginInstance->setNonRealtime (data.processMode == Vst::kOffline);

           #if JUCE_DEBUG && ! JucePlugin_ProducesMidiOutput
            const int numMidiEventsComingIn = midiBuffer.getNumEvents();
           #endif

            if (pluginInstance->isSuspended())
            {
                buffer.clear();
            }
            else
            {
                if (totalInputChans == pluginInstance->getTotalNumInputChannels()
                 && totalOutputChans == pluginInstance->getTotalNumOutputChannels())
                {
                    if (isBypassed())
                        pluginInstance->processBlockBypassed (buffer, midiBuffer);
                    else
                        pluginInstance->processBlock (buffer, midiBuffer);
                }
            }

           #if JUCE_DEBUG && (! JucePlugin_ProducesMidiOutput)
            /*  This assertion is caused when you've added some events to the
                midiMessages array in your processBlock() method, which usually means
                that you're trying to send them somewhere. But in this case they're
                getting thrown away.

                If your plugin does want to send MIDI messages, you'll need to set
                the JucePlugin_ProducesMidiOutput macro to 1 in your
                JucePluginCharacteristics.h file.

                If you don't want to produce any MIDI output, then you should clear the
                midiMessages array at the end of your processBlock() method, to
                indicate that you don't want any of the events to be passed through
                to the output.
            */
            jassert (midiBuffer.getNumEvents() <= numMidiEventsComingIn);
           #endif
        }

        if (auto* changes = data.outputParameterChanges)
        {
            comPluginInstance->forAllChangedParameters ([&] (Vst::ParamID paramID, float value)
            {
                Steinberg::int32 queueIndex = 0;

                if (auto* queue = changes->addParameterData (paramID, queueIndex))
                {
                    Steinberg::int32 pointIndex = 0;
                    queue->addPoint (0, value, pointIndex);
                }
            });
        }
    }

    //==============================================================================
    template <typename FloatType>
    void allocateChannelListAndBuffers (Array<FloatType*>& channelList, AudioBuffer<FloatType>& buffer)
    {
        channelList.clearQuick();
        channelList.insertMultiple (0, nullptr, 128);

        auto& p = getPluginInstance();
        buffer.setSize (jmax (p.getTotalNumInputChannels(), p.getTotalNumOutputChannels()), p.getBlockSize() * 4);
        buffer.clear();
    }

    template <typename FloatType>
    void deallocateChannelListAndBuffers (Array<FloatType*>& channelList, AudioBuffer<FloatType>& buffer)
    {
        channelList.clearQuick();
        channelList.resize (0);
        buffer.setSize (0, 0);
    }

    template <typename FloatType>
    static FloatType** getPointerForAudioBus (Vst::AudioBusBuffers& data) noexcept
    {
        return AudioBusPointerHelper<FloatType>::impl (data);
    }

    template <typename FloatType>
    FloatType* getTmpBufferForChannel (int channel, int numSamples) noexcept
    {
        auto& buffer = ChooseBufferHelper<FloatType>::impl (emptyBufferFloat, emptyBufferDouble);

        // we can't do anything if the host requests to render many more samples than the
        // block size, we need to bail out
        if (numSamples > buffer.getNumSamples() || channel >= buffer.getNumChannels())
            return nullptr;

        return buffer.getWritePointer (channel);
    }

    void preparePlugin (double sampleRate, int bufferSize)
    {
        auto& p = getPluginInstance();

        p.setRateAndBufferSizeDetails (sampleRate, bufferSize);
        p.prepareToPlay (sampleRate, bufferSize);

        midiBuffer.ensureSize (2048);
        midiBuffer.clear();
    }

    //==============================================================================
    ScopedJuceInitialiser_GUI libraryInitialiser;

   #if JUCE_LINUX || JUCE_BSD
    SharedResourcePointer<MessageThread> messageThread;
   #endif

    std::atomic<int> refCount { 1 };
    AudioProcessor* pluginInstance = nullptr;

   #if JUCE_LINUX || JUCE_BSD
    template <class T>
    struct LockedVSTComSmartPtr
    {
        LockedVSTComSmartPtr() = default;
        LockedVSTComSmartPtr (const VSTComSmartPtr<T>& ptrIn)  : ptr (ptrIn)  {}

        ~LockedVSTComSmartPtr()
        {
            const MessageManagerLock mmLock;
            ptr = {};
        }

        T* operator->()               { return ptr.operator->(); }
        operator T*() const noexcept  { return ptr.get(); }

        template <typename... Args>
        bool loadFrom (Args&&... args)  { return ptr.loadFrom (std::forward<Args> (args)...); }

    private:
        VSTComSmartPtr<T> ptr;
    };

    LockedVSTComSmartPtr<Vst::IHostApplication> host;
    LockedVSTComSmartPtr<JuceAudioProcessor> comPluginInstance;
    LockedVSTComSmartPtr<JuceVST3EditController> juceVST3EditController;
   #else
    VSTComSmartPtr<Vst::IHostApplication> host;
    VSTComSmartPtr<JuceAudioProcessor> comPluginInstance;
    VSTComSmartPtr<JuceVST3EditController> juceVST3EditController;
   #endif

    /**
        Since VST3 does not provide a way of knowing the buffer size and sample rate at any point,
        this object needs to be copied on every call to process() to be up-to-date...
    */
    Vst::ProcessContext processContext;
    Vst::ProcessSetup processSetup;

    MidiBuffer midiBuffer;
    Array<float*> channelListFloat;
    Array<double*> channelListDouble;

    AudioBuffer<float>  emptyBufferFloat;
    AudioBuffer<double> emptyBufferDouble;

   #if JucePlugin_WantsMidiInput
    std::atomic<bool> isMidiInputBusEnabled { true };
   #endif
   #if JucePlugin_ProducesMidiOutput
    std::atomic<bool> isMidiOutputBusEnabled { true };
   #endif

    static const char* kJucePrivateDataIdentifier;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JuceVST3Component)
};

const char* JuceVST3Component::kJucePrivateDataIdentifier = "JUCEPrivateData";

//==============================================================================
JUCE_BEGIN_IGNORE_WARNINGS_MSVC (4310)
JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wall")

DECLARE_CLASS_IID (JuceAudioProcessor, 0x0101ABAB, 0xABCDEF01, JucePlugin_ManufacturerCode, JucePlugin_PluginCode)
DEF_CLASS_IID (JuceAudioProcessor)

#if JUCE_VST3_CAN_REPLACE_VST2
 FUID getFUIDForVST2ID (bool forControllerUID)
 {
     TUID uuid;
     extern JUCE_API void getUUIDForVST2ID (bool, uint8[16]);
     getUUIDForVST2ID (forControllerUID, (uint8*) uuid);
     return FUID (uuid);
 }
 const Steinberg::FUID JuceVST3Component     ::iid (getFUIDForVST2ID (false));
 const Steinberg::FUID JuceVST3EditController::iid (getFUIDForVST2ID (true));
#else
 DECLARE_CLASS_IID (JuceVST3EditController, 0xABCDEF01, 0x1234ABCD, JucePlugin_ManufacturerCode, JucePlugin_PluginCode)
 DEF_CLASS_IID (JuceVST3EditController)

 DECLARE_CLASS_IID (JuceVST3Component, 0xABCDEF01, 0x9182FAEB, JucePlugin_ManufacturerCode, JucePlugin_PluginCode)
 DEF_CLASS_IID (JuceVST3Component)
#endif

JUCE_END_IGNORE_WARNINGS_MSVC
JUCE_END_IGNORE_WARNINGS_GCC_LIKE

//==============================================================================
bool initModule();
bool initModule()
{
   #if JUCE_MAC
    initialiseMacVST();
   #endif

    return true;
}

bool shutdownModule();
bool shutdownModule()
{
    return true;
}

#undef JUCE_EXPORTED_FUNCTION

#if JUCE_WINDOWS
 #define JUCE_EXPORTED_FUNCTION
#else
 #define JUCE_EXPORTED_FUNCTION extern "C" __attribute__ ((visibility("default")))
#endif

#if JUCE_WINDOWS
 extern "C" __declspec (dllexport) bool InitDll()   { return initModule(); }
 extern "C" __declspec (dllexport) bool ExitDll()   { return shutdownModule(); }
#elif JUCE_LINUX || JUCE_BSD
 void* moduleHandle = nullptr;
 int moduleEntryCounter = 0;

 JUCE_EXPORTED_FUNCTION bool ModuleEntry (void* sharedLibraryHandle);
 JUCE_EXPORTED_FUNCTION bool ModuleEntry (void* sharedLibraryHandle)
 {
     if (++moduleEntryCounter == 1)
     {
         moduleHandle = sharedLibraryHandle;
         return initModule();
     }

     return true;
 }

 JUCE_EXPORTED_FUNCTION bool ModuleExit();
 JUCE_EXPORTED_FUNCTION bool ModuleExit()
 {
     if (--moduleEntryCounter == 0)
     {
         moduleHandle = nullptr;
         return shutdownModule();
     }

     return true;
 }
#elif JUCE_MAC
 CFBundleRef globalBundleInstance = nullptr;
 juce::uint32 numBundleRefs = 0;
 juce::Array<CFBundleRef> bundleRefs;

 enum { MaxPathLength = 2048 };
 char modulePath[MaxPathLength] = { 0 };
 void* moduleHandle = nullptr;

 JUCE_EXPORTED_FUNCTION bool bundleEntry (CFBundleRef ref);
 JUCE_EXPORTED_FUNCTION bool bundleEntry (CFBundleRef ref)
 {
     if (ref != nullptr)
     {
         ++numBundleRefs;
         CFRetain (ref);

         bundleRefs.add (ref);

         if (moduleHandle == nullptr)
         {
             globalBundleInstance = ref;
             moduleHandle = ref;

             CFUniquePtr<CFURLRef> tempURL (CFBundleCopyBundleURL (ref));
             CFURLGetFileSystemRepresentation (tempURL.get(), true, (UInt8*) modulePath, MaxPathLength);
         }
     }

     return initModule();
 }

 JUCE_EXPORTED_FUNCTION bool bundleExit();
 JUCE_EXPORTED_FUNCTION bool bundleExit()
 {
     if (shutdownModule())
     {
         if (--numBundleRefs == 0)
         {
             for (int i = 0; i < bundleRefs.size(); ++i)
                 CFRelease (bundleRefs.getUnchecked (i));

             bundleRefs.clear();
         }

         return true;
     }

     return false;
 }
#endif

//==============================================================================
/** This typedef represents VST3's createInstance() function signature */
using CreateFunction = FUnknown* (*)(Vst::IHostApplication*);

static FUnknown* createComponentInstance (Vst::IHostApplication* host)
{
    return static_cast<Vst::IAudioProcessor*> (new JuceVST3Component (host));
}

static FUnknown* createControllerInstance (Vst::IHostApplication* host)
{
    return static_cast<Vst::IEditController*> (new JuceVST3EditController (host));
}

//==============================================================================
struct JucePluginFactory;
static JucePluginFactory* globalFactory = nullptr;

//==============================================================================
struct JucePluginFactory  : public IPluginFactory3
{
    JucePluginFactory()
        : factoryInfo (JucePlugin_Manufacturer, JucePlugin_ManufacturerWebsite,
                       JucePlugin_ManufacturerEmail, Vst::kDefaultFactoryFlags)
    {
    }

    virtual ~JucePluginFactory()
    {
        if (globalFactory == this)
            globalFactory = nullptr;
    }

    //==============================================================================
    bool registerClass (const PClassInfo2& info, CreateFunction createFunction)
    {
        if (createFunction == nullptr)
        {
            jassertfalse;
            return false;
        }

        auto entry = std::make_unique<ClassEntry> (info, createFunction);
        entry->infoW.fromAscii (info);

        classes.push_back (std::move (entry));

        return true;
    }

    //==============================================================================
    JUCE_DECLARE_VST3_COM_REF_METHODS

    tresult PLUGIN_API queryInterface (const TUID targetIID, void** obj) override
    {
        TEST_FOR_AND_RETURN_IF_VALID (targetIID, IPluginFactory3)
        TEST_FOR_AND_RETURN_IF_VALID (targetIID, IPluginFactory2)
        TEST_FOR_AND_RETURN_IF_VALID (targetIID, IPluginFactory)
        TEST_FOR_AND_RETURN_IF_VALID (targetIID, FUnknown)

        jassertfalse; // Something new?
        *obj = nullptr;
        return kNotImplemented;
    }

    //==============================================================================
    Steinberg::int32 PLUGIN_API countClasses() override
    {
        return (Steinberg::int32) classes.size();
    }

    tresult PLUGIN_API getFactoryInfo (PFactoryInfo* info) override
    {
        if (info == nullptr)
            return kInvalidArgument;

        memcpy (info, &factoryInfo, sizeof (PFactoryInfo));
        return kResultOk;
    }

    tresult PLUGIN_API getClassInfo (Steinberg::int32 index, PClassInfo* info) override
    {
        return getPClassInfo<PClassInfo> (index, info);
    }

    tresult PLUGIN_API getClassInfo2 (Steinberg::int32 index, PClassInfo2* info) override
    {
        return getPClassInfo<PClassInfo2> (index, info);
    }

    tresult PLUGIN_API getClassInfoUnicode (Steinberg::int32 index, PClassInfoW* info) override
    {
        if (info != nullptr)
        {
            if (auto& entry = classes[(size_t) index])
            {
                memcpy (info, &entry->infoW, sizeof (PClassInfoW));
                return kResultOk;
            }
        }

        return kInvalidArgument;
    }

    tresult PLUGIN_API createInstance (FIDString cid, FIDString sourceIid, void** obj) override
    {
        ScopedJuceInitialiser_GUI libraryInitialiser;

       #if JUCE_LINUX || JUCE_BSD
        SharedResourcePointer<MessageThread> messageThread;
       #endif

        *obj = nullptr;

        TUID tuid;
        memcpy (tuid, sourceIid, sizeof (TUID));

       #if VST_VERSION >= 0x030608
        auto sourceFuid = FUID::fromTUID (tuid);
       #else
        FUID sourceFuid;
        sourceFuid = tuid;
       #endif

        if (cid == nullptr || sourceIid == nullptr || ! sourceFuid.isValid())
        {
            jassertfalse; // The host you're running in has severe implementation issues!
            return kInvalidArgument;
        }

        TUID iidToQuery;
        sourceFuid.toTUID (iidToQuery);

        for (auto& entry : classes)
        {
            if (doUIDsMatch (entry->infoW.cid, cid))
            {
                if (auto* instance = entry->createFunction (host))
                {
                    const FReleaser releaser (instance);

                    if (instance->queryInterface (iidToQuery, obj) == kResultOk)
                        return kResultOk;
                }

                break;
            }
        }

        return kNoInterface;
    }

    tresult PLUGIN_API setHostContext (FUnknown* context) override
    {
        // WaveLab 10 on OS X (and maybe other WaveLabs): if the context object gets smart referenced here,
        // a crash happens on shutdown in case multiple instances were active.
        if (getHostType().isWavelab()) return kNotImplemented;
        
        host.loadFrom (context);

        if (host != nullptr)
        {
            Vst::String128 name;
            host->getName (name);

            return kResultTrue;
        }

        return kNotImplemented;
    }

private:
    //==============================================================================
    std::atomic<int> refCount { 1 };
    const PFactoryInfo factoryInfo;
    VSTComSmartPtr<Vst::IHostApplication> host;

    //==============================================================================
    struct ClassEntry
    {
        ClassEntry() noexcept {}

        ClassEntry (const PClassInfo2& info, CreateFunction fn) noexcept
            : info2 (info), createFunction (fn) {}

        PClassInfo2 info2;
        PClassInfoW infoW;
        CreateFunction createFunction = {};
        bool isUnicode = false;

    private:
        JUCE_DECLARE_NON_COPYABLE (ClassEntry)
    };

    std::vector<std::unique_ptr<ClassEntry>> classes;

    //==============================================================================
    template <class PClassInfoType>
    tresult PLUGIN_API getPClassInfo (Steinberg::int32 index, PClassInfoType* info)
    {
        if (info != nullptr)
        {
            zerostruct (*info);

            if (auto& entry = classes[(size_t) index])
            {
                if (entry->isUnicode)
                    return kResultFalse;

                memcpy (info, (PClassInfoType*) &entry->info2, sizeof (PClassInfoType));
                return kResultOk;
            }
        }

        jassertfalse;
        return kInvalidArgument;
    }

    //==============================================================================
    // no leak detector here to prevent it firing on shutdown when running in hosts that
    // don't release the factory object correctly...
    JUCE_DECLARE_NON_COPYABLE (JucePluginFactory)
};

} // namespace juce

//==============================================================================
#ifndef JucePlugin_Vst3ComponentFlags
 #if JucePlugin_IsSynth
  #define JucePlugin_Vst3ComponentFlags Vst::kSimpleModeSupported
 #else
  #define JucePlugin_Vst3ComponentFlags 0
 #endif
#endif

#ifndef JucePlugin_Vst3Category
 #if JucePlugin_IsSynth
  #define JucePlugin_Vst3Category Vst::PlugType::kInstrumentSynth
 #else
  #define JucePlugin_Vst3Category Vst::PlugType::kFx
 #endif
#endif

using namespace juce;

//==============================================================================
// The VST3 plugin entry point.
JUCE_EXPORTED_FUNCTION IPluginFactory* PLUGIN_API GetPluginFactory()
{
    PluginHostType::jucePlugInClientCurrentWrapperType = AudioProcessor::wrapperType_VST3;

   #if JUCE_MSVC || (JUCE_WINDOWS && JUCE_CLANG)
    // Cunning trick to force this function to be exported. Life's too short to
    // faff around creating .def files for this kind of thing.
    #if JUCE_32BIT
     #pragma comment(linker, "/EXPORT:GetPluginFactory=_GetPluginFactory@0")
    #else
     #pragma comment(linker, "/EXPORT:GetPluginFactory=GetPluginFactory")
    #endif
   #endif

    if (globalFactory == nullptr)
    {
        globalFactory = new JucePluginFactory();

        static const PClassInfo2 componentClass (JuceVST3Component::iid,
                                                 PClassInfo::kManyInstances,
                                                 kVstAudioEffectClass,
                                                 JucePlugin_Name,
                                                 JucePlugin_Vst3ComponentFlags,
                                                 JucePlugin_Vst3Category,
                                                 JucePlugin_Manufacturer,
                                                 JucePlugin_VersionString,
                                                 kVstVersionString);

        globalFactory->registerClass (componentClass, createComponentInstance);

        static const PClassInfo2 controllerClass (JuceVST3EditController::iid,
                                                  PClassInfo::kManyInstances,
                                                  kVstComponentControllerClass,
                                                  JucePlugin_Name,
                                                  JucePlugin_Vst3ComponentFlags,
                                                  JucePlugin_Vst3Category,
                                                  JucePlugin_Manufacturer,
                                                  JucePlugin_VersionString,
                                                  kVstVersionString);

        globalFactory->registerClass (controllerClass, createControllerInstance);
    }
    else
    {
        globalFactory->addRef();
    }

    return dynamic_cast<IPluginFactory*> (globalFactory);
}

//==============================================================================
#if JUCE_WINDOWS
extern "C" BOOL WINAPI DllMain (HINSTANCE instance, DWORD reason, LPVOID) { if (reason == DLL_PROCESS_ATTACH) Process::setCurrentModuleInstanceHandle (instance); return true; }
#endif

#endif //JucePlugin_Build_VST3
