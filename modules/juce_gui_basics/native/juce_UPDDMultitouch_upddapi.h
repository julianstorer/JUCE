#ifndef TBAPI_H 
#define TBAPI_H 

#define UPDD_API_VERSION 7

#include "stdint.h"
#include <stddef.h> //https://stackoverflow.com/questions/49990753/xcode-7-unknown-type-name-size-t
#undef TBAPIDLLPFX 
#ifndef TBCHAR 
#ifdef UNDER_CE
#define TBCHAR wchar_t
#define _TBT(x)      L ## x
#else
#define TBCHAR char
#define _TBT 
#endif
#endif

#ifndef t_string 
  #ifdef UNDER_CE
    #define t_string wstring
  #else
    #define t_string string
  #endif
#endif

#if _WIN32 || _WIN64
#if _WIN64
#define ENVIRONMENT64
#else
#define ENVIRONMENT32
#endif
#endif

// Check GCC
#if __GNUC__
#if __x86_64__ || __ppc64__ || __aarch64__
#define ENVIRONMENT64
#else
#define ENVIRONMENT32
#endif
#endif

#ifdef __APPLE__
#define ENVIRONMENT64
#endif

#ifdef ENVIRONMENT64
#define UPDD_CONTEXT unsigned long long 
#else 
#define UPDD_CONTEXT unsigned long  
#endif


#ifdef INTERNALAPI
// this mode is for internal touch-base use 
#ifdef _WIN32
//#pragma comment(linker,"/nodefaultlib:libcmt") 
//#pragma comment(linker,"/nodefaultlib:libcmtd") 
#pragma comment(linker,"/nodefaultlib:msvcrtd") 
#pragma comment(linker,"/nodefaultlib:msvcrt") 
#pragma comment(lib,"setupapi.lib")
#endif
#endif 

#ifdef _WIN32
#define TBCALL _stdcall
#else
#define TBCALL
#endif

#ifdef TBAPIDLL_EXPORTS 
#ifdef _WIN32
#define TBAPI __declspec(dllexport) TBCALL
#pragma warning(disable:4518)            
#endif
#else
#ifdef _WIN32
#define TBAPI __declspec(dllimport) TBCALL
#pragma warning(disable:4518)            
#endif
//#define TBAPIDLLPFX 
#endif 

#ifndef TBAPI 
#define TBAPI 
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#define TBBOOL uint16_t


// constants that define data types to be read
// can be ORed, eg _EventTypeXY | _EventTypeEvent
// to read pointer co-ordinates and button events

#define _EventTypeXY 0x0001    // pointer co-ordinates
#define _EventTypeEval 0x0002    // change in evaluation state 
#define _EventTypeRaw 0x0008  // raw data
#define _EventTypeToolbar 0x0010  // toolbar events 
#define _EventConfiguration 0x0020  // OBSOLESCENT - typo for _EventTypeConfiguration, this value will be removed in due course  
#define _EventTypeConfiguration 0x0020  // notifications of changes to driver configuration and state 
//#define _EventTypeZ 0x0080   // notifications of change in Z values OBSOLETE; use _EventTypeDigitiserEvent
#define _EventTypeRelative 0x0100 // notifications of relative movement
#define _EventTypeUnload 0x0200  // the driver is about to attempt an unload 
#define _EventTypeXYNoMask 0x1000    // same as _EventTypeXY but not masked by toolbars and surrounds used for calibration  -- WAS _EventTypeXYCal prior to V6
#define _EventTypeInteractiveTouch 0x4000 // mouse pointer state events for interactive touch mode
#ifdef UPDD_GESTURES
#define _EventTypeGesture 0x8000  
#endif
#define _EventTypePlayUPDDSound      0x800000  // play a sound defined for this device
#define _EventTypeMouseEvent         0x1000000  // raw data sent to mouse port 
#define _EventTypeSecBlock           0x2000000  // touch data was received when a security block was in place 
#define _EventTypeRawMouse           0x8000000  // internal use only 
#define _EventTypeLogicalEvent       0x20000000  // state changes passed to operating system 
#define _EventTypePhysicalEvent      0x40000000  // changes in the actual "touching" state OBSOLESCENT
#define _EventTypeDigitiserEvent     0x4000000  
#define _EventTypeDigitiserEventTOIP 0x10000000  // for UPDD touch over ip internal use only, do not use in 3rd party software


#define CONFIG_EVENT_SETTINGS 1             // one or more updd settings have been changed 
#define CONFIG_EVENT_CONCURRENCY_SIGNAL  2  // a signal was requested by a call to TBApiRegisterProgram 
#define CONFIG_EVENT_CONNECT 3              // a client connection to the driver has been established 
#define CONFIG_EVENT_DISCONNECT 4           // the client connection to the driver has disconnected 
#define CONFIG_EVENT_UNLOAD 5               // the driver has requested termination of this application, typically for uninstallation
#define CONFIG_EVENT_DEVICE 6               // notification of a change in physical device state 
#define CONFIG_EVENT_AUTOCONFIGURE 7        // an auto configure operation has been triggered 
#define CONFIG_EVENT_CONCURRENCY_CHANGE  8  // a program was registered with TBApiRegisterProgram or deregistered 
#define CONFIG_EVENT_MONITOR_DETECT  9      // sent at beginning and end of a monitor detection sequence 
#define CONFIG_EVENT_INTERNAL  10           // reserved for internal use
#define CONFIG_EVENT_DEVICE_BIND  11        // notification of a change in logical device state 
#define CONFIG_EVENT_INTERNAL_2  12           // reserved for internal use

