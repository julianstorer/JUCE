/*
  ==============================================================================

   This file is part of the JUCE 6 technical preview.
   Copyright (c) 2020 - Raw Material Software Limited

   You may use this code under the terms of the GPL v3
   (see www.gnu.org/licenses).

   For this technical preview, this file is not subject to commercial licensing.

   JUCE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

  ==============================================================================
*/

namespace juce
{

    bool MouseInterceptor::interceptMouseDown (const MouseEvent&) { return false; }
    bool MouseInterceptor::interceptMouseDrag (const MouseEvent&) { return false; }
    bool MouseInterceptor::interceptMouseUp (const MouseEvent&) { return false; }

} // namespace juce
