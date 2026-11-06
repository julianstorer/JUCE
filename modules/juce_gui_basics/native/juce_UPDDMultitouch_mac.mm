
#include "juce_UPDDMultitouch_upddapi.h"

namespace juce
{

class UPDDManager : public DeletedAtShutdown
{
public:
#ifndef DOXYGEN
    JUCE_DECLARE_SINGLETON(UPDDManager, true)
#endif

    typedef void (*TBApiOpenType)(void);
    typedef void (*TBApiCloseType)(void);
    typedef void (*TBApiRegisterEventType)(HTBDEVICE, unsigned long long, unsigned long, TB_EVENT_CALL);
    typedef void (*TBApiUnregisterEventType)(TB_EVENT_CALL);
    typedef HTBDEVICE (*TBApiGetRelativeDeviceType)(int);
    typedef TBBOOL (*TBApiGetSettingAsIntType)(HTBDEVICE, const TBCHAR *, int32_t *);

    enum class TouchState {
        Unknown, Began, Moved, Released, Stationary
    };

    struct TouchIdentifier {
        HTBDEVICE hDevice = 0;
        TBSTYLUS hStylus = 0;
        unsigned char hidReportid = 0;
        unsigned char usbInterface = 0;
        bool isPen = false;
        
        bool operator==(const TouchIdentifier &other) const {
            return hDevice == other.hDevice && hStylus == other.hStylus && hidReportid == other.hidReportid
                   && usbInterface == other.usbInterface && isPen == other.isPen;
        }
        
        bool operator<(const TouchIdentifier &other) const {
            if (hDevice != other.hDevice) {
                return hDevice < other.hDevice;
            }
            
            if (hStylus != other.hStylus) {
                return hStylus < other.hStylus;
            }
            
            if (hidReportid != other.hidReportid) {
                return hidReportid < other.hidReportid;
            }
            
            if (usbInterface != other.usbInterface) {
                return usbInterface < other.usbInterface;
            }
            
            return isPen < other.isPen;
        }
        
        static TouchIdentifier none;
    };
    
    struct TouchData {
        Point<int> screenPosition;
        Point<int> prevScreenPosition;
        TouchState state = TouchState::Unknown;
        TouchState prevState = TouchState::Unknown;
        float pressure = 0;
        int id = -1;
    };
    
    struct UPDDDeviceData {
        std::map<TouchIdentifier, TouchData> touches;
        bool touchesChangedSinceLastUpdate = false;
        uint32_t minZ = 0;
        uint32_t maxZ = 127;
    };
    
    struct JUCEDeviceData {
        std::map<TouchIdentifier, WeakReference<Component>> touchWindows;
    };
    
private:
    CriticalSection lock;
    void *libupddapi = nullptr;
    TBApiOpenType dTBApiOpen = nullptr;
    TBApiCloseType dTBApiClose = nullptr;
    TBApiRegisterEventType dTBApiRegisterEvent = nullptr;
    TBApiUnregisterEventType dTBApiUnregisterEvent = nullptr;
    TBApiGetRelativeDeviceType dTBApiGetRelativeDevice = nullptr;
    TBApiGetSettingAsIntType dTBApiGetSettingAsInt = nullptr;
    std::atomic_bool opened{false};
    std::atomic_bool connected{false};
    std::atomic_bool registered{false};
    std::atomic_bool unloaded{false};
    std::map<HTBDEVICE, UPDDDeviceData> upddDeviceDataByHandle; // NB: will be accessed by multiple threads
    std::map<HTBDEVICE, JUCEDeviceData> JUCEDeviceDataByHandle; // NB: must only be accessed in main thread
    
public:
    void ensureConnected()
    {
        const ScopedLock sl(lock);
        
        if (!isUPDDAvailable() || unloaded || opened) {
            return;
        }
        
        dTBApiRegisterEvent(0, 0, _EventConfiguration, upddConfigurationEventStatic);
        dTBApiOpen();
        opened = true;
    }
    
    bool isUPDDAvailable()
    {
        return libupddapi && dTBApiOpen && dTBApiClose && dTBApiRegisterEvent && dTBApiUnregisterEvent
               && dTBApiGetRelativeDevice && dTBApiGetSettingAsInt;
    }

private:
    UPDDManager()
    {
        loadUPDDAPI();
    }
    