#define TOUCH_BIT_FLAGS_LEFT 0x1
#define TOUCH_BIT_FLAGS_RIGHT 0x2 
#define TOUCH_BIT_FLAGS_IN_RANGE 0x8

#define PEN_BIT_FLAGS_TIP 0x1
#define PEN_BIT_FLAGS_BARREL 0x2 
#define PEN_BIT_FLAGS_ERASER 0x4
#define PEN_BIT_FLAGS_IN_RANGE 0x8
#define PEN_BIT_FLAGS_INVERT 0x10
#define PEN_BIT_FLAGS_Z 0x20

#define DIGITIZER_TYPE_PEN 0x2
#define DIGITIZER_TYPE_TOUCH 0x4

#define HTBDEVICE unsigned char
#define HTBTOOLBAR int16_t
#define TBSTYLUS unsigned char
#define TB_INVALID_HANDLE_VALUE 0x00

#define MAXSTYLENAME 32
#define MAXCALPOINTS 25

#define INJECT_FLAG_IGNORE_MP_DISABLED 2 
#define INJECT_FLAG_GENERATE_POINTER_EVENTS 4
#define INJECT_FLAG_GENERATE_COMPATIBILITY_EVENTS 8
#define INJECT_FLAG_INTERNAL_COORDINATES 16
#define INJECT_FLAG_RAW_COORDINATES 32
#define INJECT_FLAG_NOT_LAST_CONTACT 64

#define NOTIFY_LEVEL_OTHER 0
#define NOTIFY_LEVEL_CONFIG_WARNINGS 1
#define NOTIFY_LEVEL_EVAL_AND_CRITICAL 2

#ifndef TBAPIDEFNS_H 
#define TBAPIDEFNS_H 
 
#pragma pack (push, 1)
#ifndef __cplusplus
typedef struct _PointerEvent _PointerEvent;
#endif
struct _PointerEvent // was PointerData prior to V6
{
  HTBDEVICE hDevice;    // device handle that this event relates to or 0 if it is not device specific 
  TBSTYLUS hStylus;     // stylus number, (also known as contact number or touch number) was HTBSTYLUS prior to V6 
  uint64_t type;   // data type of the event, indicates which of the items in the union below is populated
  uint64_t length; // length of data (currently only raw data) 
  unsigned char touchDelegated; // set to true (1) if this app should act as the primary provider of touch. 
  unsigned char usbConfiguration;
  unsigned char usbInterface;
  unsigned char hidEndpoint;
  unsigned char hidReportid;
  unsigned char calibrating;  // set to true (1) if calibration is active; most client apps (ie non calibration apps) should ignore events with this set 
  uint8_t monitor_number;
  uint32_t timestamp;
  uint8_t priority;           // for internal use only 
  uint8_t reserved_byte[2];
  uint32_t reserved[14];
//  unsigned long reserved[16];

//  unsigned long tick;		// indicates relative time data was read 
  union _pe 		          // only one of the following is used, as indicated by the "type" member
  {
    struct  _digitiserEvent
    {
      union  _de
      {
        struct _penEvent
        {
          uint8_t tipSwitch : 1;        // bit flags relating to pen devices, relates to PEN_BIT_FLAGS_XXX above
          uint8_t barrelSwitch : 1;
          uint8_t invert : 1;
          uint8_t inrange : 1;
          uint8_t eraser : 1;
          uint8_t reserved2 : 1;
          uint8_t reserved3 : 1;
          uint8_t reserved4 : 1;
          uint32_t reserved5;
        }penEvent;
        struct _touchEvent
        {
          uint8_t touchingLeft : 1; // bit flags relating to regular touch devices, relates to TOUCH_BIT_FLAGS_XXX above
          uint8_t touchingRight : 1;
          uint8_t unused : 1;
          uint8_t inrange : 1;
        }touchEvent;
      }de;
      uint8_t deltaBits;          // a bit mask to indicate which bits are changed since last _digitiserEvent 
      uint8_t validBits;          // a bit mask to indicate which bits are supported by the originating hardware 
      int32_t screenx;            // screen co-ordinate values, these values are in screen pixels and take account of the co-ordinate range of the associated monitors
      int32_t screeny;	          // so for example with 2 monitors, resolution 1024 x 768 side by side; with the left monitor being the primary, 
                                  // touching the centre of the right gives about 1536, 384
      int32_t internalx;          // the corresponding windows co-ordinate value, the primary monitor has the range 0xffff, and other monitors are scaled from that
      int32_t internaly;	        // so in the example given above the result is 0x17fee, 0x7fff
      int32_t calx;               // the calibrated co-ordinates values; a value from 0 - 0xffff, giving the absolute position of touch in the range of the originating hardware 
      int32_t caly;	              // so for example touching the centre of a screen will give  around 0x7fff regardless of the associated monitor
      TBBOOL zSupport;            // set to TRUE (1) if the originating hardware supports z values
      uint32_t z;                 // the raw z value reported by the controller, typically this is used to indicate pressure

