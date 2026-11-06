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

#include <juce_core/system/juce_TargetPlatform.h>
#include <juce_core/system/juce_CompilerWarnings.h>
#include <juce_audio_processors_headless/format/juce_PluginFormatDefs.h>

//==============================================================================
#if JucePlugin_Build_VST3

JUCE_BEGIN_NO_SANITIZE ("vptr")

#if JUCE_INTERNAL_HAS_VST3
 #if JUCE_MAC
  #include <CoreFoundation/CoreFoundation.h>
 #endif
 #undef JUCE_VST3HEADERS_INCLUDE_HEADERS_ONLY
 #define JUCE_VST3HEADERS_INCLUDE_HEADERS_ONLY 1
#endif

#include <juce_audio_processors_headless/format_types/juce_VST3Headers.h>

#undef JUCE_VST3HEADERS_INCLUDE_HEADERS_ONLY
#define JUCE_GUI_BASICS_INCLUDE_XHEADERS 1

#include <juce_audio_plugin_client/detail/juce_CheckSettingMacros.h>
#include <juce_audio_plugin_client/detail/juce_IncludeSystemHeaders.h>
#include <juce_audio_plugin_client/detail/juce_PluginUtilities.h>
#include <juce_audio_plugin_client/detail/juce_PluginScaleFactorUtilities.h>
#include <juce_audio_plugin_client/detail/juce_LinuxMessageThread.h>
#include <juce_audio_plugin_client/detail/juce_VSTWindowUtilities.h>
#include <juce_gui_basics/native/juce_WindowsHooks_windows.h>

//==============================================================================
#include <juce_audio_processors_headless/format_types/juce_LegacyAudioParameter.h>
#include <juce_audio_processors_headless/utilities/juce_FlagCache.h>
#include <juce_audio_processors_headless/format_types/juce_VST3Utilities.h>
#include <juce_audio_processors_headless/format_types/juce_VST3Common.h>
#include <juce_audio_plugin_client/VST3/juce_VST3ModuleInfo.h>

#if JUCE_VST3_CAN_REPLACE_VST2 && ! JUCE_FORCE_USE_LEGACY_PARAM_IDS && ! JUCE_IGNORE_VST3_MISMATCHED_PARAMETER_ID_WARNING

 // If you encounter this error there may be an issue migrating parameter
 // automation between sessions saved using the VST2 and VST3 versions of this
 // plugin.
 //
 // If you have released neither a VST2 or VST3 version of the plugin,
 // consider only releasing a VST3 version and disabling JUCE_VST3_CAN_REPLACE_VST2.
 //
 // If you have released a VST2 version of the plugin but have not yet released
 // a VST3 version of the plugin, consider enabling JUCE_FORCE_USE_LEGACY_PARAM_IDS.
 // This will ensure that the parameter IDs remain compatible between both the
 // VST2 and VST3 versions of the plugin in all hosts.
 //
 // If you have released a VST3 version of the plugin but have not released a
 // VST2 version of the plugin, enable JUCE_IGNORE_VST3_MISMATCHED_PARAMETER_ID_WARNING.
 // DO NOT change the JUCE_VST3_CAN_REPLACE_VST2 or JUCE_FORCE_USE_LEGACY_PARAM_IDS
 // values as this will break compatibility with currently released VST3
 // versions of the plugin.
 //
 // If you have already released a VST2 and VST3 version of the plugin you may
 // find in some hosts when a session containing automation data is saved using
 // the VST2 or VST3 version, and is later loaded using the other version, the
 // automation data will fail to control any of the parameters in the plugin as
 // the IDs for these parameters are different. To fix parameter automation for
 // the VST3 plugin when a session was saved with the VST2 plugin, implement
 // VST3ClientExtensions::getCompatibleParameterIds() and enable
 // JUCE_IGNORE_VST3_MISMATCHED_PARAMETER_ID_WARNING.

 #error You may have a conflict with parameter automation between VST2 and VST3 versions of your plugin. See the comment above for more details.
#endif

#ifndef JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS
 #if JucePlugin_WantsMidiInput
  #define JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS 1
 #endif
#endif

#if JUCE_LINUX || JUCE_BSD
 #include <juce_events/native/juce_EventLoopInternal_linux.h>
 #include <unordered_map>
#endif

#if JUCE_MAC
 #include <juce_core/native/juce_CFHelpers_mac.h>
#endif

namespace juce
{

using namespace Steinberg;

//==============================================================================
#if JUCE_LINUX || JUCE_BSD

enum class HostMessageThreadAttached { no, yes };

class HostMessageThreadState
{
public:
    template <typename Callback>
    void setStateWithAction (HostMessageThreadAttached stateIn, Callback&& action)
    {
        const std::lock_guard<std::mutex> lock { m };
        state = stateIn;
        action();
    }

    void assertHostMessageThread()
    {
        const std::lock_guard<std::mutex> lock { m };

        if (state == HostMessageThreadAttached::no)
            return;

        JUCE_ASSERT_MESSAGE_THREAD
    }

private:
    HostMessageThreadAttached state = HostMessageThreadAttached::no;
    std::mutex m;
};

class EventHandler final  : public Linux::IEventHandler,
                            private LinuxEventLoopInternal::Listener
{
public:
    EventHandler()
    {
        LinuxEventLoopInternal::registerLinuxEventLoopListener (*this);
    }

    ~EventHandler() override
    {
        jassert (hostRunLoops.empty());

        LinuxEventLoopInternal::deregisterLinuxEventLoopListener (*this);

        if (! messageThread->isRunning())
            hostMessageThreadState.setStateWithAction (HostMessageThreadAttached::no,
                                                       [this]() { messageThread->start(); });
    }

    JUCE_DECLARE_VST3_COM_REF_METHODS

    tresult PLUGIN_API queryInterface (const TUID targetIID, void** obj) override
    {
        return testFor (*this, targetIID, UniqueBase<Linux::IEventHandler>{}).extract (obj);
    }

    void PLUGIN_API onFDIsSet (Linux::FileDescriptor fd) override
    {
        updateCurrentMessageThread();
        LinuxEventLoopInternal::invokeEventLoopCallbackForFd (fd);
    }

    //==============================================================================
    void registerHandlerForRunLoop (Linux::IRunLoop* l)
    {
        if (l == nullptr)
            return;

        refreshAttachedEventLoop ([this, l] { hostRunLoops.insert (l); });
        updateCurrentMessageThread();
    }

    void unregisterHandlerForRunLoop (Linux::IRunLoop* l)
    {
        if (l == nullptr)
            return;

        refreshAttachedEventLoop ([this, l]
        {
            const auto it = hostRunLoops.find (l);

            if (it != hostRunLoops.end())
                hostRunLoops.erase (it);
        });
    }

    /* Asserts if it can be established that the calling thread is different from the host's message
       thread.

       On Linux this can only be determined if the host has already registered its run loop. Until
       then JUCE messages are serviced by a background thread internal to the plugin.
    */
    static void assertHostMessageThread()
    {
        hostMessageThreadState.assertHostMessageThread();
    }

private:
    //==============================================================================
    /*  Connects all known FDs to a single host event loop instance. */
    class AttachedEventLoop
    {
    public:
        AttachedEventLoop() = default;

        AttachedEventLoop (Linux::IRunLoop* loopIn, Linux::IEventHandler* handlerIn)
            : loop (loopIn), handler (handlerIn)
        {
            for (auto& fd : LinuxEventLoopInternal::getRegisteredFds())
                loop->registerEventHandler (handler, fd);
        }

        AttachedEventLoop (AttachedEventLoop&& other) noexcept
        {
            swap (other);
        }

        AttachedEventLoop& operator= (AttachedEventLoop&& other) noexcept
        {
            swap (other);
            return *this;
        }

        AttachedEventLoop (const AttachedEventLoop&) = delete;
        AttachedEventLoop& operator= (const AttachedEventLoop&) = delete;

        ~AttachedEventLoop()
        {
            if (loop == nullptr)
                return;

            loop->unregisterEventHandler (handler);
        }

    private:
        void swap (AttachedEventLoop& other)
        {
            std::swap (other.loop, loop);
            std::swap (other.handler, handler);
        }

        Linux::IRunLoop* loop = nullptr;
        Linux::IEventHandler* handler = nullptr;
    };

    //==============================================================================
    void updateCurrentMessageThread()
    {
        if (! MessageManager::getInstance()->isThisTheMessageThread())
        {
            if (messageThread->isRunning())
                messageThread->stop();

            hostMessageThreadState.setStateWithAction (HostMessageThreadAttached::yes,
                                                       [] { MessageManager::getInstance()->setCurrentThreadAsMessageThread(); });
        }
    }

    void fdCallbacksChanged() override
    {
        // The set of active FDs has changed, so deregister from the current event loop and then
        // re-register the current set of FDs.
        refreshAttachedEventLoop ([]{});
    }

    /*  Deregisters from any attached event loop, updates the set of known event loops, and then
        attaches all FDs to the first known event loop.

        The same event loop instance is shared between all plugin instances. Every time an event
        loop is added or removed, this function should be called to register all FDs with a
        suitable event loop.

        Note that there's no API to deregister a single FD for a given event loop. Instead, we must
        deregister all FDs, and then register all known FDs again.
    */
    template <typename Callback>
    void refreshAttachedEventLoop (Callback&& modifyKnownRunLoops)
    {
        // Deregister the old event loop.
        // It's important to call the destructor from the old attached loop before calling the
        // constructor of the new attached loop.
        attachedEventLoop = AttachedEventLoop();

        modifyKnownRunLoops();

        // If we still know about an extant event loop, attach to it.
        if (hostRunLoops.begin() != hostRunLoops.end())
            attachedEventLoop = AttachedEventLoop (*hostRunLoops.begin(), this);
    }

    SharedResourcePointer<detail::MessageThread> messageThread;

    std::atomic<int> refCount { 1 };

    std::multiset<Linux::IRunLoop*> hostRunLoops;
    AttachedEventLoop attachedEventLoop;

    static inline HostMessageThreadState hostMessageThreadState;

    //==============================================================================
    JUCE_DECLARE_NON_MOVEABLE (EventHandler)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EventHandler)
};

#endif

static void assertHostMessageThread()
{
   #if JUCE_LINUX || JUCE_BSD
    EventHandler::assertHostMessageThread();
   #else
    JUCE_ASSERT_MESSAGE_THREAD
   #endif
}

//==============================================================================
class InParameterChangedCallbackSetter
{
public:
    explicit InParameterChangedCallbackSetter (bool& ref)
        : inner ([&]() -> auto& { jassert (! ref); return ref; }(), true, false) {}

private:
    ScopedValueSetter<bool> inner;
};

template <typename Member>
static QueryInterfaceResult queryAdditionalInterfaces (AudioProcessor* processor,
                                                       const TUID targetIID,
                                                       Member&& member)
{
    if (processor == nullptr)
        return {};

    void* obj = nullptr;

    if (auto* extensions = processor->getVST3ClientExtensions())
    {
        const auto result = (extensions->*member) (targetIID, &obj);
        return { result, obj };
    }

    return {};
}

static tresult extractResult (const QueryInterfaceResult& userInterface,
                              const InterfaceResultWithDeferredAddRef& juceInterface,
                              void** obj)
{
    if (userInterface.isOk() && juceInterface.isOk())
    {
        // If you hit this assertion, you've provided a custom implementation of an interface
        // that JUCE implements already. As a result, your plugin may not behave correctly.
        // Consider removing your custom implementation.
        jassertfalse;

        return userInterface.extract (obj);
    }

    if (userInterface.isOk())
        return userInterface.extract (obj);

    return juceInterface.extract (obj);
}

//==============================================================================
class JuceAudioProcessor final : public Vst::IUnitInfo
{
public:
    explicit JuceAudioProcessor (AudioProcessor* source) noexcept
        : audioProcessor (source)
    {
        setupParameters();
    }

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
            info.programListId  = getProgramListCount() > 0
                                ? static_cast<Vst::ProgramListID> (programParamID)
                                : Vst::kNoProgramListId;

            toString128 (info.name, TRANS ("Root Unit"));

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