    ~UPDDManager()
    {
        shutdown();
        
        if (libupddapi) {
            dlclose(libupddapi);
        }
        
        clearSingletonInstance();
    }
    
    void loadUPDDAPI()
    {
        if (!File("/Library/Application Support/UPDD").exists()) {
            // UPDD v7 is not installed, nothing to do here
            return;
        }
        
        dlerror(); // Clear any existing errors, just in case
        String errorString;
        
        // First try finding libupddapi on RPATH:
        libupddapi = dlopen("@rpath/libupddapi.7.0.0.dylib", RTLD_LAZY);
        
        if (!libupddapi) {
            errorString << dlerror();
            // Failing that, try absolute path:
            libupddapi = dlopen("/Library/Application Support/UPDD/libupddapi.7.0.0.dylib", RTLD_LAZY);
        }
        
        if (!libupddapi) {
            errorString << "; " << dlerror();
            DBG("Failed to load libupddapi: " << errorString);
            DBG("Multi-touch will not work");
            return;
        }
        
        dTBApiOpen = (TBApiOpenType)dlsym(libupddapi, "TBApiOpen");
        
        if (!dTBApiOpen) {
            DBG("Failed to load libupddapi: " << dlerror());
            DBG("Multi-touch will not work");
            return;
        }
        
        dTBApiClose = (TBApiCloseType)dlsym(libupddapi, "TBApiClose");
        
        if (!dTBApiClose) {
            DBG("Failed to load libupddapi: " << dlerror());
            DBG("Multi-touch will not work");
            return;
        }
        
        dTBApiRegisterEvent = (TBApiRegisterEventType)dlsym(libupddapi, "TBApiRegisterEvent");
        
        if (!dTBApiRegisterEvent) {
            DBG("Failed to load libupddapi: " << dlerror());
            DBG("Multi-touch will not work");
            return;
        }
        
        dTBApiUnregisterEvent = (TBApiUnregisterEventType)dlsym(libupddapi, "TBApiUnregisterEvent");
        
        if (!dTBApiUnregisterEvent) {
            DBG("Failed to load libupddapi: " << dlerror());
            DBG("Multi-touch will not work");
            return;
        }
        
        dTBApiGetRelativeDevice = (TBApiGetRelativeDeviceType)dlsym(libupddapi, "TBApiGetRelativeDevice");
        
        if (!dTBApiGetRelativeDevice) {
            DBG("Failed to load libupddapi: " << dlerror());
            DBG("Multi-touch will not work");
            return;
        }
        
        dTBApiGetSettingAsInt = (TBApiGetSettingAsIntType)dlsym(libupddapi, "TBApiGetSettingAsInt");
        
        if (!dTBApiGetSettingAsInt) {
            DBG("Failed to load libupddapi: " << dlerror());
            DBG("Multi-touch will not work");
            return;
        }
    }
    
    void shutdown()
    {
        if (!isUPDDAvailable()) {
            return;
        }
        
        const ScopedLock sl(lock);
        
        if (registered) {
            dTBApiUnregisterEvent(upddTouchEventStatic);
            registered = false;
        }
        
        if (opened) {
            dTBApiUnregisterEvent(upddConfigurationEventStatic);
            dTBApiClose();
            opened = false;
        }
    }
    
    static void upddConfigurationEventStatic(UPDD_CONTEXT, _PointerEvent *event)
    {
        // Being careful just in case this function executes after UPDDManager
        // has been deleted during shutdown. Will follow this pattern wherever
        // we schedule call to a method of UPDDManager
        if (UPDDManager *shared = UPDDManager::getInstance()) {
            shared->upddConfigurationEvent(event);
        }
    }
    
    void upddConfigurationEvent(_PointerEvent *event)
    {
        switch(event->pe.config.configEventType) {
            case CONFIG_EVENT_CONNECT:
                connected = true;
                DBG("Connected to UPDD");
                dispatch_async(dispatch_get_main_queue(), ^{
                    if (UPDDManager *shared = UPDDManager::getInstance()) {
                        shared->onUPDDConnected();
                    }
                });
                break;

            case CONFIG_EVENT_DISCONNECT:
                DBG("Disconnected from UPDD");
                connected = false;
                break;
            
            case CONFIG_EVENT_UNLOAD:
                DBG("UPDD is unloading");
                unloaded = true;
                shutdown();
                break;
            
            case CONFIG_EVENT_DEVICE:
            case CONFIG_EVENT_DEVICE_BIND:
                dispatch_async(dispatch_get_main_queue(), ^{
                    if (UPDDManager *shared = UPDDManager::getInstance()) {
                        shared->updateDevices();
                    }
                });
                break;
        }
    }
    
