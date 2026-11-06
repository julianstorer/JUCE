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

#ifdef JUCE_GUI_BASICS_H_INCLUDED
 /* When you add this cpp file to your project, you mustn't include it in a file where you've
    already included any other headers - just put it inside a file on its own, possibly with your config
    flags preceding it, but don't include anything else. That also includes avoiding any automatic prefix
    header files that the compiler may be using.
 */
 #error "Incorrect use of JUCE cpp file"
#endif

#define NS_FORMAT_FUNCTION(F,A) // To avoid spurious warnings from GCC

#define JUCE_CORE_INCLUDE_OBJC_HELPERS 1
#define JUCE_CORE_INCLUDE_COM_SMART_PTR 1
#define JUCE_CORE_INCLUDE_JNI_HELPERS 1
#define JUCE_CORE_INCLUDE_NATIVE_HEADERS 1
#define JUCE_EVENTS_INCLUDE_WIN32_MESSAGE_WINDOW 1
#define JUCE_GRAPHICS_INCLUDE_COREGRAPHICS_HELPERS 1
#define JUCE_GUI_BASICS_INCLUDE_XHEADERS 1
#define JUCE_GUI_BASICS_INCLUDE_SCOPED_THREAD_DPI_AWARENESS_SETTER 1

#include "juce_gui_basics.h"

#include <cctype>

//==============================================================================
#ifdef JUCE_DISPLAY_SPLASH_SCREEN
 JUCE_COMPILER_WARNING ("This version of JUCE does not use the splash screen, the flag JUCE_DISPLAY_SPLASH_SCREEN is ignored")
#endif

#ifdef JUCE_USE_DARK_SPLASH_SCREEN
 JUCE_COMPILER_WARNING ("This version of JUCE does not use the splash screen, the flag JUCE_USE_DARK_SPLASH_SCREEN is ignored")
#endif

//==============================================================================
#if JUCE_MAC
 #import <IOKit/pwr_mgt/IOPMLib.h>
 #import <MetalKit/MetalKit.h>

 #if JUCE_MAC_API_VERSION_MIN_REQUIRED_AT_LEAST (14, 4)
  #import <ScreenCaptureKit/ScreenCaptureKit.h>
 #endif

#elif JUCE_IOS
 #import <UserNotifications/UserNotifications.h>
 #import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
 #import <MetalKit/MetalKit.h>
 #import <UIKit/UIActivityViewController.h>

//==============================================================================
#elif JUCE_WINDOWS
 #include <commctrl.h>
 #include <commdlg.h>
 #include <d2d1_3.h>
 #include <d3d11_2.h>
 #include <dxgi1_3.h>
 #include <sapi.h>
 #include <vfw.h>
 #include <windowsx.h>
 #include <dwmapi.h>
 #include <dwrite_3.h>
 #include <dcomp.h>
 #include <shellscalingapi.h>

 #if JUCE_ETW_TRACELOGGING
  #include <TraceLoggingProvider.h>
  #include <evntrace.h>
 #endif

 #include <uiautomation.h>

 #undef new

 #if JUCE_WEB_BROWSER
  #include <exdisp.h>
  #include <exdispid.h>
 #endif

 #if ! JUCE_DONT_AUTOLINK_TO_WIN32_LIBRARIES
  #pragma comment(lib, "vfw32.lib")
  #pragma comment(lib, "imm32.lib")
  #pragma comment(lib, "comctl32.lib")
  #pragma comment(lib, "dwmapi.lib")
  #pragma comment(lib, "shcore.lib")

  // Link a newer version of the side-by-side comctl32 dll.
  // Required to enable the newer native message box and visual styles on vista and above.
  #pragma comment(linker,                             \
          "\"/MANIFESTDEPENDENCY:type='Win32' "       \
          "name='Microsoft.Windows.Common-Controls' " \
          "version='6.0.0.0' "                        \
          "processorArchitecture='*' "                \
          "publicKeyToken='6595b64144ccf1df' "        \
          "language='*'\""                            \
      )
  #if JUCE_OPENGL
   #pragma comment(lib, "OpenGL32.Lib")
   #pragma comment(lib, "GlU32.Lib")
  #endif

 #endif