            toString128 (info.name, TRANS ("Factory Presets"));

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

    tresult PLUGIN_API hasProgramPitchNames (Vst::ProgramListID, Steinberg::int32) override
    {
        for (int i = 0; i <= 127; ++i)
            if (audioProcessor->getNameForMidiNoteNumber (i, 1))
                return kResultTrue;

        return kResultFalse;
    }

    tresult PLUGIN_API getProgramPitchName (Vst::ProgramListID, Steinberg::int32, Steinberg::int16 midiNote, Vst::String128 nameOut) override
    {
        if (auto name = audioProcessor->getNameForMidiNoteNumber (midiNote, 1))
        {
            toString128 (nameOut, *name);
            return kResultTrue;
        }

        return kResultFalse;
    }

    tresult PLUGIN_API getProgramInfo (Vst::ProgramListID, Steinberg::int32, Vst::CString, Vst::String128) override             { return kNotImplemented; }
    tresult PLUGIN_API selectUnit (Vst::UnitID) override                                                                        { return kNotImplemented; }
    tresult PLUGIN_API setUnitProgramData (Steinberg::int32, Steinberg::int32, IBStream*) override                              { return kNotImplemented; }
    Vst::UnitID PLUGIN_API getSelectedUnit() override                                                                           { return Vst::kRootUnitId; }

    tresult PLUGIN_API getUnitByBus (Vst::MediaType, Vst::BusDirection, Steinberg::int32, Steinberg::int32, Vst::UnitID& unitId) override
    {
        unitId = Vst::kRootUnitId;
        return kResultOk;
    }

    //==============================================================================
    inline Vst::ParamID getVSTParamIDForIndex (int paramIndex) const noexcept
    {
       #if JUCE_FORCE_USE_LEGACY_PARAM_IDS
        return static_cast<Vst::ParamID> (paramIndex);
       #else
        jassert (paramIndex < vstParamIDs.size());
        return vstParamIDs.getReference (paramIndex);
       #endif
    }

    AudioProcessorParameter* getParamForVSTParamID (Vst::ParamID paramID) const noexcept
    {
        const auto iter = paramMap.find (paramID);
        return iter != paramMap.end() ? iter->second : nullptr;
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

    int findCacheIndexForParamID (Vst::ParamID paramID) const noexcept { return vstParamIDs.indexOf (paramID); }

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

    std::map<Vst::ParamID, AudioProcessorParameter*> getParameterMap (const VST3Interface::Id& pluginId) const
    {
        const auto iter = compatibleParameterIdMap.find (pluginId);
        return iter != compatibleParameterIdMap.end() ? iter->second
                                                      : std::map<Vst::ParamID, AudioProcessorParameter*>{};
    }

    AudioProcessorParameter* getParameter (const String& juceParamId) const
    {
        const auto iter = juceIdParameterMap.find (juceParamId);
        return iter != juceIdParameterMap.end() ? iter->second : nullptr;
    }

    void updateParameterMapping()
    {
        static const auto currentPluginId = getVST3InterfaceId (VST3Interface::Type::component);

        compatibleParameterIdMap = {};
        compatibleParameterIdMap[currentPluginId] = paramMap;

        auto* ext = audioProcessor->getVST3ClientExtensions();

        // If there are no extensions, we assume that no adjustments should be made to the mapping.
        if (ext == nullptr)
            return;

        for (const auto& compatibleClass : getAllVST3CompatibleClasses())
        {
            auto& parameterIdMap = compatibleParameterIdMap[compatibleClass];
            for (auto [oldParamId, newParamId] : ext->getCompatibleParameterIds (compatibleClass))
            {
                auto* parameter = getParameter (newParamId);
                parameterIdMap[oldParamId] = parameter;

                // This means a parameter ID returned by getCompatibleParameterIds()
                // does not match any parameters declared in the plugin. All IDs must
                // match an existing parameter, or return an empty string to indicate
                // there is no parameter to map to.
                jassert (parameter != nullptr || newParamId.isEmpty());

                // This means getCompatibleParameterIds() returned a parameter mapping
                // that will hide a parameter in the current plugin! If this is due to
                // an ID collision between plugin versions, you may be able to determine
                // the mapping to report based on setStateInformation(). If you've
                // already done this you can safely ignore this warning. If there is no
                // way to determine the difference between the two plugin versions in
                // setStateInformation() the best course of action is to remove the
                // problematic parameter from the mapping.
                jassert (compatibleClass != currentPluginId
                         || getParamForVSTParamID (oldParamId) == nullptr
                         || parameter == getParamForVSTParamID (oldParamId));
            }
        }
    }

    //==============================================================================
    inline static const FUID iid = toSteinbergUID (getVST3InterfaceId (VST3Interface::Type::processor));

private:
    //==============================================================================
    void setupParameters()
    {
        parameterGroups = audioProcessor->getParameterTree().getSubgroups (true);

       #if JUCE_ASSERTIONS_ENABLED_OR_LOGGED
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
            ownedBypassParameter.reset (new AudioParameterBool ("byps", "Bypass", false));
            bypassParameter = ownedBypassParameter.get();
        }

        // if the bypass parameter is not part of the exported parameters that the plug-in supports
        // then add it to the end of the list as VST3 requires the bypass parameter to be exported!
        bypassIsRegularParameter = juceParameters.contains (audioProcessor->getBypassParameter());

        if (! bypassIsRegularParameter)
            juceParameters.addNonOwning (bypassParameter);

        int i = 0;
        for (auto* juceParam : juceParameters)
        {
            bool isBypassParameter = (juceParam == bypassParameter);

            Vst::ParamID vstParamID = forceLegacyParamIDs ? static_cast<Vst::ParamID> (i++)
                                                          : generateVSTParamIDForParam (juceParam);

            if (isBypassParameter)
            {
                // we need to remain backward compatible with the old bypass id
                if (vst3WrapperProvidedBypassParam)
                {
                    JUCE_BEGIN_IGNORE_WARNINGS_MSVC (6240)
                    vstParamID = static_cast<Vst::ParamID> ((isUsingManagedParameters() && ! forceLegacyParamIDs) ? paramBypass : numParameters);
                    JUCE_END_IGNORE_WARNINGS_MSVC
                }

                bypassParamID = vstParamID;
            }

            vstParamIDs.add (vstParamID);
            paramMap[vstParamID] = juceParam;
            juceIdParameterMap[LegacyAudioParameter::getParamID (juceParam, false)] = juceParam;
        }

        auto numPrograms = audioProcessor->getNumPrograms();

        if (numPrograms > 1)
        {
            ownedProgramParameter = std::make_unique<AudioParameterInt> ("juceProgramParameter", "Program",
                                                                         0, numPrograms - 1,
                                                                         audioProcessor->getCurrentProgram());

            juceParameters.addNonOwning (ownedProgramParameter.get());

            if (forceLegacyParamIDs)
                programParamID = static_cast<Vst::ParamID> (i++);

            vstParamIDs.add (programParamID);
            paramMap[programParamID] = ownedProgramParameter.get();
        }

        cachedParamValues = CachedParamValues { { vstParamIDs.begin(), vstParamIDs.end() } };
    }

    Vst::ParamID generateVSTParamIDForParam (const AudioProcessorParameter* param)
    {
        const auto juceParamID = LegacyAudioParameter::getParamID (param, false);

       #if JUCE_FORCE_USE_LEGACY_PARAM_IDS
        return static_cast<Vst::ParamID> (juceParamID.getIntValue());
       #else
        return VST3ClientExtensions::convertJuceParameterId (juceParamID, JUCE_USE_STUDIO_ONE_COMPATIBLE_PARAMETERS);
       #endif
    }

    //==============================================================================
    Array<Vst::ParamID> vstParamIDs;
    CachedParamValues cachedParamValues;
    Vst::ParamID bypassParamID = 0, programParamID = static_cast<Vst::ParamID> (paramPreset);
    bool bypassIsRegularParameter = false;
    std::map<VST3Interface::Id, std::map<Vst::ParamID, AudioProcessorParameter*>> compatibleParameterIdMap;
    std::map<String, AudioProcessorParameter*> juceIdParameterMap;

    //==============================================================================
    std::atomic<int> refCount { 0 };
    std::unique_ptr<AudioProcessor> audioProcessor;

    //==============================================================================
    LegacyAudioParametersWrapper juceParameters;
    std::map<Vst::ParamID, AudioProcessorParameter*> paramMap;
    std::unique_ptr<AudioProcessorParameter> ownedBypassParameter, ownedProgramParameter;
    Array<const AudioProcessorParameterGroup*> parameterGroups;

    JuceAudioProcessor() = delete;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JuceAudioProcessor)
};

#if JUCE_LINUX || JUCE_BSD
using RunLoop = VSTComSmartPtr<Linux::IRunLoop>;

class ScopedRunLoop
{
public:
    explicit ScopedRunLoop (const RunLoop& l)
        : runLoop (l)
    {
        eventHandler->registerHandlerForRunLoop (runLoop.get());
    }

    ~ScopedRunLoop()
    {
        eventHandler->unregisterHandlerForRunLoop (runLoop.get());
    }

    RunLoop get() const { return runLoop; }

    JUCE_DECLARE_NON_COPYABLE (ScopedRunLoop)
    JUCE_DECLARE_NON_MOVEABLE (ScopedRunLoop)

    static RunLoop getRunLoopFromFrame (IPlugFrame* plugFrame)
    {
        VSTComSmartPtr<Linux::IRunLoop> result;
        result.loadFrom (plugFrame);
        return result;
    }

private:
    ScopedJuceInitialiser_GUI libraryInitialiser;
    SharedResourcePointer<detail::MessageThread> messageThread;
    SharedResourcePointer<EventHandler> eventHandler;
    RunLoop runLoop;
};
#else
struct RunLoop
{
    void loadFrom (FUnknown*) {}
};

class ScopedRunLoop
{
public:
    explicit ScopedRunLoop (const RunLoop&) {}
    RunLoop get() const { return {}; }
    static RunLoop getRunLoopFromFrame (IPlugFrame*) { return {}; }

private:
    ScopedJuceInitialiser_GUI libraryInitialiser;
};
#endif

class JuceVST3Component;

static thread_local bool inParameterChangedCallback = false;

static void setValueAndNotifyIfChanged (AudioProcessorParameter& param, float newValue)
{
    if (approximatelyEqual (param.getValue(), newValue))
        return;

    const InParameterChangedCallbackSetter scopedSetter { inParameterChangedCallback };
    param.setValueNotifyingHost (newValue);
}