    void onUPDDConnected()
    {
        if (!registered) {
            dTBApiRegisterEvent(0, 0, _EventTypeDigitiserEvent, upddTouchEventStatic);
            registered = true;
        }
    }
    
    void updateDevices()
    {
        const ScopedLock sl(lock);
        
        int deviceIndex = 0;
        HTBDEVICE device = dTBApiGetRelativeDevice(deviceIndex);
        
        while(device != TB_INVALID_HANDLE_VALUE) {
            UPDDDeviceData &deviceData = upddDeviceDataByHandle[device];
            getPressureRangeFromUPDD(deviceData.minZ, deviceData.maxZ, device);
            
            device = dTBApiGetRelativeDevice(deviceIndex);
        }
    }

    int32_t upddGetSettingAsIntWithDefault(HTBDEVICE device, const char *name, int32_t defaultValue)
    {
        int32_t result;
        bool success = dTBApiGetSettingAsInt(device, name, &result);
        
        if (success) {
            return result;
        } else {
            return defaultValue;
        }
    }
    
    void getPressureRangeFromUPDD(uint32_t &minPressure, uint32_t &maxPressure, HTBDEVICE deviceHandle)
    {
        minPressure = 0;    
        maxPressure = 127; // Default max pressure value, probably is not correct
        
        if (upddGetSettingAsIntWithDefault(deviceHandle, "ignore_maxz", 0) == 1) {
            return;
        }
        
        int32_t minp = upddGetSettingAsIntWithDefault(deviceHandle, "minpressure", 0);
        int32_t maxp = upddGetSettingAsIntWithDefault(deviceHandle, "maxpressure", 0);
        
        if (minp > 0) {
            minPressure = (uint32_t)minp;
        }
        
        if (maxp > 0) {
            maxPressure = (uint32_t)maxp;
        }
    }

    static void upddTouchEventStatic(UPDD_CONTEXT, _PointerEvent *event)
    {
        if (UPDDManager *shared = UPDDManager::getInstance()) {
            shared->upddTouchEvent(event);
        }
    }
    
