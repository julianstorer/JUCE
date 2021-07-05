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

//==============================================================================
#if JUCE_MAC
 #import <WebKit/WebKit.h>
 #import <IOKit/pwr_mgt/IOPMLib.h>

 #if JUCE_SUPPORT_CARBON
  #import <Carbon/Carbon.h> // still needed for SetSystemUIMode()
 #endif

#elif JUCE_IOS
 #if JUCE_PUSH_NOTIFICATIONS && defined (__IPHONE_10_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_10_0
  #import <UserNotifications/UserNotifications.h>
 #endif

 #import <UIKit/UIActivityViewController.h>

//==============================================================================
#elif JUCE_WINDOWS
 #include <windowsx.h>
 #include <vfw.h>
 #include <commdlg.h>

 #if ! JUCE_MINGW
  #include <UIAutomation.h>
  #include <sapi.h>
 #endif

 #if JUCE_WEB_BROWSER
  #include <exdisp.h>
  #include <exdispid.h>
 #endif

 #if JUCE_MINGW
  #include <imm.h>
 #elif ! JUCE_DONT_AUTOLINK_TO_WIN32_LIBRARIES
  #pragma comment(lib, "vfw32.lib")
  #pragma comment(lib, "imm32.lib")

  #if JUCE_OPENGL
   #pragma comment(lib, "OpenGL32.Lib")
   #pragma comment(lib, "GlU32.Lib")
  #endif

  #if JUCE_DIRECT2D
   #pragma comment (lib, "Dwrite.lib")
   #pragma comment (lib, "D2d1.lib")
  #endif
 #endif
#endif

#include <set>

//==============================================================================
#define JUCE_ASSERT_MESSAGE_MANAGER_IS_LOCKED_OR_OFFSCREEN \
    jassert ((MessageManager::getInstanceWithoutCreating() != nullptr \
               && MessageManager::getInstanceWithoutCreating()->currentThreadHasLockedMessageManager()) \
              || getPeer() == nullptr);

namespace juce
{
    bool juce_areThereAnyAlwaysOnTopWindows();

    bool isEmbeddedInForegroundProcess (Component* c);

   #if ! JUCE_WINDOWS
    bool isEmbeddedInForegroundProcess (Component*) { return false; }
   #endif

    /*  Returns true if this process is in the foreground, or if the viewComponent
        is embedded into a window owned by the foreground process.
    */
    bool isForegroundOrEmbeddedProcess (Component* viewComponent)
    {
        return Process::isForegroundProcess() || isEmbeddedInForegroundProcess (viewComponent);
    }
}

