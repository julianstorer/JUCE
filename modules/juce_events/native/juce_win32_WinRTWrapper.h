/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2020 - Raw Material Software Limited

   JUCE is an open source library subject to commercial or open-source
   licensing.

   The code included in this file is provided under the terms of the ISC license
   http://www.isc.org/downloads/software-support-policy/isc-license. Permission
   To use, copy, modify, and/or distribute this software for any purpose with or
   without fee is hereby granted provided that the above copyright notice and
   this permission notice appear in all copies.

   JUCE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

  ==============================================================================
*/

namespace juce
{

class WinRTWrapper :   public DeletedAtShutdown
{
public:
    //==============================================================================
    ~WinRTWrapper();
    bool isInitialised() const noexcept  { return initialised; }

    JUCE_DECLARE_SINGLETON (WinRTWrapper, false)

    //==============================================================================
    template <class ComClass>
    ComSmartPtr<ComClass> activateInstance (const wchar_t* runtimeClassID, REFCLSID classUUID)
    {
        ComSmartPtr<ComClass> result;

        if (isInitialised())
        {
            ComSmartPtr<IInspectable> inspectable;
            ScopedHString runtimeClass (runtimeClassID);
            auto hr = roActivateInstance (runtimeClass.get(), inspectable.resetAndGetPointerAddress());

            if (SUCCEEDED (hr))
                inspectable->QueryInterface (classUUID, (void**) result.resetAndGetPointerAddress());
        }

        return result;
    }

    template <class ComClass>
    ComSmartPtr<ComClass> getWRLFactory (const wchar_t* runtimeClassID)
    {
        ComSmartPtr<ComClass> comPtr;

        if (isInitialised())
        {
            ScopedHString classID (runtimeClassID);

            if (classID.get() != nullptr)
                roGetActivationFactory (classID.get(), __uuidof (ComClass), (void**) comPtr.resetAndGetPointerAddress());
        }

        return comPtr;
    }

    //==============================================================================
    class ScopedHString
    {
    public:
        ScopedHString (String);
        ~ScopedHString();

        HSTRING get() const noexcept          { return hstr; }

    private:
        HSTRING hstr = nullptr;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScopedHString)
    };

    String hStringToString (HSTRING);

private:
    WinRTWrapper();

    //==============================================================================
    HMODULE winRTHandle = nullptr;
    bool initialised = false;

    typedef HRESULT (WINAPI* RoInitializeFuncPtr) (int);
    typedef HRESULT (WINAPI* WindowsCreateStringFuncPtr) (LPCWSTR, UINT32, HSTRING*);
    typedef HRESULT (WINAPI* WindowsDeleteStringFuncPtr) (HSTRING);
    typedef PCWSTR  (WINAPI* WindowsGetStringRawBufferFuncPtr) (HSTRING, UINT32*);
    typedef HRESULT (WINAPI* RoActivateInstanceFuncPtr) (HSTRING, IInspectable**);
    typedef HRESULT (WINAPI* RoGetActivationFactoryFuncPtr) (HSTRING, REFIID, void**);

    RoInitializeFuncPtr roInitialize = nullptr;
    WindowsCreateStringFuncPtr createHString = nullptr;
    WindowsDeleteStringFuncPtr deleteHString = nullptr;
    WindowsGetStringRawBufferFuncPtr getHStringRawBuffer = nullptr;
    RoActivateInstanceFuncPtr roActivateInstance = nullptr;
    RoGetActivationFactoryFuncPtr roGetActivationFactory = nullptr;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WinRTWrapper)
};

} // namespace juce
