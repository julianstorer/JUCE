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

/*
  ==============================================================================

   Utility to turn a bunch of binary files into a .cpp file and .h file full of
   data so they can be built directly into an executable.

   Use this code at your own risk! It carries no warranty!

  ==============================================================================
*/

#include <JuceHeader.h>


//==============================================================================
static int addFile (const File& file,
                    const String& classname,
                    OutputStream& headerStream,
                    OutputStream& cppStream)
{
    MemoryBlock mb;
    file.loadFileAsData (mb);

    const String name (file.getFileName()
                           .replaceCharacter (' ', '_')
                           .replaceCharacter ('.', '_')
                           .retainCharacters ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_0123456789"));

    std::cout << "Adding " << name << ": "
              << (int) mb.getSize() << " bytes" << std::endl;

    headerStream << "    extern const char*  " << name << ";\r\n"
                    "    const int           " << name << "Size = "
                 << (int) mb.getSize() << ";\r\n\r\n";

    static int tempNum = 0;

    cppStream << "static const unsigned char temp" << ++tempNum << "[] = {";

    size_t i = 0;
    const uint8* const data = (const uint8*) mb.getData();

    while (i < mb.getSize() - 1)
    {
        if ((i % 40) != 39)
            cppStream << (int) data[i] << ",";
        else
            cppStream << (int) data[i] << ",\r\n  ";

        ++i;
    }

    cppStream << (int) data[i] << ",0,0};\r\n";

    cppStream << "const char* " << classname << "::" << name
              << " = (const char*) temp" << tempNum << ";\r\n\r\n";

    return (int) mb.getSize();
}

static bool isHiddenFile (const File& f, const File& root)
{
    return f.getFileName().endsWithIgnoreCase (".scc")
         || f.getFileName() == ".svn"
         || f.getFileName().startsWithChar ('.')
         || (f.getSize() == 0 && ! f.isDirectory())
         || (f.getParentDirectory() != root && isHiddenFile (f.getParentDirectory(), root));
}


//==============================================================================
int main (int argc, char* argv[])
{
    std::cout << std::endl << " BinaryBuilder!  Visit www.juce.com for more info." << std::endl;

    if (argc < 4 || argc > 5)
    {
        std::cout << " Usage: BinaryBuilder  sourcedirectory targetdirectory targetclassname [optional wildcard pattern]\n\n"
                     " BinaryBuilder will find all files in the source directory, and encode them\n"
                     " into two files called (targetclassname).cpp and (targetclassname).h, which it\n"
                     " will write into the target directory supplied.\n\n"
                     " Any files in sub-directories of the source directory will be put into the\n"
                     " resultant class, but #ifdef'ed out using the name of the sub-directory (hard to\n"
                     " explain, but obvious when you try it...)\n";

        return 2; // USAGE
    }

    const File sourceDirectory (File::getCurrentWorkingDirectory()
                                     .getChildFile (String (argv[1]).unquoted()));

    if (! sourceDirectory.isDirectory())
    {
        std::cout << "Source directory doesn't exist: "
                  << sourceDirectory.getFullPathName()
                  << std::endl << std::endl;

        return 1; // General ERROR
    }

    const File destDirectory (File::getCurrentWorkingDirectory()
                                   .getChildFile (String (argv[2]).unquoted()));

    if (! destDirectory.isDirectory())
    {
        std::cout << "Destination directory doesn't exist: "
                  << destDirectory.getFullPathName() << std::endl << std::endl;

        return 1; // General ERROR
    }

    String className (argv[3]);
    className = className.trim();

    const File headerFile (destDirectory.getChildFile (className).withFileExtension (".h"));
    const File cppFile    (destDirectory.getChildFile (className).withFileExtension (".cpp"));

    std::cout << "Creating " << headerFile.getFullPathName()
              << " and " << cppFile.getFullPathName()
              << " from files in " << sourceDirectory.getFullPathName()
              << "..." << std::endl << std::endl;

    auto files = sourceDirectory.findChildFiles (File::findFiles, true,
                                                 (argc > 4) ? argv[4] : "*");

    if (files.isEmpty())
    {
        std::cout << "Didn't find any source files in: "
                  << sourceDirectory.getFullPathName() << std::endl << std::endl;
        return 1; // General ERROR
    }

    headerFile.deleteFile();
    cppFile.deleteFile();

    std::unique_ptr<OutputStream> header (headerFile.createOutputStream());

    if (header == nullptr)
    {
        std::cout << "Couldn't open "
                  << headerFile.getFullPathName() << " for writing" << std::endl << std::endl;
        return 126; // NO_PERM
    }

    std::unique_ptr<OutputStream> cpp (cppFile.createOutputStream());

    if (cpp == nullptr)
    {
        std::cout << "Couldn't open "
                  << cppFile.getFullPathName() << " for writing" << std::endl << std::endl;
        return 126; // NO_PERM
    }

    *header << "/* (Auto-generated binary data file). */\r\n\r\n"
               "#pragma once\r\n\r\n"
               "namespace " << className << "\r\n"
               "{\r\n";

    *cpp << "/* (Auto-generated binary data file). */\r\n\r\n"
            "#include \"" << className << ".h\"\r\n\r\n";

    int totalBytes = 0;

    for (int i = 0; i < files.size(); ++i)
    {
        const File file (files[i]);

        // (avoid source control files and hidden files..)
        if (! isHiddenFile (file, sourceDirectory))
        {
            if (file.getParentDirectory() != sourceDirectory)
            {
                *header << "  #ifdef " << file.getParentDirectory().getFileName().toUpperCase() << "\r\n";
                *cpp << "#ifdef " << file.getParentDirectory().getFileName().toUpperCase() << "\r\n";

                totalBytes += addFile (file, className, *header, *cpp);

                *header << "  #endif\r\n";
                *cpp << "#endif\r\n";
            }
            else
            {
                totalBytes += addFile (file, className, *header, *cpp);
            }
        }
    }

    *header << "}\r\n";

    header = nullptr;
    cpp = nullptr;

    std::cout << std::endl << " Total size of binary data: " << totalBytes << " bytes" << std::endl;

    return 0;
}