#endif

//==============================================================================
#include <juce_graphics/native/juce_EventTracing.h>

#include "detail/juce_AccessibilityHelpers.h"
#include "detail/juce_ComponentHelpers.h"
#include "detail/juce_FocusHelpers.h"
#include "detail/juce_PointerState.h"
#include "detail/juce_CustomMouseCursorInfo.h"
#include "detail/juce_MouseInputSourceImpl.h"
#include "detail/juce_MouseInputSourceList.h"
#include "detail/juce_ScopedMessageBoxInterface.h"
#include "detail/juce_ScopedMessageBoxImpl.h"
#include "detail/juce_ScopedContentSharerInterface.h"
#include "detail/juce_ScopedContentSharerImpl.h"
#include "detail/juce_WindowingHelpers.h"
#include "detail/juce_AlertWindowHelpers.h"
#include "detail/juce_StandardCachedComponentImage.h"
#include "detail/juce_TopLevelWindowManager.h"
#include "detail/juce_FocusRestorer.h"

//==============================================================================
#if JUCE_IOS || JUCE_WINDOWS || JUCE_LINUX || JUCE_BSD
 #include "native/juce_MultiTouchMapper.h"
#endif

#if JUCE_ANDROID || JUCE_WINDOWS || JUCE_IOS || JUCE_UNIT_TESTS
 #include "native/accessibility/juce_AccessibilityTextHelpers.h"
#endif

#if JUCE_MAC || JUCE_IOS
 #include "native/accessibility/juce_AccessibilitySharedCode_mac.mm"
 #include "native/juce_CGMetalLayerRenderer_mac.h"

 #if JUCE_IOS
  #include "native/juce_UIViewComponentPeer_ios.mm"
  #include "native/accessibility/juce_Accessibility_ios.mm"
  #include "native/juce_WindowUtils_ios.mm"
  #include "native/juce_Windowing_ios.mm"
  #include "native/juce_NativeMessageBox_ios.mm"
  #include "native/juce_NativeModalWrapperComponent_ios.h"
  #include "native/juce_FileChooser_ios.mm"

  #if JUCE_CONTENT_SHARING
   #include "native/juce_ContentSharer_ios.cpp"
  #endif

 #else
  #include "native/accessibility/juce_Accessibility_mac.mm"
  #include "native/juce_PerScreenDisplayLinks_mac.h"
  #include "native/juce_UPDDMultitouch_mac.mm"
  #include "native/juce_NSViewComponentPeer_mac.mm"
  #include "native/juce_WindowUtils_mac.mm"
  #include "native/juce_Windowing_mac.mm"
  #include "native/juce_NativeMessageBox_mac.mm"
  #include "native/juce_MainMenu_mac.mm"
  #include "native/juce_FileChooser_mac.mm"
  #include "detail/juce_ComponentPeerHelpers.h"
  #include "detail/juce_ComponentPeerHelpers.cpp"
 #endif

 #include "native/juce_MouseCursor_mac.mm"