      TBBOOL isTimed;             // set to TRUE (1) if the event is triggered by a timeout (eg liftoff time)  
      TBBOOL isToolbar;           // set to TRUE (1) if the event is for a touch start started in a toolbar
      TBBOOL stylusSupport;       // set to TRUE (1) if the originating hardware supports stylus values
      uint8_t digitizerType;      // see DIGITIZER_TYPE_xxx
      TBBOOL  lastContact;        // set to TRUE (1) if the event is triggered by the last contact in a touch event from the source device
      int32_t internal_event_number;        // for internal use only
      uint32_t contact_width;
      uint32_t contact_height;
      int8_t xtilt;
      int8_t ytilt;
      int32_t rawx;	// the raw X value from the controller 
      int32_t rawy;	// the raw Y value from the controller 
    }digitiserEvent;

    struct _xy		// co-ordinate data
    {	
      int32_t rawx;	// the raw X value from the controller 
      int32_t rawy;	// the raw Y value from the controller 
      int32_t calx; // the corresponding calibrated value 
      int32_t caly;	//           --- ditto ---
      int32_t calx_rotated; // the corresponding calibrated unrotated value (for toolbars)
      int32_t caly_rotated;	//           --- ditto ---

      
      int32_t screenx; // the corresponding screen co-ordinate value 
      int32_t screeny;	//           --- ditto ---
      int32_t internalx; // the corresponding windows co-ordinate value 
      int32_t internaly;	//           --- ditto ---
    }xy;				
    struct _z		// pressure data
    {	
      uint32_t rawz;	// the raw z value from the controller
    }z;				
    struct _logicalEvent		
    {
      TBBOOL left;            // does this represent a left mouse button action 	
      TBBOOL state;  	        // the value that the state changed to  	
      TBBOOL timed;           // whether the change is triggered by a timeout (eg liftoff time)  
//      TBBOOL wasToolbarTouch; // was the touch in a toolbar area when the state change to true
    }logicalEvent;                             
    struct  _physicalEvent  
    {
      TBBOOL state;           // the value that the state changed to  	
      TBBOOL timed;           // whether the change is triggered by a timeout (eg liftoff time)  
  //    TBBOOL wasToolbarTouch; // was the touch in a toolbar area when the state change to true
    }physicalEvent;                             

    // following structure is obsolescent; you should use _touchEvent instead 
    // but currently still supported in the api so you can uncomment this if needed; however we recommend converting to _touchEvent  
    //struct _flagsEvent
    //{
    //  TBBOOL hasEVNN;         // true if the evnn field has valid data
    //  uint32_t evnn;          // value of the EVNN bits in the data packet
    //  TBBOOL hasLBit;         // true if the lBit field has valid data
    //  TBBOOL lBit;            // value of the L bit in the data packet
    //  TBBOOL hasRBit;         // true if the rBit field has valid data
    //  TBBOOL rBit;            // value of the R bit in the data packet
    //}flagsEvent;                             
    struct _raw  // raw data
    {
      uint8_t byte[64];
    }raw;
    struct _toolbar  // a toolbar area was touched 
    {
      int16_t htoolbar;   // tooolbar handle 
      int16_t row;        // row # of cell
      int16_t column;     // column # of cell
      uint8_t touching;   // true (1) if a physical touch is active
      uint8_t on;         // true (1) if the cell is "on" 
                          // for a non-latched toolbar a cell is on while it is being touched 
                          // a latched toolbar' cell toggles between the on & off states with each touch
    }toolbar;

    struct _interactiveTouch  // co-ordinate data 
    {  
      uint32_t ticks;   // ticks since touch  
      uint32_t maxTicks; // tick count at which icon change will occur  
    }interactiveTouch ;     
    struct _sound
    {
      uint32_t fileIndex;
      uint32_t reserved1;
      uint32_t reserved2;
    }sound;
    struct _eval
    {
      uint16_t clicksRemaining;
      uint32_t packageExpired;
    }eval;


    //struct _mouseEvent mouseEvent;
    //struct _rawMouseEvent rawMouseEvent;
#ifdef UPDD_GESTURES
#include "gestures_event.inl" 
#endif

    struct _config
    {
      uint16_t configEventType;
      uint16_t configEventLevel;
      union _ce
      {
        uint8_t configText[64 - sizeof(uint32_t)];
        uint32_t signalValue;
      }ce;
      struct _internal
      {
        uint32_t v1;
        uint32_t v2;
        uint32_t v3;
      }internal;
      int64_t originatingPID;
    }config;

  }pe; 
};


#ifndef __cplusplus
typedef struct _HIDPacket _HIDPacket;
#endif

#define UPDD_VHID_REPORT_ID_TOUCH 1
#define UPDD_VHID_REPORT_ID_KEYBOARD 2
#define UPDD_VHID_REPORT_ID_TOUCH_MOUSE 3
#define UPDD_VHID_REPORT_ID_PEN 6
#define UPDD_VHID_REPORT_ID_REGULAR_MOUSE 4
#define UPDD_VHID_MAX_X 32767
#define UPDD_VHID_MAX_Y 32767