#include "accessibility/juce_AccessibilityHandler.cpp"
#include "components/juce_Component.cpp"
#include "components/juce_ComponentListener.cpp"
#include "components/juce_FocusTraverser.cpp"
#include "mouse/juce_MouseInputSource.cpp"
#include "desktop/juce_Displays.cpp"
#include "desktop/juce_Desktop.cpp"
#include "components/juce_ModalComponentManager.cpp"
#include "mouse/juce_ComponentDragger.cpp"
#include "mouse/juce_DragAndDropContainer.cpp"
#include "mouse/juce_MouseCursor.cpp"
#include "mouse/juce_MouseEvent.cpp"
#include "mouse/juce_MouseInactivityDetector.cpp"
#include "mouse/juce_MouseListener.cpp"
#include "mouse/juce_MouseInterceptor.cpp"
#include "keyboard/juce_CaretComponent.cpp"
#include "keyboard/juce_KeyboardFocusTraverser.cpp"
#include "keyboard/juce_KeyListener.cpp"
#include "keyboard/juce_KeyPress.cpp"
#include "keyboard/juce_ModifierKeys.cpp"
#include "buttons/juce_ArrowButton.cpp"
#include "buttons/juce_Button.cpp"
#include "buttons/juce_DrawableButton.cpp"
#include "buttons/juce_HyperlinkButton.cpp"
#include "buttons/juce_ImageButton.cpp"
#include "buttons/juce_ShapeButton.cpp"
#include "buttons/juce_TextButton.cpp"
#include "buttons/juce_ToggleButton.cpp"
#include "buttons/juce_ToolbarButton.cpp"
#include "drawables/juce_Drawable.cpp"
#include "drawables/juce_DrawableComposite.cpp"
#include "drawables/juce_DrawableImage.cpp"
#include "drawables/juce_DrawablePath.cpp"
#include "drawables/juce_DrawableRectangle.cpp"
#include "drawables/juce_DrawableShape.cpp"
#include "drawables/juce_DrawableText.cpp"
#include "drawables/juce_SVGParser.cpp"
#include "filebrowser/juce_DirectoryContentsDisplayComponent.cpp"
#include "filebrowser/juce_DirectoryContentsList.cpp"
#include "filebrowser/juce_FileBrowserComponent.cpp"
#include "filebrowser/juce_FileChooser.cpp"
#include "filebrowser/juce_FileChooserDialogBox.cpp"
#include "filebrowser/juce_FileListComponent.cpp"
#include "filebrowser/juce_FilenameComponent.cpp"
#include "filebrowser/juce_FileSearchPathListComponent.cpp"
#include "filebrowser/juce_FileTreeComponent.cpp"
#include "filebrowser/juce_ImagePreviewComponent.cpp"
#include "filebrowser/juce_ContentSharer.cpp"
#include "layout/juce_ComponentAnimator.cpp"
#include "layout/juce_ComponentBoundsConstrainer.cpp"
#include "layout/juce_ComponentBuilder.cpp"
#include "layout/juce_ComponentMovementWatcher.cpp"
#include "layout/juce_ConcertinaPanel.cpp"
#include "layout/juce_GroupComponent.cpp"
#include "layout/juce_MultiDocumentPanel.cpp"
#include "layout/juce_ResizableBorderComponent.cpp"
#include "layout/juce_ResizableCornerComponent.cpp"
#include "layout/juce_ResizableEdgeComponent.cpp"
#include "layout/juce_ScrollBar.cpp"
#include "layout/juce_SidePanel.cpp"
#include "layout/juce_StretchableLayoutManager.cpp"
#include "layout/juce_StretchableLayoutResizerBar.cpp"
#include "layout/juce_StretchableObjectResizer.cpp"
#include "layout/juce_TabbedButtonBar.cpp"
#include "layout/juce_TabbedComponent.cpp"
#include "layout/juce_Viewport.cpp"
#include "lookandfeel/juce_LookAndFeel.cpp"
#include "lookandfeel/juce_LookAndFeel_V2.cpp"
#include "lookandfeel/juce_LookAndFeel_V1.cpp"
#include "lookandfeel/juce_LookAndFeel_V3.cpp"
#include "lookandfeel/juce_LookAndFeel_V4.cpp"
#include "menus/juce_MenuBarComponent.cpp"
#include "menus/juce_BurgerMenuComponent.cpp"
#include "menus/juce_MenuBarModel.cpp"
#include "menus/juce_PopupMenu.cpp"
#include "positioning/juce_MarkerList.cpp"
#include "positioning/juce_RelativeCoordinate.cpp"
#include "positioning/juce_RelativeCoordinatePositioner.cpp"
#include "positioning/juce_RelativeParallelogram.cpp"
#include "positioning/juce_RelativePoint.cpp"
#include "positioning/juce_RelativePointPath.cpp"
#include "positioning/juce_RelativeRectangle.cpp"
#include "properties/juce_BooleanPropertyComponent.cpp"
#include "properties/juce_ButtonPropertyComponent.cpp"
#include "properties/juce_ChoicePropertyComponent.cpp"
#include "properties/juce_PropertyComponent.cpp"
#include "properties/juce_PropertyPanel.cpp"
#include "properties/juce_SliderPropertyComponent.cpp"
#include "properties/juce_TextPropertyComponent.cpp"
#include "properties/juce_MultiChoicePropertyComponent.cpp"
#include "widgets/juce_ComboBox.cpp"
#include "widgets/juce_ImageComponent.cpp"
#include "widgets/juce_Label.cpp"
#include "widgets/juce_ListBox.cpp"
#include "widgets/juce_ProgressBar.cpp"
#include "widgets/juce_Slider.cpp"
#include "widgets/juce_TableHeaderComponent.cpp"
#include "widgets/juce_TableListBox.cpp"
#include "widgets/juce_TextEditor.cpp"
#include "widgets/juce_ToolbarItemComponent.cpp"
#include "widgets/juce_Toolbar.cpp"
#include "widgets/juce_ToolbarItemPalette.cpp"
#include "widgets/juce_TreeView.cpp"
#include "windows/juce_AlertWindow.cpp"
#include "windows/juce_CallOutBox.cpp"
#include "windows/juce_ComponentPeer.cpp"
#include "windows/juce_DialogWindow.cpp"
#include "windows/juce_DocumentWindow.cpp"
#include "windows/juce_ResizableWindow.cpp"
#include "windows/juce_ThreadWithProgressWindow.cpp"
#include "windows/juce_TooltipWindow.cpp"
#include "windows/juce_TopLevelWindow.cpp"
#include "commands/juce_ApplicationCommandInfo.cpp"
#include "commands/juce_ApplicationCommandManager.cpp"
#include "commands/juce_ApplicationCommandTarget.cpp"
#include "commands/juce_KeyPressMappingSet.cpp"
#include "application/juce_Application.cpp"
#include "misc/juce_BubbleComponent.cpp"
#include "misc/juce_DropShadower.cpp"
#include "misc/juce_JUCESplashScreen.cpp"