    void upddTouchEvent(_PointerEvent *event)
    {
        if (event->type != _EventTypeDigitiserEvent || event->pe.digitiserEvent.digitizerType != DIGITIZER_TYPE_TOUCH
            || event->calibrating || unloaded) {
            return;
        }
        
        const ScopedLock sl(lock);

        UPDDDeviceData &deviceData = upddDeviceDataByHandle[event->hDevice];
        TouchIdentifier touchId;
        touchId.hDevice = event->hDevice;
        touchId.hStylus = event->hStylus;
        touchId.hidReportid = event->hidReportid;
        touchId.usbInterface = event->usbInterface;
        touchId.isPen = (event->pe.digitiserEvent.digitizerType == DIGITIZER_TYPE_PEN);
        bool touching = event->pe.digitiserEvent.de.touchEvent.touchingLeft;
        bool prevTouching = (deviceData.touches.count(touchId) == 1);
        
        TouchData &touchData = deviceData.touches[touchId];
        touchData.prevScreenPosition = touchData.screenPosition;
        touchData.prevState = touchData.state;
        touchData.screenPosition = Point((int)event->pe.digitiserEvent.screenx, (int)event->pe.digitiserEvent.screeny);
        touchData.pressure = event->pe.digitiserEvent.zSupport
                             ? jmax(jmin((float(event->pe.digitiserEvent.z - deviceData.minZ) / float(deviceData.maxZ)),
                                         1.0f),
                                    0.0f)
                             : 0.0f;

        if (touchData.id == -1) {
            touchData.id = findNextUnusedTouchIdForDevice(deviceData);
        }
    
        if (touching && !prevTouching) {
            // Touch began
            deviceData.touchesChangedSinceLastUpdate = true;
            touchData.state = TouchState::Began;
            
        } else if (!touching && prevTouching) {
            // Touch ended
            deviceData.touchesChangedSinceLastUpdate = true;
            touchData.state = TouchState::Released;
            
        } else if (touching && prevTouching) {
            // Touch updated

            if ((touchData.prevState != TouchState::Moved && touchData.prevState != TouchState::Stationary
                 && touchData.prevState != TouchState::Began)
                || touchData.screenPosition != touchData.prevScreenPosition) {
                deviceData.touchesChangedSinceLastUpdate = true;
                touchData.state = TouchState::Moved;
            } else {
                touchData.state = TouchState::Stationary;
            }
        } else {
            // Received touch released event before touch down event
            // Some touch devices do this, in which case we want to ignore that touch event
            deviceData.touches.erase(touchId);
        }
        
        if (event->pe.digitiserEvent.lastContact && deviceData.touchesChangedSinceLastUpdate) {
            deviceData.touchesChangedSinceLastUpdate = false;
            {
                HTBDEVICE deviceHandle = event->hDevice;
                std::map<TouchIdentifier, TouchData> touches = deviceData.touches;
                
                dispatch_async(dispatch_get_main_queue(), ^{
                    if (UPDDManager *shared = UPDDManager::getInstance()) {
                        shared->postDeviceTouchesToContainers(deviceHandle, touches);
                    }
                });
            }
            
            for(auto it = deviceData.touches.begin(); it != deviceData.touches.end();) {
                if (it->second.state == TouchState::Released) {
                    it = deviceData.touches.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }
    
    Component * findTopLevelComponentContainingPoint(const Point<int> &p)
    {
        Component *component = Desktop::getInstance().findComponentAt(p);
        
        if (!component) {
            return nullptr;
        }
        
        return component->getTopLevelComponent();
    }
    
    void postDeviceTouchesToContainers(HTBDEVICE deviceHandle, const std::map<TouchIdentifier, TouchData> &touches)
    {
        JUCEDeviceData &deviceData = JUCEDeviceDataByHandle[deviceHandle];
        
        for(auto it = touches.begin(); it != touches.end(); ++it) {
            const TouchIdentifier &touchId = it->first;
            const TouchData &touchData = it->second;
            
            if (touchData.state == TouchState::Unknown || touchData.state == TouchState::Stationary) {
                // Ignore degenerate or stationary touch
                continue;
            }
            
            if (touchData.state == TouchState::Began) {
                Component *window = findTopLevelComponentContainingPoint(touchData.screenPosition);
                deviceData.touchWindows[touchId] = WeakReference<Component>(window);
            }
            
            Component *touchWindow = deviceData.touchWindows[touchId].get();
            
            if (touchData.state == TouchState::Released) {
                deviceData.touchWindows.erase(touchId);
            }
            
            if (!touchWindow) {
                // A window might have been closed or deleted over the course of a touch
                continue;
            }
            
            ComponentPeer *peer = touchWindow->getPeer();
            
            if (!peer) {
                continue;
            }
            
            Point<int> localPosition = touchWindow->getLocalPoint(nullptr, touchData.screenPosition);
            ModifierKeys modifiers;
            
            if (touchData.state == TouchState::Began || touchData.state == TouchState::Moved) {
                modifiers = ModifierKeys::getCurrentModifiers().withoutMouseButtons().withFlags(ModifierKeys::leftButtonModifier);
            } else {
                modifiers = ModifierKeys::getCurrentModifiers().withoutMouseButtons();
            }
            
            peer->handleMouseEvent(MouseInputSource::InputSourceType::touch, localPosition.toFloat(),
                                   modifiers, touchData.pressure, 0.0, Time::currentTimeMillis(), {}, touchData.id);
        }
    }
    
    int findNextUnusedTouchIdForDevice(UPDDDeviceData &deviceData)
    {
        SortedSet<int> usedIds;
        
        for(const auto &it : deviceData.touches) {
            usedIds.add(it.second.id);
        }
        
        int id = 0;
        
        while(usedIds.contains(id)) {
            ++id;
        }
        
        return id;
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UPDDManager)
};

JUCE_IMPLEMENT_SINGLETON (UPDDManager)

}