struct _HIDPacket 
{
  uint8_t report_id;
  union _h
  {
    struct _touch
    {
      struct _contact
      {
        uint8_t touching : 1;
        uint8_t unused:2;
        uint8_t contact_number:5;
        uint16_t x;
        uint16_t unused_2;
        uint16_t y;
        uint16_t w;
        uint16_t h;
      }contact[5];
      uint8_t scan_rate;
      uint8_t unused;
      uint8_t contact_count;
    }touch;
    struct _pen
    {

      uint8_t in_range : 1;
      uint8_t invert : 1;
      uint8_t unused : 1;
      uint8_t eraser : 1;
      uint8_t barrel : 1;
      uint8_t tip : 1;

      uint16_t x;
      uint16_t y;
      uint16_t z;
      uint16_t unused_2;
      uint8_t dummy[10];
    }pen; 
    struct _touch_mouse
    {
      uint8_t button_left : 1;
      uint8_t button_right : 1;
      uint8_t button_middle : 1;
      uint8_t unused : 5;
      uint16_t x;
      uint16_t y;
    }touch_mouse;
    struct _regular_mouse
    {
      uint8_t button_left : 1;
      uint8_t button_right : 1;
      uint8_t button_middle : 1;
      uint8_t unused : 5;
      int8_t x;
      int8_t y;
    }regular_mouse;
    struct _keyboard
    {
      uint8_t modifier_lctrl  : 1;
      uint8_t modifier_lshift : 1;
      uint8_t modifier_lalt   : 1;
      uint8_t modifier_lmeta  : 1;
      uint8_t modifier_rctrl  : 1;
      uint8_t modifier_rshift : 1;
      uint8_t modifier_ralt   : 1;
      uint8_t modifier_rmeta  : 1;
      uint8_t unused;
      uint8_t key[6];
      uint8_t unused2[50];
    }keyboard;
  }h;
};

#pragma pack (pop)

#endif // TBAPIDEFNS_H

#ifndef DRIVERSTRINGS	

//function pointer definition for data callback 
#ifdef ENVIRONMENT64
typedef void (TBCALL *TB_EVENT_CALL)(unsigned long long context, _PointerEvent* aEvent);
#else 
typedef void (TBCALL* TB_EVENT_CALL)(unsigned long context, _PointerEvent* aEvent);
#endif

//function pointer definition for event source 
typedef void (TBCALL* TB_EVENT_CALL_SOURCE)(_PointerEvent* aEvent);

// V6 Migration note TBApiInit, TBApiInitEx, TBApiTerminate and TBApiDefaultSettingsPath are no longer used 

//void TBAPI TBApiOpen(int argc, char* argv[]);
void TBAPI TBApiOpen();
// Establishes a connection to the device driver
// most API functions require an open connection
// NB only call this once in your program, typically at startup

// note that the connection to the driver is performed asynchronously
// use TBApiIsDriverConnected to check the status of the connection



void TBAPI TBApiClose();
// Closes the connection to the device driver
// NB only call this once in your program, typically at termination


TBBOOL TBAPI TBApiIsDriverConnected();
// Returns a TBBOOL indication of whether a driver connection is in place
// ie has TBApiOpen been sucessfully actioned.
// NB because this API dispatches Qt events, this should only be used in a Qt application before
// any signal / slot connections are made, non-Qt applications are not subject to this limitation
// to get the same functionality in a Qt application after any signal / slot connections are made
// use TBApiIsDriverConnectedNoDispatch

TBBOOL TBAPI TBApiIsDriverConnectedNoDispatch();
// funtionally identical to TBApiIsDriverConnected but implemented to be safe to use
// in Qt applications after any signal / slot connections are made


TBBOOL TBAPI TBApiGetDriverVersion(TBCHAR* aVersion);
// returns -- 0 = fail, 1 = OK
// aVersion must point to an address to receive the version number of the driver, 16 bytes should be allocated 

void TBAPI TBApiGetApiVersion(TBCHAR* aVersion);
// aVersion must point to an address to receive the version number of the API, 16 bytes should be allocated 


// --------------------------------------------------------------- //
//         a number of functions require a device id               // 
//   the following family of functions provide valid device id's   // 
// --------------------------------------------------------------- // 
// 
 
HTBDEVICE TBAPI   TBApiGetRelativeDevice(int o);
// this api simply gets the device by it's order in the internal device list 
// typically used to get the only device in a single device system eg 
//
//  HTBDEVICE device = TBApiGetRelativeDevice(0);
//
//  or to enumerate all devices eg
//
//  HTBDEVICE device = TBApiGetRelativeDevice(0);
//  for(int i=0; device != TB_INVALID_HANDLE_VALUE;) 
//  {
//      DoSomethingWithDevice(device);
//      device = TBApiGetRelativeDevice(++i);
//  }
//
//  a return value of TB_INVALID_HANDLE_VALUE means that the requested device does not exist

int TBAPI TBApiGetRelativeDeviceFromHandle( HTBDEVICE aDeviceHandle); 
// this api performs the opposite role to TBApiGetRelativeDevice
// Given a Device handle the (zero based) position in the list is returned 
// a return value of -1 means that the requested device does not exist

HTBDEVICE TBAPI   TBApiGetRelativeDeviceExcludeHidden(int o);