#include "layout/juce_FlexBox.cpp"
#include "layout/juce_GridItem.cpp"
#include "layout/juce_Grid.cpp"

#if JUCE_IOS || JUCE_WINDOWS
 #include "native/juce_MultiTouchMapper.h"
#endif

namespace juce
{

static const juce::Identifier disableAsyncLayerBackedViewIdentifier { "disableAsyncLayerBackedView" };

/** Used by the macOS and iOS peers. */
void setComponentAsyncLayerBackedViewDisabled (juce::Component& comp, bool shouldDisableAsyncLayerBackedView)
{
    comp.getProperties().set (disableAsyncLayerBackedViewIdentifier, shouldDisableAsyncLayerBackedView);
}

/** Used by the macOS and iOS peers. */
bool getComponentAsyncLayerBackedViewDisabled (juce::Component& comp)
{
    return comp.getProperties()[disableAsyncLayerBackedViewIdentifier];
}

} // namespace juce

#if JUCE_MAC || JUCE_IOS
 #if JUCE_IOS
  #include "native/juce_ios_UIViewComponentPeer.mm"
  #include "native/juce_ios_Windowing.mm"
  #include "native/juce_ios_FileChooser.mm"

  #if JUCE_CONTENT_SHARING
   #include "native/juce_ios_ContentSharer.cpp"
  #endif

 #else
  #include "native/accessibility/juce_mac_Accessibility.mm"
  #include "native/juce_mac_NSViewComponentPeer.mm"
  #include "native/juce_mac_Windowing.mm"
  #include "native/juce_mac_MainMenu.mm"
  #include "native/juce_mac_FileChooser.mm"
 #endif

 #include "native/juce_mac_MouseCursor.mm"

#elif JUCE_WINDOWS

 #if ! JUCE_MINGW
  #include "native/accessibility/juce_win32_WindowsUIAWrapper.h"
  #include "native/accessibility/juce_win32_AccessibilityElement.h"
  #include "native/accessibility/juce_win32_UIAHelpers.h"
  #include "native/accessibility/juce_win32_UIAProviders.h"
  #include "native/accessibility/juce_win32_AccessibilityElement.cpp"
  #include "native/accessibility/juce_win32_Accessibility.cpp"
 #else
  namespace juce
  {
      namespace WindowsAccessibility
      {
          long getUiaRootObjectId()  { return -1; }
          bool handleWmGetObject (AccessibilityHandler*, WPARAM, LPARAM, LRESULT*) { return false; }
          void revokeUIAMapEntriesForWindow (HWND) {}
      }
  }
 #endif

 #include "native/juce_win32_Windowing.cpp"
 #include "native/juce_win32_DragAndDrop.cpp"
 #include "native/juce_win32_FileChooser.cpp"

#elif JUCE_LINUX || JUCE_BSD
 #include "native/x11/juce_linux_X11_Symbols.cpp"
 #include "native/x11/juce_linux_X11_DragAndDrop.cpp"

 JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wzero-as-null-pointer-constant")

 #include "native/juce_linux_Windowing.cpp"
 #include "native/x11/juce_linux_XWindowSystem.cpp"

 JUCE_END_IGNORE_WARNINGS_GCC_LIKE

 #include "native/juce_linux_FileChooser.cpp"

#elif JUCE_ANDROID
 #include "native/juce_android_Windowing.cpp"
 #include "native/juce_common_MimeTypes.cpp"
 #include "native/juce_android_FileChooser.cpp"

 #if JUCE_CONTENT_SHARING
  #include "native/juce_android_ContentSharer.cpp"
 #endif

#endif

#if ! JUCE_NATIVE_ACCESSIBILITY_INCLUDED
namespace juce
{
    class AccessibilityHandler::AccessibilityNativeImpl { public: AccessibilityNativeImpl (AccessibilityHandler&) {} };
    void AccessibilityHandler::notifyAccessibilityEvent (AccessibilityEvent) const {}
    void AccessibilityHandler::postAnnouncement (const String&, AnnouncementPriority) {}
    AccessibilityNativeHandle* AccessibilityHandler::getNativeImplementation() const { return nullptr; }
    AccessibilityHandler::AccessibilityNativeImpl* AccessibilityHandler::createNativeImpl (AccessibilityHandler&) { return nullptr; }
    void AccessibilityHandler::DestroyNativeImpl::operator() (AccessibilityHandler::AccessibilityNativeImpl*) const noexcept {}
    bool areAnyAccessibilityClientsActive() { return false; }
    void notifyAccessibilityEventInternal (const AccessibilityHandler&, InternalAccessibilityEvent) {}
}
#endif