//==============================================================================
class JuceVST3EditController final : public Vst::EditController,
                                     public Vst::IMidiMapping,
                                     public Vst::IUnitInfo,
                                     public Vst::IRemapParamID,
                                     public Vst::ChannelContext::IInfoListener,
                                    #if JucePlugin_Enable_ARA
                                     public Presonus::IPlugInViewEmbedding,
                                    #endif
                                     public AudioProcessorListener,
                                     private ComponentRestarter::Listener
{
public:
    JuceVST3EditController (const VSTComSmartPtr<Vst::IHostApplication>& host,
                            const RunLoop& l)
        : scopedRunLoop (l)
    {
        if (host != nullptr)
            host->queryInterface (FUnknown::iid, (void**) &hostContext);

        blueCatPatchwork |= isBlueCatHost (host.get());
    }

    //==============================================================================
    inline static const FUID iid = toSteinbergUID (getVST3InterfaceId (VST3Interface::Type::controller));

    //==============================================================================
    JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Winconsistent-missing-override")

    REFCOUNT_METHODS (ComponentBase)

    JUCE_END_IGNORE_WARNINGS_GCC_LIKE

    tresult PLUGIN_API queryInterface (const TUID targetIID, void** obj) override
    {
        const auto userProvidedInterface = queryAdditionalInterfaces (getPluginInstance(),
                                                                      targetIID,
                                                                      &VST3ClientExtensions::queryIEditController);

        const auto juceProvidedInterface = queryInterfaceInternal (targetIID);

        return extractResult (userProvidedInterface, juceProvidedInterface, obj);
    }

    //==============================================================================
    tresult PLUGIN_API initialize (FUnknown* context) override
    {
        if (hostContext != context)
            hostContext = context;

        blueCatPatchwork |= isBlueCatHost (context);

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
    struct Param final : public Vst::Parameter
    {
        Param (JuceVST3EditController& editController, AudioProcessorParameter& p,
               Vst::ParamID vstParamID, Vst::UnitID vstUnitID,
               bool isBypassParameter)
            : owner (editController), param (p)
        {
            info.id = vstParamID;
            info.unitId = vstUnitID;

            updateParameterInfo();

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

            const auto updateParamIfScalarChanged = [] (auto& toChange, const auto newValue)
            {
                return ! exactlyEqual (std::exchange (toChange, newValue), newValue);
            };

            const auto newStepCount = [&]
            {
               #if ! JUCE_FORCE_LEGACY_PARAMETER_AUTOMATION_TYPE
                if (! param.isDiscrete())
                    return 0;
               #endif

                const auto numSteps = param.getNumSteps();
                return (Steinberg::int32) (0 < numSteps && numSteps < 0x7fffffff ? numSteps - 1 : 0);
            }();

            auto anyUpdated = updateParamIfChanged (info.title, param.getName (128));
            anyUpdated |= updateParamIfChanged (info.shortTitle, param.getName (8));
            anyUpdated |= updateParamIfChanged (info.units, param.getLabel());
            anyUpdated |= updateParamIfScalarChanged (info.stepCount, newStepCount);
            anyUpdated |= updateParamIfScalarChanged (info.defaultNormalizedValue, (double) param.getDefaultValue());

            jassert (0 <= info.defaultNormalizedValue && info.defaultNormalizedValue <= 1.0);

            return anyUpdated;
        }

        bool setNormalized (Vst::ParamValue v) override
        {
            v = jlimit (0.0, 1.0, v);

            if (! approximatelyEqual (v, valueNormalized))
            {
                valueNormalized = v;

                // Only update the AudioProcessor here if we're not playing,
                // otherwise we get parallel streams of parameter value updates
                // during playback
                if (! owner.vst3IsPlaying)
                    setValueAndNotifyIfChanged (param, (float) v);

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
    struct ProgramChangeParameter final : public Vst::Parameter
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
            const auto programValue = getProgramValueFromNormalised (v);

            if (programValue != owner.getCurrentProgram())
                owner.setCurrentProgram (programValue);

            if (! approximatelyEqual (valueNormalized, v))
            {
                valueNormalized = v;
                changed();

                return true;
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

        Steinberg::int32 getProgramValueFromNormalised (Vst::ParamValue v) const
        {
            return jmin (info.stepCount, (Steinberg::int32) (v * (info.stepCount + 1)));
        }

        Vst::ParamValue toPlain (Vst::ParamValue v) const override       { return getProgramValueFromNormalised (v); }
        Vst::ParamValue toNormalized (Vst::ParamValue v) const override  { return v / info.stepCount; }

    private:
        AudioProcessor& owner;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProgramChangeParameter)
    };

    //==============================================================================
    tresult PLUGIN_API getCompatibleParamID (const TUID pluginToReplaceUID,
                                             Vst::ParamID oldParamID,
                                             Vst::ParamID& newParamID) override
    {
        if (audioProcessor == nullptr)
        {
            jassertfalse;
            return kResultFalse;
        }

        const auto parameterMap = audioProcessor->getParameterMap (toVST3InterfaceId (pluginToReplaceUID));
        const auto iter = parameterMap.find (oldParamID);

        if (iter == parameterMap.end())
        {
            // This suggests a host is trying to load a plugin and parameter ID
            // combination that hasn't been accounted for in getCompatibleParameterIds().
            // Override this method in VST3ClientExtensions and return a suitable
            // parameter mapping to silence this warning.
            jassertfalse;
            return kResultFalse;
        }

        const auto* parameter = iter->second;

        if (parameter == nullptr)
        {
            // There's a null entry in the map of compatible parameters.
            // This implies a problem with the implementation of getCompatibleParameterIds - one of
            // the IDs in the returned map doesn't refer to any parameter in the current plugin.
            jassertfalse;
            return kResultFalse;
        }

        // We found a compatible parameter in the map.
        newParamID = audioProcessor->getVSTParamIDForIndex (parameter->getParameterIndex());
        return kResultTrue;
    }

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
                        trackProperties.name = std::make_optional (toString (channelName));
                }

                {
                    Steinberg::int64 colour;
                    if (list->getInt (Vst::ChannelContext::kChannelColorKey, colour) == kResultTrue)
                        trackProperties.colourARGB.emplace ((uint32) colour);
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
   #if JucePlugin_Enable_ARA
    Steinberg::TBool PLUGIN_API isViewEmbeddingSupported() override
    {
        if (auto* pluginInstance = getPluginInstance())
            if (auto* extension = pluginInstance->getARAClientExtensions())
                return (Steinberg::TBool) extension->isEditorView();

        return (Steinberg::TBool) false;
    }

    Steinberg::tresult PLUGIN_API setViewIsEmbedded (Steinberg::IPlugView* /*view*/, Steinberg::TBool /*embedded*/) override
    {
        return kResultOk;
    }
   #endif

    //==============================================================================
    tresult PLUGIN_API setComponentState (IBStream*) override
    {
        // As an IEditController member, the host should only call this from the message thread.
        assertHostMessageThread();

        auto restartFlags = toUnderlyingType (Vst::kParamValuesChanged);

        if (audioProcessor != nullptr)
        {
            auto* pluginInstance = getPluginInstance();

            for (auto vstParamId : audioProcessor->getParamIDs())
            {
                auto paramValue = std::invoke ([&]
                {
                    if (vstParamId == audioProcessor->getProgramParamID() && pluginInstance != nullptr)
                        return EditController::plainParamToNormalized (audioProcessor->getProgramParamID(),
                                                                       pluginInstance->getCurrentProgram());

                    return (double) audioProcessor->getParamForVSTParamID (vstParamId)->getValue();
                });

                setParamNormalized (vstParamId, paramValue);
            }

            if (! getAllVST3CompatibleClasses().empty())
            {
                restartFlags |= Vst::kParamIDMappingChanged;
                audioProcessor->updateParameterMapping();
            }
        }

        if (auto* handler = getComponentHandler())
            handler->restartComponent (restartFlags);

        return kResultOk;
    }

    
    //==============================================================================
    tresult PLUGIN_API setComponentHandler (Steinberg::Vst::IComponentHandler* newHandler) override
    {
        auto result = Vst::EditController::setComponentHandler (newHandler); 
        // The following is needed because Studio One calls this method after our installAudioProcessor method,
        // so installAudioProcessor is not able to set a non-null IComponentHandler on VST3ClientExtensions.
        // Other DAWs call this method before our installAudioProcessor (when audioProcessor is still nullptr).        
        if(audioProcessor != nullptr)
        {
            if (auto* extensions = audioProcessor->get()->getVST3ClientExtensions())
            {
                extensions->setIComponentHandler (componentHandler);
            }            
        }
        return result;
    }

    void setAudioProcessor (JuceAudioProcessor* audioProc)
    {
        if (audioProcessor.get() != audioProc)
            installAudioProcessor ({ audioProc, IncrementRef::yes });
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
    tresult PLUGIN_API getMidiControllerAssignment ([[maybe_unused]] Steinberg::int32 busIndex,
                                                    [[maybe_unused]] Steinberg::int16 channel,
                                                    [[maybe_unused]] Vst::CtrlNumber midiControllerNumber,
                                                    [[maybe_unused]] Vst::ParamID& resultID) override
    {
       #if JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS
        resultID = midiControllerToParameter[channel][midiControllerNumber];
        return kResultTrue; // Returning false makes some hosts stop asking for further MIDI Controller Assignments
       #else
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

        jassertfalse;
        if (unitIndex == 0)
        {
            info.id             = Vst::kRootUnitId;
            info.parentUnitId   = Vst::kNoParentUnitId;
            info.programListId  = Vst::kNoProgramListId;

            toString128 (info.name, TRANS ("Root Unit"));

            return kResultTrue;
        }

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
                                           IBStream* data) override
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

    tresult PLUGIN_API setComponentHandler (Vst::IComponentHandler* handler) override
    {
        const auto result = EditController::setComponentHandler (handler);

        if (result != kResultTrue)
            return result;

        if (audioProcessor != nullptr)
        {
            if (auto* extensions = audioProcessor->get()->getVST3ClientExtensions())
                extensions->setIComponentHandler (componentHandler);
        }

        return kResultTrue;
    }

    //==============================================================================
    IPlugView* PLUGIN_API createView (const char* name) override
    {
        if (auto* pluginInstance = getPluginInstance())
        {
            const auto mayCreateEditor = pluginInstance->hasEditor()
                                      && name != nullptr
                                      && std::strcmp (name, Vst::ViewType::kEditor) == 0
                                      && (pluginInstance->getActiveEditor() == nullptr
                                          || detail::PluginUtilities::getHostType().isAdobeAudition()
                                          || detail::PluginUtilities::getHostType().isPremiere());

            if (mayCreateEditor)
                return new JuceVST3Editor (*this, *audioProcessor);
        }

        return nullptr;
    }

    //==============================================================================
    void beginGesture (Vst::ParamID vstParamId)
    {
        if (! inSetState && MessageManager::getInstance()->isThisTheMessageThread())
            beginEdit (vstParamId);
    }

    void endGesture (Vst::ParamID vstParamId)
    {
        if (! inSetState && MessageManager::getInstance()->isThisTheMessageThread())
            endEdit (vstParamId);
    }

    void paramChanged (Steinberg::int32 parameterIndex, Vst::ParamID vstParamId, double newValue)
    {
        if (inParameterChangedCallback || inSetState)
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
                    if (param->updateParameterInfo())
                        flags |= Vst::kParamTitlesChanged;
        }

        if (auto* pluginInstance = getPluginInstance())
        {
            if (details.programChanged)
            {
                const auto programParameterId = audioProcessor->getProgramParamID();

                if (audioProcessor->getParamForVSTParamID (programParameterId) != nullptr)
                {
                    const auto currentProgram = pluginInstance->getCurrentProgram();
                    const auto paramValue = roundToInt (EditController::normalizedParamToPlain (programParameterId,
                                                                                                EditController::getParamNormalized (programParameterId)));

                    if (currentProgram != paramValue)
                    {
                        beginGesture (programParameterId);
                        paramChanged (audioProcessor->findCacheIndexForParamID (programParameterId),
                                      programParameterId,
                                      EditController::plainParamToNormalized (programParameterId, currentProgram));
                        endGesture (programParameterId);

                        flags |= Vst::kParamValuesChanged;
                    }
                }
            }

            auto latencySamples = pluginInstance->getLatencySamples();

           #if JucePlugin_Enable_ARA
            jassert (latencySamples == 0 || std::invoke ([&]
            {
                if (auto* extension = pluginInstance->getARAClientExtensions())
                    return ! extension->isBoundToARA();

                jassertfalse;
                return false;
            }));
           #endif

            if (details.latencyChanged && latencySamples != lastLatencySamples)
            {
                flags |= Vst::kLatencyChanged;
                lastLatencySamples = latencySamples;
            }
        }

        if (details.nonParameterStateChanged)
            flags |= pluginShouldBeMarkedDirtyFlag;

        if (inSetupProcessing)
            flags &= Vst::kLatencyChanged;

        componentRestarter.restart (flags);
    }

    //==============================================================================
    AudioProcessor* getPluginInstance() const noexcept
    {
        if (audioProcessor != nullptr)
            return audioProcessor->get();

        return nullptr;
    }

    static constexpr auto pluginShouldBeMarkedDirtyFlag = 1 << 16;

private:
    bool isBlueCatHost (FUnknown* context) const
    {
        // We can't use the normal PluginHostType mechanism here because that will give us the name
        // of the host process. However, this plugin instance might be loaded in an instance of
        // the BlueCat PatchWork host, which might itself be a plugin.

        VSTComSmartPtr<Vst::IHostApplication> host;
        host.loadFrom (context);

        if (host == nullptr)
            return false;

        Vst::String128 name;

        if (host->getName (name) != kResultOk)
            return false;

        const auto hostName = toString (name);
        return hostName.contains ("Blue Cat's VST3 Host");
    }

    friend JuceVST3Component;
    friend Param;

    //==============================================================================
    ScopedRunLoop scopedRunLoop;
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
        if ((flags & pluginShouldBeMarkedDirtyFlag) != 0)
            setDirty (true);

        flags &= ~pluginShouldBeMarkedDirtyFlag;

        if (auto* handler = componentHandler.get())
            handler->restartComponent (flags);
    }

    //==============================================================================
    struct OwnedParameterListener  final : public AudioProcessorParameter::Listener
    {
        OwnedParameterListener (JuceVST3EditController& editController,
                                AudioProcessorParameter& parameter,
                                Vst::ParamID paramID,
                                int cacheIndex)
            : owner (editController),
              vstParamID (paramID),
              parameterIndex (cacheIndex)
        {
            // We shouldn't be using an OwnedParameterListener for parameters that have
            // been added directly to the AudioProcessor. We observe those via the
            // normal audioProcessorParameterChanged mechanism.
            jassert (parameter.getParameterIndex() == -1);
            // The parameter must have a non-negative index in the parameter cache.
            jassert (parameterIndex >= 0);
            parameter.addListener (this);
        }

        void parameterValueChanged (int, float newValue) override
        {
            owner.paramChanged (parameterIndex, vstParamID, newValue);
        }

        void parameterGestureChanged (int, bool gestureIsStarting) override
        {
            if (gestureIsStarting)
                owner.beginGesture (vstParamID);
            else
                owner.endGesture (vstParamID);
        }

        JuceVST3EditController& owner;
        const Vst::ParamID vstParamID = Vst::kNoParamId;
        const int parameterIndex = -1;
    };

    std::vector<std::unique_ptr<OwnedParameterListener>> ownedParameterListeners;

    //==============================================================================
    bool inSetState = false;
    std::atomic<bool> vst3IsPlaying     { false },
                      inSetupProcessing { false };

    int lastLatencySamples = 0;
    bool blueCatPatchwork = isBlueCatHost (hostContext.get());

    detail::PluginScaleFactorManager scaleManager;

    InterfaceResultWithDeferredAddRef queryInterfaceInternal (const TUID targetIID)
    {
        const auto result = testForMultiple (*this,
                                             targetIID,
                                             UniqueBase<FObject>{},
                                             UniqueBase<JuceVST3EditController>{},
                                             UniqueBase<Vst::IEditController>{},
                                             UniqueBase<Vst::IEditController2>{},
                                             UniqueBase<Vst::IConnectionPoint>{},
                                             UniqueBase<Vst::IMidiMapping>{},
                                             UniqueBase<Vst::IUnitInfo>{},
                                             UniqueBase<Vst::IRemapParamID>{},
                                             UniqueBase<Vst::ChannelContext::IInfoListener>{},
                                             SharedBase<IPluginBase, Vst::IEditController>{},
                                             UniqueBase<IDependent>{},
                                            #if JucePlugin_Enable_ARA
                                             UniqueBase<Presonus::IPlugInViewEmbedding>{},
                                            #endif
                                             SharedBase<FUnknown, Vst::IEditController>{});

        if (result.isOk())
            return result;

        if (doUIDsMatch (targetIID, JuceAudioProcessor::iid))
            return { kResultOk, audioProcessor.get() };

        return {};
    }

    void installAudioProcessor (const VSTComSmartPtr<JuceAudioProcessor>& newAudioProcessor)
    {
        audioProcessor = newAudioProcessor;

        if (auto* extensions = audioProcessor->get()->getVST3ClientExtensions())
        {
            if (componentHandler != nullptr)
            {
                extensions->setIComponentHandler (componentHandler);
            }
            extensions->setIHostApplication (hostContext.get());
        }

        if (auto* pluginInstance = getPluginInstance())
        {
            lastLatencySamples = pluginInstance->getLatencySamples();

            pluginInstance->addListener (this);

            // as the bypass is not part of the regular parameters we need to listen for it explicitly
            if (! audioProcessor->isBypassRegularParameter())
            {
                const auto paramID = audioProcessor->getBypassParamID();
                ownedParameterListeners.push_back (std::make_unique<OwnedParameterListener> (*this,
                                                                                             *audioProcessor->getParamForVSTParamID (paramID),
                                                                                             paramID,
                                                                                             audioProcessor->findCacheIndexForParamID (paramID)));
            }

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

                const auto programParamId = audioProcessor->getProgramParamID();

                if (auto* programParam = audioProcessor->getParamForVSTParamID (programParamId))
                {
                    ownedParameterListeners.push_back (std::make_unique<OwnedParameterListener> (*this,
                                                                                                 *programParam,
                                                                                                 programParamId,
                                                                                                 audioProcessor->findCacheIndexForParamID (programParamId)));

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

        if (VSTComSmartPtr message { allocateMessage(), IncrementRef::no })
        {
            message->setMessageID (idTag);
            message->getAttributes()->setInt (idTag, value);
            sendMessage (message.get());
        }
    }

    class EditorContextMenu final : public HostProvidedContextMenu
    {
    public:
        EditorContextMenu (AudioProcessorEditor& editorIn,
                           VSTComSmartPtr<Vst::IContextMenu> contextMenuIn)
            : editor (editorIn), contextMenu (contextMenuIn) {}

        PopupMenu getEquivalentPopupMenu() const override
        {
            using MenuItem   = Vst::IContextMenuItem;
            using MenuTarget = Vst::IContextMenuTarget;

            struct Submenu
            {
                PopupMenu menu;
                String name;
                bool enabled;
            };

            std::vector<Submenu> menuStack (1);

            for (int32_t i = 0, end = contextMenu->getItemCount(); i < end; ++i)
            {
                MenuItem item{};
                MenuTarget* target = nullptr;
                contextMenu->getItem (i, item, &target);

                if ((item.flags & MenuItem::kIsGroupStart) == MenuItem::kIsGroupStart)
                {
                    menuStack.push_back ({ PopupMenu{},
                                           toString (item.name),
                                           (item.flags & MenuItem::kIsDisabled) == 0 });
                }
                else if ((item.flags & MenuItem::kIsGroupEnd) == MenuItem::kIsGroupEnd)
                {
                    const auto back = menuStack.back();
                    menuStack.pop_back();

                    if (menuStack.empty())
                    {
                        // malformed menu
                        jassertfalse;
                        return {};
                    }

                    menuStack.back().menu.addSubMenu (back.name, back.menu, back.enabled);
                }
                else if ((item.flags & MenuItem::kIsSeparator) == MenuItem::kIsSeparator)
                {
                    menuStack.back().menu.addSeparator();
                }
                else
                {
                    const auto callback = [menu = contextMenu, i]
                    {
                        MenuItem localItem{};
                        MenuTarget* localTarget = nullptr;

                        if (menu->getItem (i, localItem, &localTarget) == kResultOk && localTarget != nullptr)
                            localTarget->executeMenuItem (localItem.tag);
                    };

                    menuStack.back().menu.addItem (toString (item.name),
                                                   (item.flags & MenuItem::kIsDisabled) == 0,
                                                   (item.flags & MenuItem::kIsChecked) != 0,
                                                   callback);
                }
            }

            if (menuStack.size() != 1)
            {
                // malformed menu
                jassertfalse;
                return {};
            }

            return menuStack.back().menu;
        }

        void showNativeMenu (Point<int> pos) const override
        {
            const auto scaled = pos * Component::getApproximateScaleFactorForComponent (&editor);
            contextMenu->popup (scaled.x, scaled.y);
        }

    private:
        AudioProcessorEditor& editor;
        VSTComSmartPtr<Vst::IContextMenu> contextMenu;
    };

    class EditorHostContext final : public AudioProcessorEditorHostContext
    {
    public:
        EditorHostContext (JuceAudioProcessor& processorIn,
                           AudioProcessorEditor& editorIn,
                           Vst::IComponentHandler* handler,
                           IPlugView* viewIn)
            : processor (processorIn), editor (editorIn), componentHandler (handler), view (viewIn) {}

        std::unique_ptr<HostProvidedContextMenu> getContextMenuForParameter (const AudioProcessorParameter* parameter) const override
        {
            if (componentHandler == nullptr || view == nullptr)
                return {};

            FUnknownPtr<Vst::IComponentHandler3> handler (componentHandler);

            if (handler == nullptr)
                return {};

            const auto idToUse = parameter != nullptr ? processor.getVSTParamIDForIndex (parameter->getParameterIndex()) : 0;
            VSTComSmartPtr menu { handler->createContextMenu (view, &idToUse), IncrementRef::no };
            return std::make_unique<EditorContextMenu> (editor, menu);
        }

    private:
        JuceAudioProcessor& processor;
        AudioProcessorEditor& editor;
        Vst::IComponentHandler* componentHandler = nullptr;
        IPlugView* view = nullptr;
    };

    //==============================================================================
    class JuceVST3Editor final : public Vst::EditorView,
                                 public Vst::IParameterFinder,
                                 public IPlugViewContentScaleSupport,
                                 private Timer
    {
    public:
        JuceVST3Editor (JuceVST3EditController& ec, JuceAudioProcessor& p)
            : EditorView (&ec, nullptr),
              owner (&ec, IncrementRef::yes),
              pluginInstance (*p.get())
        {
            createContentWrapperComponentIfNeeded();

           #if JUCE_MAC
            if (detail::PluginUtilities::getHostType().type == PluginHostType::SteinbergCubase10)
                cubase10Workaround.reset (new Cubase10WindowResizeWorkaround (*this));
           #endif
        }

        ~JuceVST3Editor() override = default; // NOLINT

        tresult PLUGIN_API queryInterface (const TUID targetIID, void** obj) override
        {
            const auto result = testForMultiple (*this,
                                                 targetIID,
                                                 UniqueBase<Vst::IParameterFinder>{},
                                                 UniqueBase<IPlugViewContentScaleSupport>{});

            if (result.isOk())
                return result.extract (obj);

            return Vst::EditorView::queryInterface (targetIID, obj);
        }

        // NOLINTBEGIN
        REFCOUNT_METHODS (Vst::EditorView)
        // NOLINTEND

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

            viewRunLoop.emplace (ScopedRunLoop::getRunLoopFromFrame (plugFrame));

            systemWindow = parent;

           #if JUCE_WINDOWS && JUCE_WIN_PER_MONITOR_DPI_AWARE
            // Hosts may report the content scale either before or after attaching the view, so
            // we need to record the DPI both here and in setContentScaleFactor()
            if (owner->scaleManager.getHostScale().has_value())
                dpiWhenHostScaleReported = getDpiOfDisplayUnderWindow (systemWindow);
           #endif

            createContentWrapperComponentIfNeeded();

            const auto [desktopFlags, windowsMultiTouch] = detail::PluginUtilities::getDesktopFlagsAndWindowsMultiTouchMode (component->pluginEditor.get());

           #if JUCE_WINDOWS || JUCE_LINUX || JUCE_BSD
            component->setOpaque (true);
            component->addToDesktop (desktopFlags, systemWindow);

            if (auto* peer = component->getPeer())
                peer->setWindowsCanUseMultiTouch (windowsMultiTouch);

            component->setVisible (true);
           #else
            macHostWindow = detail::VSTWindowUtilities::attachComponentToWindowRefVST (component.get(), desktopFlags, parent);
           #endif

            component->resizeHostWindow();
            attachedToParent();

            // Life's too short to faff around with wave lab
            if (detail::PluginUtilities::getHostType().isWavelab())
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
                    detail::VSTWindowUtilities::detachComponentFromWindowRefVST (component.get(), macHostWindow);
                    macHostWindow = nullptr;
                }
               #endif

                component = nullptr;
                lastReportedSize.reset();
            }

            viewRunLoop.reset();

            return CPluginView::removed();
        }

        tresult PLUGIN_API onSize (ViewRect* newSize) override
        {
            if (newSize == nullptr)
            {
                jassertfalse;
                return kResultFalse;
            }

            lastReportedSize.reset();

            if (component == nullptr)
                return kResultTrue;

           #if JUCE_WINDOWS && JUCE_WIN_PER_MONITOR_DPI_AWARE
            // A per-monitor DPI aware v1 host such as REAPER resizes a resizable view for a new
            // display before it reports the new content scale. If the host has resized the view by
            // the ratio of the two DPIs, the new scale is adopted now.
            if (const auto hostScale = owner->scaleManager.getHostScale(); hostScale.has_value() && 0.0 < dpiWhenHostScaleReported)
            {
                if (const auto dpi = getDpiOfDisplayUnderWindow (systemWindow); 0.0 < dpi && ! approximatelyEqual (dpi, dpiWhenHostScaleReported))
                {
                    const auto dpiRatio = dpi / dpiWhenHostScaleReported;
                    const auto requestedHostSize = createRectangle (*newSize).toFloat();
                    const auto previousHostSize = owner->scaleManager.convertToHostBounds (component->getSizeToContainChild().toFloat()).toFloat();

                    const auto distanceFromRequestedHostSize = [&] (Rectangle<float> r)
                    {
                        return std::abs (requestedHostSize.getWidth() - r.getWidth()) + std::abs (requestedHostSize.getHeight() - r.getHeight());
                    };

                    if (distanceFromRequestedHostSize (previousHostSize * (float) dpiRatio) < distanceFromRequestedHostSize (previousHostSize))
                    {
                        dpiWhenHostScaleReported = dpi;
                        owner->scaleManager.setHostScale (*hostScale * (float) dpiRatio);
                    }
                }
            }
           #endif

            const auto rounded = owner->scaleManager.convertFromHostBounds (createRectangle (*newSize)).toNearestIntEdges();
            component->onSize (rounded.getWidth(), rounded.getHeight());

           #if JUCE_MAC
            if (cubase10Workaround != nullptr)
            {
                cubase10Workaround->triggerAsyncUpdate();
            }
            else
           #endif
            {
                if (auto* peer = component->getPeer())
                {
                    peer->setBounds ((Rectangle { newSize->getWidth(), newSize->getHeight() }.toFloat()
                                         / peer->getPlatformScaleFactor()).toNearestInt(),
                                     false);
                }
            }

            return kResultTrue;
        }

        tresult PLUGIN_API getSize (ViewRect* size) override
        {
           #if JUCE_WINDOWS && JUCE_WIN_PER_MONITOR_DPI_AWARE
            if (detail::PluginUtilities::getHostType().isAbletonLive() && systemWindow == nullptr)
                return kResultFalse;
           #endif

            if (size == nullptr || component == nullptr || component->pluginEditor == nullptr)
                return kResultFalse;

            const auto editorBounds = component->getSizeToContainChild();

            lastReportedSize = *size = lastReportedSize.value_or (createViewRect (owner->scaleManager.convertToHostBounds (editorBounds.withZeroOrigin().toFloat())));
            return kResultTrue;
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
                    if (canResize() == kResultFalse)
                    {
                        // Ableton Live will call checkSizeConstraint even if the view returns false
                        // from canResize. Set the out param to an appropriate size for the editor
                        // and return.
                        auto constrainedRect = component->getLocalArea (editor, editor->getLocalBounds())
                                                        .getSmallestIntegerContainer();

                        *rectToCheck = createViewRect (owner->scaleManager.convertFromHostBounds (createRectangle (*rectToCheck)).toNearestIntEdges());
                        rectToCheck->right  = rectToCheck->left + roundToInt (constrainedRect.getWidth());
                        rectToCheck->bottom = rectToCheck->top  + roundToInt (constrainedRect.getHeight());
                        *rectToCheck = createViewRect (owner->scaleManager.convertToHostBounds (createRectangle (*rectToCheck).toFloat()));
                    }
                    else if (auto* constrainer = editor->getConstrainer())
                    {
                        const auto clientBounds = owner->scaleManager.convertFromHostBounds (createRectangle (*rectToCheck));
                        const auto editorBounds = editor->getLocalArea (component.get(), clientBounds);

                        auto minW = (float) constrainer->getMinimumWidth();
                        auto maxW = (float) constrainer->getMaximumWidth();
                        auto minH = (float) constrainer->getMinimumHeight();
                        auto maxH = (float) constrainer->getMaximumHeight();

                        auto width  = jlimit (minW, maxW, editorBounds.getWidth());
                        auto height = jlimit (minH, maxH, editorBounds.getHeight());

                        auto aspectRatio = (float) constrainer->getFixedAspectRatio();

                        if (! approximatelyEqual (aspectRatio, 0.0f))
                        {
                            bool adjustWidth = (width / height > aspectRatio);

                            if (detail::PluginUtilities::getHostType().type == PluginHostType::SteinbergCubase9)
                            {
                                auto currentEditorBounds = editor->getBounds().toFloat();

                                if (approximatelyEqual (currentEditorBounds.getWidth(), width) && ! approximatelyEqual (currentEditorBounds.getHeight(), height))
                                    adjustWidth = true;
                                else if (approximatelyEqual (currentEditorBounds.getHeight(), height) && ! approximatelyEqual (currentEditorBounds.getWidth(), width))
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

                        auto constrainedRect = component->getLocalArea (editor, Rectangle<float> (width, height));

                        *rectToCheck = createViewRect (owner->scaleManager.convertToHostBounds (clientBounds.withWidth (constrainedRect.getWidth())
                                                                                                            .withHeight (constrainedRect.getHeight())));
                    }
                }

                return kResultTrue;
            }

            jassertfalse;
            return kResultFalse;
        }

        tresult PLUGIN_API setContentScaleFactor ([[maybe_unused]] const IPlugViewContentScaleSupport::ScaleFactor factor) override
        {
           #if ! JUCE_MAC
            const auto scaleToApply = std::invoke ([&]
            {
               #if JUCE_WINDOWS && JUCE_WIN_PER_MONITOR_DPI_AWARE
                // Cubase 10 only sends integer scale factors, so correct this for fractional scales
                if (detail::PluginUtilities::getHostType().type != PluginHostType::SteinbergCubase10)
                    return factor;

                const auto hostWindowScale = (decltype (factor)) detail::PluginScaleFactorManager::getScaleFactorForWindow (static_cast<HWND> (systemWindow));

                if (hostWindowScale <= 0.0 || approximatelyEqual (factor, hostWindowScale))
                    return factor;

                return hostWindowScale;
               #else
                return factor;
               #endif
            });

           #if JUCE_WINDOWS && JUCE_WIN_PER_MONITOR_DPI_AWARE
            dpiWhenHostScaleReported = getDpiOfDisplayUnderWindow (systemWindow);
           #endif

            owner->scaleManager.setHostScale (scaleToApply);

            return kResultTrue;
           #else
            return kResultFalse;
           #endif
        }

        tresult PLUGIN_API findParameter (int32 xPos, int32 yPos, Vst::ParamID& resultTag) override
        {
            if (const auto paramId = findParameterImpl (xPos, yPos))
            {
                resultTag = *paramId;
                return kResultTrue;
            }

            return kResultFalse;
        }

    private:
        std::optional<Vst::ParamID> findParameterImpl (int32 xPos, int32 yPos) const
        {
            auto* wrapper = component.get();

            if (wrapper == nullptr)
                return {};

            auto* componentAtPosition = wrapper->getComponentAt (xPos, yPos);

            if (componentAtPosition == nullptr)
                return {};

            auto* editor = wrapper->pluginEditor.get();

            if (editor == nullptr)
                return {};

            const auto parameterIndex = editor->getControlParameterIndex (*componentAtPosition);

            if (parameterIndex < 0)
                return {};

            auto processor = owner->audioProcessor;

            if (processor == nullptr)
                return {};

            return processor->getVSTParamIDForIndex (parameterIndex);
        }

        void timerCallback() override
        {
            stopTimer();

            ViewRect viewRect;
            getSize (&viewRect);
            onSize (&viewRect);
        }

        static ViewRect createViewRect (Rectangle<int> r)
        {
            return { r.getX(), r.getY(), r.getRight(), r.getBottom() };
        }

        static Rectangle<int> createRectangle (ViewRect viewRect)
        {
            return Rectangle<int>::leftTopRightBottom ((int) viewRect.left,
                                                       (int) viewRect.top,
                                                       (int) viewRect.right,
                                                       (int) viewRect.bottom);
        }

        //==============================================================================
        struct ContentWrapperComponent final : public Component,
                                               private detail::PluginScaleFactorManagerListener
        {
            ContentWrapperComponent (JuceVST3Editor& editor)  : owner (editor)
            {
                setOpaque (true);
                setBroughtToFrontOnMouseClick (true);
                owner.owner->scaleManager.addListener (*this);
                owner.owner->scaleManager.startObserving (*this);
            }

            ~ContentWrapperComponent() override
            {
                owner.owner->scaleManager.stopObserving (*this);
                owner.owner->scaleManager.removeListener (*this);

                if (pluginEditor != nullptr)
                {
                    PopupMenu::dismissAllActiveMenus();
                    pluginEditor->processor.editorBeingDeleted (pluginEditor.get());
                }
            }

            void createEditor (AudioProcessor& plugin)
            {
                pluginEditor.reset (plugin.createEditorAndMakeActive());

               #if JucePlugin_Enable_ARA
                jassert (pluginEditor->getARAClientExtensions() != nullptr);
                // for proper view embedding, ARA plug-ins must be resizable
                jassert (pluginEditor->isResizable());
               #endif

                if (pluginEditor != nullptr)
                {
                    editorHostContext = std::make_unique<EditorHostContext> (*owner.owner->audioProcessor,
                                                                             *pluginEditor,
                                                                             owner.owner->getComponentHandler(),
                                                                             &owner);

                    pluginEditor->setHostContext (editorHostContext.get());

                    addAndMakeVisible (pluginEditor.get());
                    pluginEditor->setTopLeftPosition (0, 0);

                    setBounds (getSizeToContainChild());

                    resizeHostWindow();
                }
                else
                {
                    // if hasEditor() returns true then createEditor() has to return a valid editor
                    jassertfalse;
                }
            }

            void paint (Graphics& g) override
            {
                g.fillAll (Colours::black);
            }

            Rectangle<int> getSizeToContainChild()
            {
                if (pluginEditor != nullptr)
                    return getLocalArea (pluginEditor.get(), pluginEditor->getLocalBounds());

                return {};
            }

            void onSize (int w, int h)
            {
                const ScopedValueSetter resizingChildSetter (resizingChild, true);

                if (pluginEditor != nullptr)
                {
                    const auto editorArea = pluginEditor->getLocalArea (this, Rectangle { w, h });
                    pluginEditor->setBoundsConstrained (editorArea.withZeroOrigin());
                }
            }

            void childBoundsChanged (Component*) override
            {
                if (resizingChild)
                    return;

                resizeHostWindow();

               #if JUCE_LINUX || JUCE_BSD
                if (detail::PluginUtilities::getHostType().isBitwigStudio())
                    repaint();
               #endif
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
                        auto newSize = owner.createViewRect (owner.owner->scaleManager.convertToHostBounds (editorBounds.withZeroOrigin().toFloat()));

                        owner.plugFrame->resizeView (&owner, &newSize);
                        auto host = detail::PluginUtilities::getHostType();

                       #if JUCE_MAC
                        if (host.isWavelab() || host.isReaper() || owner.owner->blueCatPatchwork)
                       #else
                        if (host.isWavelab() || host.isAbletonLive() || host.isBitwigStudio() || owner.owner->blueCatPatchwork)
                       #endif
                            setBounds (editorBounds.withZeroOrigin());
                    }
                }
            }

            std::unique_ptr<AudioProcessorEditor> pluginEditor;

        private:
            void peerBoundsDidUpdate() override
            {
                resizeHostWindow();
            }

            JuceVST3Editor& owner;
            std::unique_ptr<EditorHostContext> editorHostContext;
            bool resizingChild = false;

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

       #if JUCE_WINDOWS && JUCE_WIN_PER_MONITOR_DPI_AWARE
        static double getDpiOfDisplayUnderWindow (void* window)
        {
            if (window == nullptr)
                return 0.0;

            const auto hwnd = static_cast<HWND> (window);
            const auto root = GetAncestor (hwnd, GA_ROOT);

            RECT rootRect{};

            if (! GetWindowRect (root != nullptr ? root : hwnd, &rootRect))
                return 0.0;

            const auto rootBounds = Rectangle<int>::leftTopRightBottom (rootRect.left, rootRect.top, rootRect.right, rootRect.bottom);

            if (auto* display = Desktop::getInstance().getDisplays().getDisplayForRect (rootBounds, true))
                return display->dpi;

            return 0.0;
        }
       #endif

        //==============================================================================
        std::optional<ScopedRunLoop> viewRunLoop;
        std::optional<ViewRect> lastReportedSize;

       #if JUCE_WINDOWS && JUCE_WIN_PER_MONITOR_DPI_AWARE
        double dpiWhenHostScaleReported = 0.0;
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

        friend ContentWrapperComponent;

       #if JUCE_MAC
        void* macHostWindow = nullptr;

        // On macOS Cubase 10 resizes the host window after calling onSize() resulting in the peer
        // bounds being a step behind the plug-in. Calling updateBounds() asynchronously seems to fix things...
        struct Cubase10WindowResizeWorkaround final : public AsyncUpdater
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
       #elif JUCE_WINDOWS
        detail::WindowsHooks hooks;
       #endif

        //==============================================================================
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JuceVST3Editor)
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JuceVST3EditController)
};