TBBOOL TBAPI TBApiGetRotate(HTBDEVICE aDeviceHandle, int32_t* aRotate);
// returns (in aRotate) the rotation factor associated with the device
// returns -- 0 = fail, 1 = OK
//


TBBOOL TBAPI TBApiMousePortInterfaceEnable(HTBDEVICE UNUSED, TBBOOL aState);
// Enable / disables the mouse port interface 
// if the mouse port interface is disabled, the driver functions 
// normally, except that the mouse pointer is not moved and 
// mouse button clicks are not emulated. Data can be read via the api
// returns -- 0 = fail, 1 = OK

#ifdef ENVIRONMENT64
TBBOOL TBAPI TBApiRegisterEvent(HTBDEVICE aDeviceHandle, unsigned long long aContext, unsigned long aTypes, TB_EVENT_CALL aFunc);
#else
TBBOOL TBAPI TBApiRegisterEvent(HTBDEVICE aDeviceHandle, unsigned long aContext, unsigned long aTypes, TB_EVENT_CALL aFunc);
#endif
// Informs the API that a function is to be used a callback function for 
// the specified type(s) of data 
// The context value is passed unchanged to the callback function for identification purposes 
// All functions registered MUST be unregistered with TBApiUnregisterEvent
// before the program terminates
//
// In the following example CBFunc is called whenever pointer co-ordinates are processed
// by relative device 1 until TBApiUnregisterEvent is called
//
// **USAGE NOTE**
// the callback function is executed in the context of a dedicated thread 
// therefore only thread safe (reentrant) functions should be called from the callback function
// many windowing api functions are non-reentrant
// if you need to call non-reentrant functions you need to provide synchronisation management 
// a common way to achieve this is to post a message to the primary process thread and perform 
// all non-reentrant operations from the primary thread
//
// example 1; register callback for the first device found
// 
// HTBDEVICE hd = TBApiGetRelativeDevice(0);
//
// TBApiRegisterEvent(hd,0,_EventTypeXY,CBFunc); 
//
// .....
//
// void TBAPI CBFunc(unsigned long context, _PointerData* data)
// {
//      printf("device %d generated x=%d y=%d\n",(int)data->device,(int)data->xy->rawx,(int)data->xy->rawy);
// }
//
// To get data for all devices 0
// 
// example 2; register callback for all devices
// 
// TBApiRegisterEvent(0,0,_EventTypeXY,CBFunc); 
// 
// returns -- 0 = fail, 1 = OK

TBBOOL TBAPI TBApiRegisterCallbackAsTouchDelegate(TB_EVENT_CALL aFunc);
// Indicate that the specified callback function is to receive delegated touch events only // 
// returns -- 0 = fail, 1 = OK

TBBOOL TBAPI TBApiUnregisterEvent(TB_EVENT_CALL aFunc);
// Removes the specified function from the list of registered callbacks. 
// 
// eg to unregister the callback specified in the above example 
// 
// TBApiUnregisterEvent(CBFunc); 
//
// returns -- 0 = fail, 1 = OK


TBBOOL TBAPI TBApiUnregisterEventContext(unsigned long aContext);
// Removes the specified context from the list of registered callbacks. 
// 
// eg to unregister the callback specified in the above example 
// 
// TBApiUnregisterEvent(0); 
//
// returns -- 0 = fail, 1 = OK


// a set of functions to set / retrieve UPDD settings
TBBOOL TBAPI TBApiGetSetting(HTBDEVICE aHandle,const TBCHAR* aName, TBCHAR* aSZ, int lBuff);

TBBOOL TBAPI TBApiGetSettingAsInt(HTBDEVICE aHandle,const TBCHAR* aName, int32_t* val);

TBBOOL TBAPI TBApiSetSetting(HTBDEVICE aHandle, const TBCHAR* aName, const TBCHAR* aSZ, TBBOOL aDeviceSpecific);

TBBOOL TBAPI TBApiSetSettingFromInt(HTBDEVICE aHandle, const TBCHAR* aName, int32_t val, TBBOOL aDeviceSpecific);

// set the default value of a setting for a specified controller handle 
// controller handle can be "*" for all controllers 
// any non-default value is cleared,IE this value becomes effective for any device instances of this controller type 
TBBOOL TBAPI TBApiSetDefault(const TBCHAR* aController, const TBCHAR*  aSetting, const TBCHAR* aValue);

// this variant retrieves the default setting for the controller type as opposed to a specific installed instance of the controller
TBBOOL TBAPI TBApiGetControllerSetting(int aControllerHandle, const TBCHAR* aName, TBCHAR* aSZ, int lBuff);


// this variant gets a setting from the bootstrap file updd.ini given the section and setting name
// it is available before a driver connection is available
void TBAPI TBApiGetBootstrapSetting(const TBCHAR* aSection, const TBCHAR* aName, TBCHAR* aSZ, int lBuff);

TBBOOL TBAPI TBApiRemove(HTBDEVICE aHandle, const TBCHAR* aName);

// get the length of buffer needed to hold any setting as a NULL terminated string
TBBOOL TBAPI TBApiGetSettingSize(HTBDEVICE aHandle, const TBCHAR* aName, int* lBuff);
TBBOOL TBAPI TBApiGetControllerSettingSize(HTBDEVICE aHandle, const TBCHAR* aName, int* lBuff);