#elif JUCE_WINDOWS
 #include <juce_core/detail/juce_FunctionPointerDestructor.h>
 #include <juce_graphics/native/juce_Direct2DMetrics_windows.h>
 #include <juce_graphics/native/juce_Direct2DGraphicsContext_windows.h>
 #include <juce_graphics/native/juce_DirectX_windows.h>
 #include <juce_graphics/native/juce_Direct2DPixelDataPage_windows.h>
 #include <juce_graphics/images/juce_ImagePixelDataNativeExtensions.h>
 #include <juce_graphics/native/juce_Direct2DGraphicsContextImpl_windows.h>
 #include <juce_graphics/native/juce_Direct2DImage_windows.h>
 #include <juce_graphics/native/juce_Direct2DImageContext_windows.h>

 #include "native/accessibility/juce_WindowsUIAWrapper_windows.h"
 #include "native/accessibility/juce_AccessibilityElement_windows.h"
 #include "native/accessibility/juce_UIAHelpers_windows.h"
 #include "native/accessibility/juce_UIAProviders_windows.h"
 #include "native/accessibility/juce_AccessibilityElement_windows.cpp"
 #include "native/accessibility/juce_Accessibility_windows.cpp"
 #include "native/juce_Direct2DHwndContext_windows.h"
 #include "native/juce_Direct2DHwndContext_windows.cpp"
 #include "native/juce_WindowsHooks_windows.h"
 #include "native/juce_WindowUtils_windows.cpp"
 #include "native/juce_VBlank_windows.cpp"
 #include "native/juce_Windowing_windows.cpp"
 #include "native/juce_WindowsHooks_windows.cpp"
 #include "native/juce_NativeMessageBox_windows.cpp"
 #include "native/juce_DragAndDrop_windows.cpp"
 #include "native/juce_FileChooser_windows.cpp"

#elif JUCE_LINUX || JUCE_BSD
 #include "native/juce_XSymbols_linux.cpp"
 #include "native/juce_DragAndDrop_linux.cpp"

 JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wzero-as-null-pointer-constant")

 #include "native/juce_ScopedWindowAssociation_linux.h"
 #include "native/juce_WindowUtils_linux.cpp"
 #include "native/juce_Windowing_linux.cpp"
 #include "native/juce_NativeMessageBox_linux.cpp"
 #include "native/juce_XWindowSystem_linux.cpp"

 JUCE_END_IGNORE_WARNINGS_GCC_LIKE

 #include "native/juce_FileChooser_linux.cpp"

#elif JUCE_ANDROID

 #include "juce_core/files/juce_common_MimeTypes.h"
 #include "native/accessibility/juce_Accessibility_android.cpp"
 #include "native/juce_WindowUtils_android.cpp"
 #include "native/juce_Windowing_android.cpp"
 #include "native/juce_NativeMessageBox_android.cpp"
 #include "native/juce_FileChooser_android.cpp"

 #if JUCE_CONTENT_SHARING
  #include "native/juce_ContentSharer_android.cpp"
 #endif

#endif

//==============================================================================
// Depends on types defined in platform-specific windowing files
#include "mouse/juce_MouseCursor.cpp"

#if JUCE_UNIT_TESTS
 #include "native/accessibility/juce_AccessibilityTextHelpers_test.cpp"
#endif

//==============================================================================
#include "native/accessibility/juce_Accessibility.cpp"
#include "accessibility/juce_AccessibilityHandler.cpp"

#include "application/juce_Application.cpp"

#include "components/juce_Component.cpp"
#include "components/juce_ComponentListener.cpp"
#include "components/juce_FocusTraverser.cpp"
#include "components/juce_ModalComponentManager.cpp"

#include "desktop/juce_Desktop.cpp"
#include "desktop/juce_Displays.cpp"

#include "detail/juce_AccessibilityHelpers.cpp"

#include "filebrowser/juce_ContentSharer.cpp"

#include "keyboard/juce_KeyboardFocusTraverser.cpp"

#include "mouse/juce_MouseInputSource.cpp"

#include "native/juce_ScopedDPIAwarenessDisabler.cpp"

#include "windows/juce_NativeMessageBox.cpp"
#include "windows/juce_AlertWindow.cpp"
#include "windows/juce_ComponentPeer.cpp"
#include "windows/juce_ScopedMessageBox.cpp"
#include "windows/juce_TopLevelWindow.cpp"

#include "menus/juce_PopupMenu.cpp"

#include "misc/juce_DropShadower.cpp"