//==============================================================================
#if JucePlugin_Enable_ARA
 class JuceARAFactory final : public ARA::IMainFactory
 {
 public:
    JuceARAFactory() = default;

    JUCE_DECLARE_VST3_COM_REF_METHODS

    tresult PLUGIN_API queryInterface (const ::Steinberg::TUID targetIID, void** obj) override
    {
        const auto result = testForMultiple (*this,
                                             targetIID,
                                             UniqueBase<ARA::IMainFactory>{},
                                             UniqueBase<FUnknown>{});

        if (result.isOk())
            return result.extract (obj);

        if (doUIDsMatch (targetIID, JuceARAFactory::iid))
        {
            addRef();
            *obj = this;
            return kResultOk;
        }

        *obj = nullptr;
        return kNoInterface;
    }

    //---from ARA::IMainFactory-------
    const ARA::ARAFactory* PLUGIN_API getFactory() SMTG_OVERRIDE
    {
        return createARAFactory();
    }

    inline static const FUID iid = toSteinbergUID (getVST3InterfaceId (VST3Interface::Type::ara));

 private:
     //==============================================================================
     std::atomic<int> refCount { 1 };
 };
#endif

//==============================================================================
class JuceVST3Component final : public Vst::IComponent,
                                public Vst::IAudioProcessor,
                                public Vst::IUnitInfo,
                                public Vst::IConnectionPoint,
                                public Vst::IProcessContextRequirements,
                               #if JucePlugin_Enable_ARA
                                public ARA::IPlugInEntryPoint,
                                public ARA::IPlugInEntryPoint2,
                               #endif
                                public AudioPlayHead
{
public:
    JuceVST3Component (const VSTComSmartPtr<Vst::IHostApplication>& h,
                       const RunLoop& l)
        : scopedRunLoop (l),
          pluginInstance (createPluginFilterOfType (AudioProcessor::wrapperType_VST3).release()),
          host (h)
    {
        inParameterChangedCallback = false;

       #ifdef JucePlugin_PreferredChannelConfigurations
        short configs[][2] = { JucePlugin_PreferredChannelConfigurations };
        [[maybe_unused]] const int numConfigs = numElementsInArray (configs);

        jassert (numConfigs > 0 && (configs[0][0] > 0 || configs[0][1] > 0));

        pluginInstance->setPlayConfigDetails (configs[0][0], configs[0][1], 44100.0, 1024);
       #endif

        // VST-3 requires your default layout to be non-discrete!
        // For example, your default layout must be mono, stereo, quadrophonic
        // and not AudioChannelSet::discreteChannels (2) etc.
        jassert (checkBusFormatsAreNotDiscrete());

        comPluginInstance = VSTComSmartPtr (new JuceAudioProcessor (pluginInstance), IncrementRef::yes);

        zerostruct (processContext);

        processSetup.maxSamplesPerBlock = 1024;
        processSetup.processMode = Vst::kRealtime;
        processSetup.sampleRate = 44100.0;
        processSetup.symbolicSampleSize = Vst::kSample32;

        pluginInstance->setPlayHead (this);

        // Constructing the underlying static object involves dynamic allocation.
        // This call ensures that the construction won't happen on the audio thread.
        detail::PluginUtilities::getHostType();
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
    inline static const FUID iid = toSteinbergUID (getVST3InterfaceId (VST3Interface::Type::component));

    JUCE_DECLARE_VST3_COM_REF_METHODS

    tresult PLUGIN_API queryInterface (const TUID targetIID, void** obj) override
    {
        const auto userProvidedInterface = queryAdditionalInterfaces (&getPluginInstance(),
                                                                      targetIID,
                                                                      &VST3ClientExtensions::queryIAudioProcessor);

        const auto juceProvidedInterface = queryInterfaceInternal (targetIID);

        return extractResult (userProvidedInterface, juceProvidedInterface, obj);
    }

    enum class CallPrepareToPlay { no, yes };

    //==============================================================================
    tresult PLUGIN_API initialize (FUnknown* hostContext) override
    {
        if (host.get() != hostContext)
            host.loadFrom (hostContext);

        processContext.sampleRate = processSetup.sampleRate;
        preparePlugin (processSetup.sampleRate, (int) processSetup.maxSamplesPerBlock, CallPrepareToPlay::no);

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
                juceVST3EditController = VSTComSmartPtr ((JuceVST3EditController*) (pointer_sized_int) value,
                                                         IncrementRef::yes);

                if (juceVST3EditController != nullptr)
                    juceVST3EditController->setAudioProcessor (comPluginInstance.get());
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
        const FLStudioDIYSpecificationEnforcementLock lock (flStudioDIYSpecificationEnforcementMutex);

        const auto willBeActive = (state != 0);

        active = false;
        // Some hosts may call setBusArrangements in response to calls made during prepareToPlay
        // or releaseResources. Specifically, Wavelab 11.1 calls setBusArrangements in the same
        // call stack when the AudioProcessor calls setLatencySamples inside prepareToPlay.
        // In order for setBusArrangements to return successfully, the plugin must not be activated
        // until after prepareToPlay has completely finished.
        const ScopeGuard scope { [&] { active = willBeActive; } };

        if (willBeActive)
        {
            const auto sampleRate = processSetup.sampleRate > 0.0
                                  ? processSetup.sampleRate
                                  : getPluginInstance().getSampleRate();

            const auto bufferSize = processSetup.maxSamplesPerBlock > 0
                                  ? (int) processSetup.maxSamplesPerBlock
                                  : getPluginInstance().getBlockSize();

            preparePlugin (sampleRate, bufferSize, CallPrepareToPlay::yes);
        }
        else
        {
            getPluginInstance().releaseResources();
        }

        return kResultOk;
    }

    tresult PLUGIN_API setIoMode (Vst::IoMode) override                                 { return kNotImplemented; }
    tresult PLUGIN_API getRoutingInfo (Vst::RoutingInfo&, Vst::RoutingInfo&) override   { return kNotImplemented; }

    //==============================================================================
    bool isBypassed() const
    {
        if (auto* bypassParam = comPluginInstance->getBypassParameter())
            return bypassParam->getValue() >= 0.5f;

        return false;
    }

    void setBypassed (bool shouldBeBypassed)
    {
        if (auto* bypassParam = comPluginInstance->getBypassParameter())
            setValueAndNotifyIfChanged (*bypassParam, shouldBeBypassed ? 1.0f : 0.0f);
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
        bool unusedState = false;
        auto& flagToSet = juceVST3EditController != nullptr ? juceVST3EditController->inSetState : unusedState;
        const ScopedValueSetter<bool> scope (flagToSet, true);

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
    bool shouldTryToLoadVst2State()
    {
        return ! getAllVST3CompatibleClasses().empty();
    }

    bool shouldWriteStateWithVst2Compatibility()
    {
       #if JUCE_VST3_CAN_REPLACE_VST2
        return true;
       #else
        return false;
       #endif
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
            if (detail::PluginUtilities::getHostType().isAdobeAudition())
                if (block.getSize() >= 5 && memcmp (block.getData(), "VC2!E", 5) == 0)
                    return false;

            setStateInformation (block.getData(), (int) block.getSize());
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

                if (bytesRead <= 0 || (status != kResultTrue && ! detail::PluginUtilities::getHostType().isWavelab()))
                    break;

                allData.write (buffer, static_cast<size_t> (bytesRead));
            }
        }

        const size_t dataSize = allData.getDataSize();

        if (dataSize <= 0 || dataSize >= 0x7fffffff)
            return false;

        setStateInformation (allData.getData(), (int) dataSize);
        return true;
    }

    bool readVst2State (IBStream* state)
    {
        if (auto vst2State = VST3::tryVst2StateLoad (*state))
        {
            setStateInformation (vst2State->chunk.data(), (int) vst2State->chunk.size());
            return true;
        }

        return false;
    }

    tresult PLUGIN_API setState (IBStream* state) override
    {
        // The VST3 spec requires that this function is called from the UI thread.
        // If this assertion fires, your host is misbehaving!
        assertHostMessageThread();

        if (state == nullptr)
            return kInvalidArgument;

        FUnknownPtr<IBStream> stateRefHolder (state); // just in case the caller hasn't properly ref-counted the stream object

        const auto seekToBeginningOfStream = [&]
        {
            return state->seek (0, IBStream::kIBSeekSet, nullptr) == kResultTrue;
        };

        if (seekToBeginningOfStream() && shouldTryToLoadVst2State() && readVst2State (state))
            return kResultTrue;

        if (seekToBeginningOfStream() && ! detail::PluginUtilities::getHostType().isFruityLoops() && readFromMemoryStream (state))
            return kResultTrue;

        if (seekToBeginningOfStream() && readFromUnknownStream (state))
            return kResultTrue;

        return kResultFalse;
    }

    tresult getStateWithVst2Compatibility (const MemoryBlock& dataChunk, IBStream& outState)
    {
        VST3::Vst2xState vst2State;

        vst2State.chunk.resize (dataChunk.getSize());
        std::copy (dataChunk.begin(), dataChunk.end(), vst2State.chunk.begin());

        vst2State.fxUniqueID = JucePlugin_VSTUniqueID;
        vst2State.fxVersion = JucePlugin_VersionCode;
        vst2State.isBypassed = isBypassed();

        if (VST3::writeVst2State (vst2State, outState))
            return kResultTrue;

        // Please inform the JUCE team if you hit this assertion
        jassertfalse;
        return kResultFalse;
    }

    tresult PLUGIN_API getState (IBStream* state) override
    {
       if (state == nullptr)
           return kInvalidArgument;

        MemoryBlock mem;
        getStateInformation (mem);

        if (mem.isEmpty())
            return kResultFalse;

        if (shouldWriteStateWithVst2Compatibility())
            return getStateWithVst2Compatibility (mem, *state);

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
                                           IBStream* data) override                                                             { return comPluginInstance->setUnitProgramData (listOrUnitId, programIndex, data); }
    Vst::UnitID PLUGIN_API getSelectedUnit() override                                                                           { return comPluginInstance->getSelectedUnit(); }
    tresult PLUGIN_API getUnitByBus (Vst::MediaType type, Vst::BusDirection dir, Steinberg::int32 busIndex,
                                     Steinberg::int32 channel, Vst::UnitID& unitId) override                                    { return comPluginInstance->getUnitByBus (type, dir, busIndex, channel, unitId); }

    //==============================================================================
    Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setTimeInSamples (processContext.projectTimeSamples);
        info.setTimeInSeconds (static_cast<double> (*info.getTimeInSamples()) / processContext.sampleRate);
        info.setIsRecording ((processContext.state & Vst::ProcessContext::kRecording) != 0);
        info.setIsPlaying ((processContext.state & Vst::ProcessContext::kPlaying) != 0);
        info.setIsLooping ((processContext.state & Vst::ProcessContext::kCycleActive) != 0);

        info.setBpm ((processContext.state & Vst::ProcessContext::kTempoValid) != 0
                     ? makeOptional (processContext.tempo)
                     : nullopt);

        info.setTimeSignature ((processContext.state & Vst::ProcessContext::kTimeSigValid) != 0
                               ? makeOptional (TimeSignature { processContext.timeSigNumerator, processContext.timeSigDenominator })
                               : nullopt);

        info.setLoopPoints ((processContext.state & Vst::ProcessContext::kCycleValid) != 0
                            ? makeOptional (LoopPoints { processContext.cycleStartMusic, processContext.cycleEndMusic })
                            : nullopt);

        info.setPpqPosition ((processContext.state & Vst::ProcessContext::kProjectTimeMusicValid) != 0
                             ? makeOptional (processContext.projectTimeMusic)
                             : nullopt);

        info.setPpqPositionOfLastBarStart ((processContext.state & Vst::ProcessContext::kBarPositionValid) != 0
                                           ? makeOptional (processContext.barPositionMusic)
                                           : nullopt);

        info.setFrameRate ((processContext.state & Vst::ProcessContext::kSmpteValid) != 0
                           ? makeOptional (FrameRate().withBaseRate ((int) processContext.frameRate.framesPerSecond)
                                                      .withDrop ((processContext.frameRate.flags & Vst::FrameRate::kDropRate) != 0)
                                                      .withPullDown ((processContext.frameRate.flags & Vst::FrameRate::kPullDownRate) != 0))
                           : nullopt);

        info.setEditOriginTime (info.getFrameRate().hasValue()
                                ? makeOptional ((double) processContext.smpteOffsetSubframes / (80.0 * info.getFrameRate()->getEffectiveRate()))
                                : nullopt);

        info.setHostTimeNs ((processContext.state & Vst::ProcessContext::kSystemTimeValid) != 0
                            ? makeOptional ((uint64_t) processContext.systemTime)
                            : nullopt);

        return info;
    }

    //==============================================================================
    int getNumAudioBuses (bool isInput) const
    {
        int busCount = pluginInstance->getBusCount (isInput);

      #ifdef JucePlugin_PreferredChannelConfigurations
        short configs[][2] = {JucePlugin_PreferredChannelConfigurations};
        const int numConfigs = numElementsInArray (configs);

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

                [[maybe_unused]] const auto lastEnabledVst3Layout = getVst3SpeakerArrangement (bus->getLastEnabledLayout());
                jassert (lastEnabledVst3Layout.has_value() && info.channelCount == Vst::SpeakerArr::getChannelCount (*lastEnabledVst3Layout));
                toString128 (info.name, bus->getName());

                info.busType = [&]
                {
                    const auto isFirstBus = (index == 0);

                    if (dir == Vst::kInput)
                    {
                        if (isFirstBus)
                        {
                            if (auto* extensions = pluginInstance->getVST3ClientExtensions())
                                return extensions->getPluginHasMainInput() ? Vst::kMain : Vst::kAux;

                            return Vst::kMain;
                        }

                        return Vst::kAux;
                    }

                   #if JucePlugin_IsSynth
                    return Vst::kMain;
                   #else
                    return isFirstBus ? Vst::kMain : Vst::kAux;
                   #endif
                }();

               #ifdef JucePlugin_PreferredChannelConfigurations
                info.flags = Vst::BusInfo::kDefaultActive;
               #else
                info.flags = (bus->isEnabledByDefault()) ? Vst::BusInfo::kDefaultActive : 0;
               #endif

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

                toString128 (info.name, TRANS ("MIDI Input"));
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

                toString128 (info.name, TRANS ("MIDI Output"));
                info.busType = Vst::kMain;
                return kResultTrue;
            }
           #endif
        }

        zerostruct (info);
        return kResultFalse;
    }

    tresult PLUGIN_API activateBus (Vst::MediaType type,
                                    Vst::BusDirection dir,
                                    Steinberg::int32 index,
                                    TBool state) override
    {
        const FLStudioDIYSpecificationEnforcementLock lock (flStudioDIYSpecificationEnforcementMutex);

        // The host is misbehaving! The plugin must be deactivated before setting new arrangements.
        jassert (! active);

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
            const auto numPublicInputBuses  = getNumAudioBuses (true);
            const auto numPublicOutputBuses = getNumAudioBuses (false);

            if (! isPositiveAndBelow (index, dir == Vst::kInput ? numPublicInputBuses : numPublicOutputBuses))
                return kResultFalse;

            // The host is allowed to enable/disable buses as it sees fit, so the plugin needs to be
            // able to handle any set of enabled/disabled buses, including layouts for which
            // AudioProcessor::isBusesLayoutSupported would return false.
            // Our strategy is to keep track of the layout that the host last requested, and to
            // attempt to apply that layout directly.
            // If the layout isn't supported by the processor, we'll try enabling all the buses
            // instead.
            // If the host enables a bus that the processor refused to enable, then we'll ignore
            // that bus (and return silence for output buses). If the host disables a bus that the
            // processor refuses to disable, the wrapper will provide the processor with silence for
            // input buses, and ignore the contents of output buses.
            // Note that some hosts (old bitwig and cakewalk) may incorrectly call this function
            // when the plugin is in an activated state.
            if (dir == Vst::kInput)
                bufferMapper.setInputBusHostActive ((size_t) index, state != 0);
            else
                bufferMapper.setOutputBusHostActive ((size_t) index, state != 0);

            AudioProcessor::BusesLayout desiredLayout;

            for (const auto isInput : { true, false })
            {
                const auto numPublicBuses = isInput ? numPublicInputBuses : numPublicOutputBuses;
                auto& layoutBuses = isInput ? desiredLayout.inputBuses : desiredLayout.outputBuses;

                for (auto i = 0; i < numPublicBuses; ++i)
                {
                    layoutBuses.add (isInput ? bufferMapper.getRequestedLayoutForInputBus ((size_t) i)
                                             : bufferMapper.getRequestedLayoutForOutputBus ((size_t) i));
                }

                while (layoutBuses.size() < pluginInstance->getBusCount (isInput))
                    layoutBuses.add (AudioChannelSet::disabled());
            }

            const auto prev = pluginInstance->getBusesLayout();

            const auto busesLayoutSupported = [&]
            {
               #ifdef JucePlugin_PreferredChannelConfigurations
                struct ChannelPair
                {
                    short ins, outs;

                    auto tie() const { return std::tie (ins, outs); }
                    bool operator== (ChannelPair x) const { return tie() == x.tie(); }
                };

                const auto countChannels = [] (auto& range)
                {
                    return std::accumulate (range.begin(), range.end(), 0, [] (auto acc, auto set)
                    {
                        return acc + set.size();
                    });
                };

                const auto toShort = [] (int x)
                {
                    jassert (0 <= x && x <= std::numeric_limits<short>::max());
                    return (short) x;
                };

                const ChannelPair requested { toShort (countChannels (desiredLayout.inputBuses)),
                                              toShort (countChannels (desiredLayout.outputBuses)) };
                const ChannelPair configs[] = { JucePlugin_PreferredChannelConfigurations };
                return std::find (std::begin (configs), std::end (configs), requested) != std::end (configs);
               #else
                return pluginInstance->checkBusesLayoutSupported (desiredLayout);
               #endif
            }();

            if (busesLayoutSupported)
                pluginInstance->setBusesLayout (desiredLayout);
            else
                pluginInstance->enableAllBuses();

            bufferMapper.updateActiveClientBuses (pluginInstance->getBusesLayout());

            return kResultTrue;
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
        const FLStudioDIYSpecificationEnforcementLock lock (flStudioDIYSpecificationEnforcementMutex);

        if (active)
        {
            // The host is misbehaving! The plugin must be deactivated before setting new arrangements.
            jassertfalse;
            return kResultFalse;
        }

        auto numInputBuses  = pluginInstance->getBusCount (true);
        auto numOutputBuses = pluginInstance->getBusCount (false);

        if (numIns > numInputBuses || numOuts > numOutputBuses)
            return kResultFalse;

        // see the following documentation to understand the correct way to react to this callback
        // https://steinbergmedia.github.io/vst3_doc/vstinterfaces/classSteinberg_1_1Vst_1_1IAudioProcessor.html#ad3bc7bac3fd3b194122669be2a1ecc42

        const auto toLayoutsArray = [] (auto begin, auto end) -> std::optional<Array<AudioChannelSet>>
        {
            Array<AudioChannelSet> result;

            for (auto it = begin; it != end; ++it)
            {
                const auto set = getChannelSetForSpeakerArrangement (*it);

                if (! set.has_value())
                    return {};

                result.add (*set);
            }

            return result;
        };

        const auto optionalRequestedLayout = [&]() -> std::optional<AudioProcessor::BusesLayout>
        {
            const auto ins  = toLayoutsArray (inputs,  inputs  + numIns);
            const auto outs = toLayoutsArray (outputs, outputs + numOuts);

            if (! ins.has_value() || ! outs.has_value())
                return {};

            AudioProcessor::BusesLayout result;
            result.inputBuses  = *ins;
            result.outputBuses = *outs;
            return result;
        }();

        if (! optionalRequestedLayout.has_value())
            return kResultFalse;

        const auto& requestedLayout = *optionalRequestedLayout;

       #ifdef JucePlugin_PreferredChannelConfigurations
        short configs[][2] = { JucePlugin_PreferredChannelConfigurations };
        if (! AudioProcessor::containsLayout (requestedLayout, configs))
            return kResultFalse;
       #endif

        if (pluginInstance->checkBusesLayoutSupported (requestedLayout))
        {
            if (! pluginInstance->setBusesLayoutWithoutEnabling (requestedLayout))
                return kResultFalse;

            bufferMapper.updateFromProcessor (*pluginInstance);
            return kResultTrue;
        }

        // apply layout changes in reverse order as Steinberg says we should prioritize main buses
        const auto nextBest = [this, numInputBuses, numOutputBuses, &requestedLayout]
        {
            auto layout = pluginInstance->getBusesLayout();

            for (auto busIdx = jmax (numInputBuses, numOutputBuses) - 1; busIdx >= 0; --busIdx)
                for (const auto isInput : { true, false })
                    if (auto* bus = pluginInstance->getBus (isInput, busIdx))
                        bus->isLayoutSupported (requestedLayout.getChannelSet (isInput, busIdx), &layout);

            return layout;
        }();

        if (pluginInstance->setBusesLayoutWithoutEnabling (nextBest))
            bufferMapper.updateFromProcessor (*pluginInstance);

        return kResultFalse;
    }

    tresult PLUGIN_API getBusArrangement (Vst::BusDirection dir, Steinberg::int32 index, Vst::SpeakerArrangement& arr) override
    {
        if (auto* bus = pluginInstance->getBus (dir == Vst::kInput, index))
        {
            if (const auto arrangement = getVst3SpeakerArrangement (bus->getLastEnabledLayout()))
            {
                arr = *arrangement;
                return kResultTrue;
            }

            // There's a bus here, but we can't represent its layout in terms of VST3 speakers!
            jassertfalse;
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
        ScopedInSetupProcessingSetter inSetupProcessingSetter (juceVST3EditController.get());

        if (canProcessSampleSize (newSetup.symbolicSampleSize) != kResultTrue)
            return kResultFalse;

        processSetup = newSetup;
        processContext.sampleRate = processSetup.sampleRate;

        getPluginInstance().setProcessingPrecision (newSetup.symbolicSampleSize == Vst::kSample64
                                                        ? AudioProcessor::doublePrecision
                                                        : AudioProcessor::singlePrecision);
        getPluginInstance().setNonRealtime (newSetup.processMode == Vst::kOffline);

        preparePlugin (processSetup.sampleRate, processSetup.maxSamplesPerBlock, CallPrepareToPlay::no);

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

        if (std::isinf (tailLengthSeconds))
            return Vst::kInfiniteTail;

        return (Steinberg::uint32) roundToIntAccurate (tailLengthSeconds * processSetup.sampleRate);
    }

    //==============================================================================
    void processParameterChanges (Vst::IParameterChanges& paramChanges)
    {
        jassert (pluginInstance != nullptr);

        struct ParamChangeInfo
        {
            Steinberg::int32 offsetSamples = 0;
            double value = 0.0;
        };

        const auto getPointFromQueue = [] (Vst::IParamValueQueue* queue, Steinberg::int32 index)
        {
            ParamChangeInfo result;
            return queue->getPoint (index, result.offsetSamples, result.value) == kResultTrue
                   ? makeOptional (result)
                   : nullopt;
        };

        const auto numParamsChanged = paramChanges.getParameterCount();

        for (Steinberg::int32 i = 0; i < numParamsChanged; ++i)
        {
            if (auto* paramQueue = paramChanges.getParameterData (i))
            {
                const auto vstParamID = paramQueue->getParameterId();
                const auto numPoints  = paramQueue->getPointCount();

               #if JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS
                if (juceVST3EditController != nullptr && juceVST3EditController->isMidiControllerParamID (vstParamID))
                {
                    for (Steinberg::int32 point = 0; point < numPoints; ++point)
                    {
                        if (const auto change = getPointFromQueue (paramQueue, point))
                            addParameterChangeToMidiBuffer (change->offsetSamples, vstParamID, change->value);
                    }
                }
                else
               #endif
                if (const auto change = getPointFromQueue (paramQueue, numPoints - 1))
                {
                    if (auto* param = comPluginInstance->getParamForVSTParamID (vstParamID))
                        setValueAndNotifyIfChanged (*param, (float) change->value);
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
        const FLStudioDIYSpecificationEnforcementLock lock (flStudioDIYSpecificationEnforcementMutex);

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

        if (detail::PluginUtilities::getHostType().isWavelab())
        {
            const int numInputChans  = (data.inputs  != nullptr && data.inputs[0].channelBuffers32 != nullptr)  ? (int) data.inputs[0].numChannels  : 0;
            const int numOutputChans = (data.outputs != nullptr && data.outputs[0].channelBuffers32 != nullptr) ? (int) data.outputs[0].numChannels : 0;

            if ((pluginInstance->getTotalNumInputChannels() + pluginInstance->getTotalNumOutputChannels()) > 0
                 && (numInputChans + numOutputChans) == 0)
                return kResultFalse;
        }

        // If all of these are zero, the host is attempting to flush parameters without processing audio.
        if (data.numSamples != 0 || data.numInputs != 0 || data.numOutputs != 0)
        {
            if      (processSetup.symbolicSampleSize == Vst::kSample32) processAudio<float>  (data);
            else if (processSetup.symbolicSampleSize == Vst::kSample64) processAudio<double> (data);
            else jassertfalse;
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

       #if JucePlugin_ProducesMidiOutput
        if (isMidiOutputBusEnabled && data.outputEvents != nullptr)
            MidiEventList::pluginToHostEventList (*data.outputEvents, midiBuffer);
       #endif

        return kResultTrue;
    }

private:
    /*  FL's Patcher implements the VST3 specification incorrectly, calls process() before/during
        setActive().
    */
    class [[nodiscard]] FLStudioDIYSpecificationEnforcementLock
    {
    public:
        explicit FLStudioDIYSpecificationEnforcementLock (CriticalSection& mutex)
        {
            static const auto lockRequired = PluginHostType().isFruityLoops();

            if (lockRequired)
                lock.emplace (mutex);
        }

    private:
        std::optional<ScopedLock> lock;
    };

    InterfaceResultWithDeferredAddRef queryInterfaceInternal (const TUID targetIID)
    {
        const auto result = testForMultiple (*this,
                                             targetIID,
                                             UniqueBase<IPluginBase>{},
                                             UniqueBase<JuceVST3Component>{},
                                             UniqueBase<Vst::IComponent>{},
                                             UniqueBase<Vst::IAudioProcessor>{},
                                             UniqueBase<Vst::IUnitInfo>{},
                                             UniqueBase<Vst::IConnectionPoint>{},
                                             UniqueBase<Vst::IProcessContextRequirements>{},
                                            #if JucePlugin_Enable_ARA
                                             UniqueBase<ARA::IPlugInEntryPoint>{},
                                             UniqueBase<ARA::IPlugInEntryPoint2>{},
                                            #endif
                                             SharedBase<FUnknown, Vst::IComponent>{});

        if (result.isOk())
            return result;

        if (doUIDsMatch (targetIID, JuceAudioProcessor::iid))
            return { kResultOk, comPluginInstance.get() };

        return {};
    }

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
    void processAudio (Vst::ProcessData& data)
    {
        ClientRemappedBuffer<FloatType> remappedBuffer { bufferMapper, data };
        auto& buffer = remappedBuffer.buffer;

        jassert ((int) buffer.getNumChannels() == jmax (pluginInstance->getTotalNumInputChannels(),
                                                        pluginInstance->getTotalNumOutputChannels()));

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
                // processBlockBypassed should only ever be called if the AudioProcessor doesn't
                // return a valid parameter from getBypassParameter
                if (pluginInstance->getBypassParameter() == nullptr && comPluginInstance->getBypassParameter()->getValue() >= 0.5f)
                    pluginInstance->processBlockBypassed (buffer, midiBuffer);
                else
                    pluginInstance->processBlock (buffer, midiBuffer);
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
    }

    //==============================================================================
    Steinberg::uint32 PLUGIN_API getProcessContextRequirements() override
    {
        return kNeedSystemTime
             | kNeedContinousTimeSamples
             | kNeedProjectTimeMusic
             | kNeedBarPositionMusic
             | kNeedCycleMusic
             | kNeedSamplesToNextClock
             | kNeedTempo
             | kNeedTimeSignature
             | kNeedChord
             | kNeedFrameRate
             | kNeedTransportState;
    }

    void preparePlugin (double sampleRate, int bufferSize, CallPrepareToPlay callPrepareToPlay)
    {
        auto& p = getPluginInstance();

        p.setRateAndBufferSizeDetails (sampleRate, bufferSize);

        if (callPrepareToPlay == CallPrepareToPlay::yes)
            p.prepareToPlay (sampleRate, bufferSize);

        midiBuffer.ensureSize (2048);
        midiBuffer.clear();

        bufferMapper.updateFromProcessor (p);
        bufferMapper.prepare (bufferSize);
    }

    //==============================================================================
   #if JucePlugin_Enable_ARA
    const ARA::ARAFactory* PLUGIN_API getFactory() SMTG_OVERRIDE
    {
        return createARAFactory();
    }

    const ARA::ARAPlugInExtensionInstance* PLUGIN_API bindToDocumentController (ARA::ARADocumentControllerRef /*controllerRef*/) SMTG_OVERRIDE
    {
        ARA_VALIDATE_API_STATE (false && "call is deprecated in ARA 2, host must not call this");
        return nullptr;
    }

    const ARA::ARAPlugInExtensionInstance* PLUGIN_API bindToDocumentControllerWithRoles (ARA::ARADocumentControllerRef documentControllerRef,
                                                                                         ARA::ARAPlugInInstanceRoleFlags knownRoles,
                                                                                         ARA::ARAPlugInInstanceRoleFlags assignedRoles) SMTG_OVERRIDE
    {
        if (auto* araAudioProcessorExtension = pluginInstance->getARAClientExtensions())
            return araAudioProcessorExtension->bindToARA (documentControllerRef, knownRoles, assignedRoles);

        jassertfalse;
        return nullptr;
    }
   #endif

    //==============================================================================
    ScopedRunLoop scopedRunLoop;
    std::atomic<int> refCount { 1 };
    AudioProcessor* pluginInstance = nullptr;

   #if JUCE_LINUX || JUCE_BSD
    template <class T>
    struct LockedVSTComSmartPtr
    {
        LockedVSTComSmartPtr() = default;
        LockedVSTComSmartPtr (const VSTComSmartPtr<T>& ptrIn) : ptr (ptrIn)  {}
        LockedVSTComSmartPtr (const LockedVSTComSmartPtr&) = default;
        LockedVSTComSmartPtr& operator= (const LockedVSTComSmartPtr&) = default;

        ~LockedVSTComSmartPtr()
        {
            const MessageManagerLock mmLock;
            ptr = {};
        }

        T* operator->() const         { return ptr.operator->(); }
        T* get() const noexcept       { return ptr.get(); }
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
    ClientBufferMapper bufferMapper;

    bool active = false;

   #if JucePlugin_WantsMidiInput
    std::atomic<bool> isMidiInputBusEnabled { true };
   #endif
   #if JucePlugin_ProducesMidiOutput
    std::atomic<bool> isMidiOutputBusEnabled { true };
   #endif

    inline static constexpr const char* kJucePrivateDataIdentifier = "JUCEPrivateData";
    CriticalSection flStudioDIYSpecificationEnforcementMutex;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JuceVST3Component)
};

//==============================================================================
bool initModule();
bool initModule()
{
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
 #define JUCE_EXPORTED_FUNCTION extern "C" __attribute__ ((visibility ("default")))
#endif

#if JUCE_WINDOWS
 JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wmissing-prototypes")

 extern "C" __declspec (dllexport) bool InitDll()   { return initModule(); }
 extern "C" __declspec (dllexport) bool ExitDll()   { return shutdownModule(); }

 JUCE_END_IGNORE_WARNINGS_GCC_LIKE
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
class JucePluginFactory final : public JucePluginFactoryBase
{
public:
    JucePluginFactory() = default;

    Steinberg::tresult PLUGIN_API createInstance (Steinberg::FIDString cid,
                                                  Steinberg::FIDString sourceIid,
                                                  void** obj) override
    {
        const ScopedRunLoop scope { runLoop };
        return JucePluginFactoryBase::createInstance (cid, sourceIid, obj);
    }

    Steinberg::tresult PLUGIN_API setHostContext (Steinberg::FUnknown* context) override
    {
        runLoop.loadFrom (context);
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
    FUnknown* createInstance (const ClassEntry& entry) final
    {
        if (doUIDsMatch (entry.info2.cid, JuceVST3Component::iid))
            return static_cast<Vst::IAudioProcessor*> (new JuceVST3Component (host, runLoop));

        if (doUIDsMatch (entry.info2.cid, JuceVST3EditController::iid))
            return static_cast<Vst::IEditController*> (new JuceVST3EditController (host, runLoop));

       #if JucePlugin_Enable_ARA
        if (doUIDsMatch (entry.info2.cid, JuceARAFactory::iid))
            return static_cast<ARA::IMainFactory*> (new JuceARAFactory());
       #endif

        return JucePluginFactoryBase::createInstance (entry);
    }

    RunLoop runLoop;
    VSTComSmartPtr<Vst::IHostApplication> host;
};

} // namespace juce

//==============================================================================
using namespace juce;

//==============================================================================
// The VST3 plugin entry point.
extern "C" SMTG_EXPORT_SYMBOL IPluginFactory* PLUGIN_API GetPluginFactory()
{
   #if (JUCE_MSVC || (JUCE_WINDOWS && JUCE_CLANG)) && JUCE_32BIT
    // Cunning trick to force this function to be exported. Life's too short to
    // faff around creating .def files for this kind of thing.
    // Unnecessary for 64-bit builds because those don't use decorated function names.
    #pragma comment(linker, "/EXPORT:GetPluginFactory=_GetPluginFactory@0")
   #endif

    return new JucePluginFactory();
}

//==============================================================================
#if JUCE_WINDOWS
JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wmissing-prototypes")

extern "C" BOOL WINAPI DllMain (HINSTANCE instance, DWORD reason, LPVOID) { if (reason == DLL_PROCESS_ATTACH) Process::setCurrentModuleInstanceHandle (instance); return true; }

JUCE_END_IGNORE_WARNINGS_GCC_LIKE
#endif

JUCE_END_NO_SANITIZE

#endif //JucePlugin_Build_VST3