// add a new controller to the device list

// aControllerID               :  the handle of the controller definition as, for example returned by upddutils controllers 
// aDeviceName                 :  a name to identify the entry, pass NULL to assign an auto generated id
// aNewHandle                  :  address to receive he new device handle, can be NULL

TBBOOL TBAPI TBApiAddDevice(int aControllerID, const TBCHAR* aDeviceName, HTBDEVICE* aNewHandle);

TBBOOL TBAPI TBApiDeleteDevice(HTBDEVICE aDevice);

void TBAPI TBApiEnableApiTrace(TBBOOL aEnable);

 
TBBOOL TBAPI TBApiPostPacketBytes(HTBDEVICE aDevice, const char* aData);
TBBOOL TBAPI TBApiPostPacketBytesEx(HTBDEVICE aDevice, const char* aData, uint32_t aTimestamp);

// generate a touch on the selected device 
// aDevice the handle to the device to be used 
// x the x co-ordinate to be posted 
// x the y co-ordinate to be posted 
// st the sylus or contact number for multi touch
// touching: true to start or continue a touch: false ends a touch if one is active
// the x and y range depends on the controller definition, 

// NB this api operates asynchronously 
TBBOOL TBAPI TBApiInjectTouch(HTBDEVICE aDevice, int x, int y, int st, TBBOOL touching);

#ifdef UPDD_API_ALPHA // alpha test api's implemented but subject to change do not rely on this being unchanged in a subsequent build
TBBOOL TBAPI TBApiInjectTouchEx(HTBDEVICE aDevice, uint32_t x, uint32_t y, uint32_t st, TBBOOL touching, uint64_t aInjectFlags);
#endif


////////////////////////////////////////////////////////////////////////////////////////////////////
/// get the position of the primary monitor 
/// and the position and size of a monitor associated with a specified updd device 
// ** OBSOLETE 
// use the following instead
//int32_t m;
//TBApiGetSettingAsInt(aDevice, "monitor_number", &m); 
//TBApiGetMonitorMetricsForMonitor(m,....)
////////////////////////////////////////////////////////////////////////////////////////////////////
//void TBAPI TBApiGetMonitorMetrics(unsigned aDevice,
//                                  long* aPrimaryMonitorWidth, long* aPrimaryMonitorHeight,
//                                  long* aMonitorWidth, long* aMonitorHeight, long* aMonitorLeft, long* aMonitorTop);

////////////////////////////////////////////////////////////////////////////////////////////////////
/// get the  position and size of a monitor 
////////////////////////////////////////////////////////////////////////////////////////////////////

TBBOOL TBAPI TBApiGetMonitorMetricsForMonitor(unsigned aMonitor,
                                            long* aMonitorWidth, long* aMonitorHeight, long* aMonitorLeft, long* aMonitorTop);

////////////////////////////////////////////////////////////////////////////////////////////////////
/// get extended info about an api error, currently only applicable to  TBApiReadEEPROM / TBApiWriteEEPROM

////////////////////////////////////////////////////////////////////////////////////////////////////

void TBAPI TBApiGetLastError(TBCHAR* aMsg, int aMaxLength);

////////////////////////////////////////////////////////////////////////////////////////////////////
/// record a program as running or check if already running 
//  mainly used to limit client applications to a single instance
//
// arguments:- 
//           aProgramName:       a name to uniquley identify the program
//           aRegisterAsRunning: if true then record the current process along with the specified name in the list of running programs
//           aFailIfRunning:     if true and another instance of the named program is running the api call is failed
//                               the api call returns false and the process is not recorded in the running list
//           aSignalRunningApps: it true, an event of type _EventConfiguration is sent to existing instancesof the named program
//                               the field configEventType is set to CONFIG_EVENT_CONCURRENCY_SIGNAL
//
//  Example:
//          int main(...) 
//          {
//            ...
//            if (!TBApiRegisterProgram("UPDD Daemon", true, true, false))
//            {
//              TBApiClose();
//              return(1);
//            }
//
////////////////////////////////////////////////////////////////////////////////////////////////////

TBBOOL TBAPI TBApiRegisterProgram(const TBCHAR* aProgramName,TBBOOL aRegisterAsRunning, TBBOOL aFailIfRunning, TBBOOL aSignalRunningApps);

TBBOOL TBAPI TBApiRegisterProgramEx(const TBCHAR* aProgramName, TBBOOL aRegisterAsRunning, TBBOOL aFailIfRunning, TBBOOL aSignalRunningApps, uint8_t aPriority);

////////////////////////////////////////////////////////////////////////////////////////////////////
TBBOOL TBAPI TBApiLicence(const TBCHAR* aLicenceKey);


////////////////////////////////////////////////////////////////////////////////////////////////////
/// pass  PointerEvent callback to registered client apps  
////////////////////////////////////////////////////////////////////////////////////////////////////

TBBOOL TBAPI TBApiPostPointerEvent(_PointerEvent* aEvent);


////////////////////////////////////////////////////////////////////////////////////////////////////
/// is device connected TRUE / FALSE
/// for RS232 devices this refers to whether the driver has an open connection to the com port 
////////////////////////////////////////////////////////////////////////////////////////////////////
TBBOOL TBAPI TBApiIsDeviceConnected(HTBDEVICE aDeviceHandle, TBBOOL* aConnected);


////////////////////////////////////////////////////////////////////////////////////////////////////
/// get the maximum theoretical Z value
////////////////////////////////////////////////////////////////////////////////////////////////////
// obsolete 
//TBBOOL TBAPI TBApiGetMaxZ(HTBDEVICE aHandle, int32_t* aMaxZ);


////////////////////////////////////////////////////////////////////////////////////////////////////
/// issue an HID set feature request to the device
////////////////////////////////////////////////////////////////////////////////////////////////////


TBBOOL TBAPI TBApiHidSetFeature(
  HTBDEVICE aHandle,
  int aInterface,
  const void*  aReportBuffer,
  uint32_t  aReportBufferLength
  );


////////////////////////////////////////////////////////////////////////////////////////////////////
/// issue an HID get feature request to the device
////////////////////////////////////////////////////////////////////////////////////////////////////


TBBOOL TBAPI TBApiHidGetFeature(
  HTBDEVICE aDevice,
  int aInterface,
  void*  aReportBuffer,
  uint32_t  aReportBufferLength
  );


////////////////////////////////////////////////////////////////////////////////////////////////////
// get the names of all settings matching a pattern
//  aIncludeUnused: if true (1) indicates that all known matching names are to be returned as opposed to those currently used in the package 
//  if a null value is passed for aSZ, the return value addressed by lBuff will be set to the required buffer size 
// the list of names is returned as a multi line string (separated by '\n' characters) 
////////////////////////////////////////////////////////////////////////////////////////////////////

TBBOOL TBAPI TBApiGetSettings(HTBDEVICE aHandle, TBBOOL aIncludeUnused, const TBCHAR* aPattern, TBCHAR* aSZ, int* lBuff);


////////////////////////////////////////////////////////////////////////////////////////////////////
// get the help text associated with a named setting 
//  if a null value is passed for aSZ, the return value addressed by lBuff will be set to the required buffer size 
////////////////////////////////////////////////////////////////////////////////////////////////////


TBBOOL TBAPI TBApiGetSettingHelp(const TBCHAR* aName, TBCHAR* aSZ, int* lBuff);


////////////////////////////////////////////////////////////////////////////////////////////////////
// launch the calibration tool 
// mode must be one of: calibrate
//                      identify
//                      toolbar
//                      configure
// for calibrate a device handle is required toolbar index is ignored 
// for identify device handle and toolbar index are ignored 
// for configure device handle and toolbar index are ignored 
// for toolbar both are required 
////////////////////////////////////////////////////////////////////////////////////////////////////
TBBOOL TBAPI TBApiCalibrate(HTBDEVICE aHandle, int aToolbarIndex, TBCHAR* aMode);

////////////////////////////////////////////////////////////////////////////////////////////////////
// export settings  
// aDevices is a comma separate list of device handles to support OR * for all real (non zero) devices OR ** for all devices + NODEVICE 
// aNames is a comma separated list of setting names to export; can use standard wildcard characters * or ? 
// aTargetFileName  is the path to the export file 
// aFailIfExists if true and the target file exists the API call will fail (return false) 
////////////////////////////////////////////////////////////////////////////////////////////////////
TBBOOL TBAPI TBApiExportSettings(const TBCHAR* aDevices, const TBCHAR* aNames, const TBCHAR* aTargetFileName, TBBOOL aFailIfExists);

////////////////////////////////////////////////////////////////////////////////////////////////////
// import settings  previouly exported by TBApiExportSettings
// aSourceFileName  is the path to the import file 
////////////////////////////////////////////////////////////////////////////////////////////////////
TBBOOL TBAPI TBApiImportSettings(const TBCHAR* aSourceFileName);


////////////////////////////////////////////////////////////////////////////////////////////////////
// an alternate means to retrieve settings 
// this implementation is more performant than TBApiGetSetting and its variants 
// all values that match aPattern are retrieved in one call to the driver when aIndex == 0 
// the internal storage is released when aIndex >= number of values
// so it is important to always start with aIndex=0 
// due to the cached storage this function is NOT thread safe
// a previous cache allocation will be deleted if aIndex == 0
// when aIndex == number of values TRUE is returned and an empty string is given in aName and aValue 
// in the event that the buffers passed are too small for a name or value FALSE is returned 
// and TBApiGetLastError will give "Error: Insufficient buffer size"
//
//  Example:
//
//     Prints all nodevice settings starting with "m"   
//
//            char name[256];
//            char value[1024];
//            for (unsigned n = 0; ; n++)
//            {
//              if (!TBApiGetSettingByIndex(0, "m*", n,  name, sizeof(name), value, sizeof(value)))
//              {
//                char msg[1024];
//                TBApiGetLastError(msg, sizeof(msg));
//                cerr << msg << endl;
//                break;
//              }
//              if (!strlen(name))
//              {
//               break;
//              }
//              cout << name << ": " << value << endl;
//            }
////////////////////////////////////////////////////////////////////////////////////////////////////
TBBOOL TBAPI TBApiGetSettingByIndex(HTBDEVICE aHandle, const TBCHAR* aPattern, uint16_t aIndex, TBCHAR* aName, int lName, TBCHAR* aValue, int lValue);


////////////////////////////////////////////////////////////////////////////////////////////////////
// Reset settings to original values excluding reserved values such as registration, device binding 
// internal (private.*) and device counts  
////////////////////////////////////////////////////////////////////////////////////////////////////
TBBOOL TBAPI TBApiResetSettings();

TBBOOL TBAPI TBApiGetSettingByIndex(HTBDEVICE aHandle, const TBCHAR* aPattern, uint16_t aIndex, TBCHAR* aName, int lName, TBCHAR* aValue, int lValue);

////////////////////////////////////////////////////////////////////////////////////////////////////
// get a toolbar based on it's (zero based) index
// a return value of TB_INVALID_HANDLE_VALUE means that a toolbar does not exist at the 
// specified index
////////////////////////////////////////////////////////////////////////////////////////////////////
HTBTOOLBAR TBAPI   TBApiGetRelativeToolbar(int o);

////////////////////////////////////////////////////////////////////////////////////////////////////
// Create a new toolbar, default values are use for the required fields not found in the argument 
// list, to provide values for these fields use TBApiSetToolbarSetting
// note that, unlike earlier implementations, the returned handle is immutable
// so can be used as a permanent reference to this toolbar
// 
// the default values applied are
// columns=1
// rows=1
// off_screen=0
// enabled=1
// monitor_number=1
// latched=0
// hold_touch=1
// active_whilst_calibrating=0
// 
////////////////////////////////////////////////////////////////////////////////////////////////////
HTBTOOLBAR TBAPI TBApiAddToolbar(const TBCHAR* aName, uint16_t x, uint16_t y, uint16_t width, uint16_t height);

////////////////////////////////////////////////////////////////////////////////////////////////////
// Delete a toolbar 
////////////////////////////////////////////////////////////////////////////////////////////////////
TBBOOL TBAPI TBApiRemoveToolbar(HTBTOOLBAR aToolbarHandle);

////////////////////////////////////////////////////////////////////////////////////////////////////
// Get a named setting for a toolbar as a string
// the string buffer (aSZ) must be large enough to hold the returned value + a terminating null byte
// this size of this buffer must be specified in lBuff
////////////////////////////////////////////////////////////////////////////////////////////////////
TBBOOL TBAPI TBApiGetToolbarSetting(HTBTOOLBAR aToolbarHandle, const TBCHAR* aName, TBCHAR* aSZ, int lBuff);

////////////////////////////////////////////////////////////////////////////////////////////////////
// Get a named setting for a toolbar as a signed int
// note, this is a convenience function only, all settings are held internally as strings 
// and can be retrieved with TBApiGetToolbarSetting
////////////////////////////////////////////////////////////////////////////////////////////////////
TBBOOL TBAPI TBApiGetToolbarSettingAsInt(HTBTOOLBAR aToolbarHandle, const TBCHAR* aName, int32_t* val);

////////////////////////////////////////////////////////////////////////////////////////////////////
// Set a named setting for a toolbar 
////////////////////////////////////////////////////////////////////////////////////////////////////
TBBOOL TBAPI TBApiSetToolbarSetting(HTBTOOLBAR aToolbarHandle, const TBCHAR* aName, const TBCHAR* aSZ);

////////////////////////////////////////////////////////////////////////////////////////////////////
// Enable a toolbar based on the passed handle or pass 0 to enable all toolbars
////////////////////////////////////////////////////////////////////////////////////////////////////
TBBOOL TBAPI TBApiEnableToolbars(HTBTOOLBAR aToolbarHandle);

////////////////////////////////////////////////////////////////////////////////////////////////////
// Enable a toolbar based on the passed handle or pass 0 to disable all toolbars
////////////////////////////////////////////////////////////////////////////////////////////////////
TBBOOL TBAPI TBApiDisableToolbars(HTBTOOLBAR aToolbarHandle);

////////////////////////////////////////////////////////////////////////////////////////////////////
// A pair of functions to simply dispatching of api callback events to the primary thread
// This is particularly useful in more complicated applications and allows this logic to 
// implemented on a per application basis wihout losing the flexibility of the api's
// dispatching features
// Create a callback function of type TB_EVENT_CALL_SOURCE and pass its address to TBApiRegisterEventSource
// This function should post every received event for processing in the application's primary thread 
// using a client appropriate method. 
// The receiving handler in the primary thread should pass the received values to TBApiEventSink
// Having done so, callbacks registered with TBApiRegisterEvent will execute in the primary thread context
////////////////////////////////////////////////////////////////////////////////////////////////////
TBBOOL TBAPI TBApiRegisterEventSource(TB_EVENT_CALL_SOURCE aFunc);
#ifdef ENVIRONMENT64
TBBOOL TBAPI TBApiEventSink(unsigned long long context, _PointerEvent* aEvent);
#else
TBBOOL TBAPI TBApiEventSink(unsigned long context, _PointerEvent* aEvent);
#endif

TBBOOL TBAPI TBApiPostHIDPacket(HTBDEVICE aHandle, TBBOOL aDirect, _HIDPacket* aPacket);


#endif // DRIVERSTRINGS

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef TBAPIDLL_EXPORTS 
#ifdef _WIN32
#pragma warning(default:4518)
#endif
#endif



#endif







