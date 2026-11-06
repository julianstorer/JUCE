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

#include "jucer_Headers.h"
#include "jucer_Application.h"
#include "../Utility/Helpers/jucer_TranslationHelpers.h"

#include "jucer_CommandLine.h"

//==============================================================================
const char* preferredLineFeed = "\r\n";
const char* getPreferredLineFeed()     { return preferredLineFeed; }

//==============================================================================
namespace
{
    static void hideDockIcon()
    {
       #if JUCE_MAC
        Process::setDockIconVisible (false);
       #endif
    }

    static Array<File> findAllSourceFiles (const File& folder)
    {
        Array<File> files;

        for (const auto& di : RangedDirectoryIterator (folder, true, "*.cpp;*.cxx;*.cc;*.c;*.h;*.hpp;*.hxx;*.hpp;*.mm;*.m;*.java;*.dox;*.soul;*.js", File::findFiles))
            if (! di.getFile().isSymbolicLink())
                files.add (di.getFile());

        return files;
    }

    static void replaceFile (const File& file, const String& newText, const String& message)
    {
        std::cout << message << file.getFullPathName() << std::endl;

        TemporaryFile temp (file);

        if (! temp.getFile().replaceWithText (newText, false, false, nullptr))
            ConsoleApplication::fail ("!!! ERROR Couldn't write to temp file!");

        if (! temp.overwriteTargetFileWithTemporary())
            ConsoleApplication::fail ("!!! ERROR Couldn't write to file!");
    }

    //==============================================================================
    struct LoadedProject
    {
        explicit LoadedProject (const ArgumentList::Argument& fileToLoad)
        {
            hideDockIcon();

            auto projectFile = fileToLoad.resolveAsExistingFile();

            if (! projectFile.hasFileExtension (Project::projectFileExtension))
                ConsoleApplication::fail (projectFile.getFullPathName() + " isn't a valid jucer project file!");

            project.reset (new Project (projectFile));

            if (! project->loadFrom (projectFile, true, false))
            {
                project.reset();
                ConsoleApplication::fail ("Failed to load the project file: " + projectFile.getFullPathName());
            }

            preferredLineFeed = project->getProjectLineFeed().toRawUTF8();
        }

        void save (bool justSaveResources, bool fixMissingDependencies)
        {
            if (project != nullptr)
            {
                if (! justSaveResources)
                    rescanModulePathsIfNecessary();

                if (fixMissingDependencies)
                    tryToFixMissingModuleDependencies();

                const auto onCompletion = [this] (Result result)
                {
                    project.reset();

                    if (result.failed())
                        ConsoleApplication::fail ("Error when saving: " + result.getErrorMessage());
                };

                if (justSaveResources)
                    onCompletion (project->saveResourcesOnly());
                else
                    project->saveProject (Async::no, nullptr, onCompletion);
            }
        }

        void rescanModulePathsIfNecessary()
        {
            bool scanJUCEPath = false, scanUserPaths = false;

            const auto& modules = project->getEnabledModules();

            for (auto i = modules.getNumModules(); --i >= 0;)
            {
                const auto& id = modules.getModuleID (i);

                if (isJUCEModule (id) && ! scanJUCEPath)
                {
                    if (modules.shouldUseGlobalPath (id))
                        scanJUCEPath = true;
                }
                else if (! scanUserPaths)
                {
                    if (modules.shouldUseGlobalPath (id))
                        scanUserPaths = true;
                }
            }

            if (scanJUCEPath)
                ProjucerApplication::getApp().rescanJUCEPathModules();

            if (scanUserPaths)
                ProjucerApplication::getApp().rescanUserPathModules();
        }

        void tryToFixMissingModuleDependencies()
        {
            auto& modules = project->getEnabledModules();

            for (const auto& m : modules.getModulesWithMissingDependencies())
                modules.tryToFixMissingDependencies (m);
        }

        void clearMainGroup()
        {
            auto mainGroup = project->getMainGroup();
            auto mainGrouID = mainGroup.getID();

            mainGroup.removeItemFromProject();
            Project::Item cleanMainGroup (*project, ValueTree ("MAINGROUP"), false);
            cleanMainGroup.setID(mainGrouID);
            project->getProjectRoot().addChild (cleanMainGroup.state, 0, nullptr);
        }

        void addFile (const File& file)
        {
            project->getMainGroup().addFileRetainingSortOrder (file, true);
        }

        std::unique_ptr<Project> project;
    };

    //==============================================================================
    /* Running a command-line of the form "projucer --resave foobar.jucer" will try to load
       that project and re-export all of its targets.
    */
    static void resaveProject (const ArgumentList& args, bool justSaveResources)
    {
        args.checkMinNumArguments (2);
        LoadedProject proj (args[1]);

        std::cout << (justSaveResources ? "Re-saving project resources: "
                                        : "Re-saving file: ")
                  << proj.project->getFile().getFullPathName() << std::endl;

        proj.save (justSaveResources, args.containsOption ("--fix-missing-dependencies"));
    }

    static void clearMainGroup (const ArgumentList& args)
    {
        args.checkMinNumArguments (2);
        LoadedProject proj (args[1]);

        std::cout << "Clearing MAINGROUP: "
                  << std::endl;

        proj.clearMainGroup ();

        std::cout << "Re-saving file: "
                  << proj.project->getFile().getFullPathName() << std::endl;

        proj.save (false, args.containsOption ("--fix-missing-dependencies"));
    }

    static void addFile (const ArgumentList& args)
    {
        args.checkMinNumArguments (3);
        LoadedProject proj (args[1]);
        auto fileToAdd = args[2].resolveAsExistingFile();

        std::cout << "Adding File: "
                  << fileToAdd.getFileName() << std::endl;

        proj.addFile (fileToAdd.getFullPathName());

        std::cout << "Re-saving file: "
                  << proj.project->getFile().getFullPathName() << std::endl;

        proj.save (false, args.containsOption ("--fix-missing-dependencies"));
    }

    //==============================================================================
    static void getVersion (const ArgumentList& args)
    {
        args.checkMinNumArguments (2);
        LoadedProject proj (args[1]);

        std::cout << proj.project->getVersionString() << std::endl;
    }

    //==============================================================================
    static void setVersion (const ArgumentList& args)
    {
        args.checkMinNumArguments (2);
        LoadedProject proj (args[2]);

        String version (args[1].text.trim());

        std::cout << "Setting project version: " << version << std::endl;

        proj.project->setProjectVersion (version);
        proj.save (false, false);
    }

    //==============================================================================
    static void bumpVersion (const ArgumentList& args)
    {
        args.checkMinNumArguments (2);
        LoadedProject proj (args[1]);

        String version = proj.project->getVersionString();

        version = version.upToLastOccurrenceOf (".", true, false)
                    + String (version.getTrailingIntValue() + 1);

        std::cout << "Bumping project version to: " << version << std::endl;

        proj.project->setProjectVersion (version);
        proj.save (false, false);
    }

    static void gitTag (const ArgumentList& args)
    {
        args.checkMinNumArguments (2);
        LoadedProject proj (args[1]);

        String version (proj.project->getVersionString());

        if (version.trim().isEmpty())
            ConsoleApplication::fail ("Cannot read version number from project!");

        StringArray command;
        command.add ("git");
        command.add ("tag");
        command.add ("-a");
        command.add (version);
        command.add ("-m");
        command.add (version.quoted());

        std::cout << "Performing command: " << command.joinIntoString (" ") << std::endl;

        ChildProcess c;

        if (! c.start (command, 0))
            ConsoleApplication::fail ("Cannot run git!");

        c.waitForProcessToFinish (10000);

        if (c.getExitCode() != 0)
            ConsoleApplication::fail ("git command failed!");
    }

    //==============================================================================
    static void showStatus (const ArgumentList& args)
    {
        args.checkMinNumArguments (2);

        LoadedProject proj (args[1]);

        std::cout << "Project file: " << proj.project->getFile().getFullPathName() << std::endl
                  << "Name: " << proj.project->getProjectNameString() << std::endl
                  << "UID: " << proj.project->getProjectUIDString() << std::endl;

        auto& modules = proj.project->getEnabledModules();

        if (int numModules = modules.getNumModules())
        {
            std::cout << "Modules:" << std::endl;

            for (int i = 0; i < numModules; ++i)
                std::cout << "  " << modules.getModuleID (i) << std::endl;
        }
    }

    //==============================================================================
    static String getModulePackageName (const LibraryModule& module)
    {
        return module.getID() + ".jucemodule";
    }

    static void zipModule (const File& targetFolder, const File& moduleFolder)
    {
        jassert (targetFolder.isDirectory());

        auto moduleFolderParent = moduleFolder.getParentDirectory();
        LibraryModule module (moduleFolder);

        if (! module.isValid())
            ConsoleApplication::fail (moduleFolder.getFullPathName() + " is not a valid module folder!");

        auto targetFile = targetFolder.getChildFile (getModulePackageName (module));

        ZipFile::Builder zip;

        {
            for (const auto& i : RangedDirectoryIterator (moduleFolder, true, "*", File::findFiles))
                if (! i.getFile().isHidden())
                    zip.addFile (i.getFile(), 9, i.getFile().getRelativePathFrom (moduleFolderParent));
        }

        std::cout << "Writing: " << targetFile.getFullPathName() << std::endl;

        TemporaryFile temp (targetFile);

        {
            FileOutputStream out (temp.getFile());

            if (! (out.openedOk() && zip.writeToStream (out, nullptr)))
                ConsoleApplication::fail ("Failed to write to the target file: " + targetFile.getFullPathName());
        }

        if (! temp.overwriteTargetFileWithTemporary())
            ConsoleApplication::fail ("Failed to write to the target file: " + targetFile.getFullPathName());
    }

    static void buildModules (const ArgumentList& args, const bool buildAllWithIndex)
    {
        hideDockIcon();
        args.checkMinNumArguments (3);

        auto targetFolder = args[1].resolveAsFile();

        if (! targetFolder.isDirectory())
            ConsoleApplication::fail ("The first argument must be the directory to put the result.");

        if (buildAllWithIndex)
        {
            auto folderToSearch = args[2].resolveAsFile();
            var infoList;

            for (const auto& i : RangedDirectoryIterator (folderToSearch, false, "*", File::findDirectories))
            {
                LibraryModule module (i.getFile());

                if (module.isValid())
                {
                    zipModule (targetFolder, i.getFile());

                    var moduleInfo (new DynamicObject());
                    moduleInfo.getDynamicObject()->setProperty ("file", getModulePackageName (module));
                    moduleInfo.getDynamicObject()->setProperty ("info", module.moduleDescription.getModuleInfo());
                    infoList.append (moduleInfo);
                }
            }

            auto indexFile = targetFolder.getChildFile ("modulelist");
            std::cout << "Writing: " << indexFile.getFullPathName() << std::endl;
            indexFile.replaceWithText (JSON::toString (infoList), false, false);
        }
        else
        {
            for (int i = 2; i < args.size(); ++i)
                zipModule (targetFolder, args[i].resolveAsFile());
        }
    }

    //==============================================================================
    struct CleanupOptions
    {
        bool removeTabs;
        bool fixDividerComments;
    };

    static void cleanWhitespace (const File& file, CleanupOptions options)
    {
        auto content = file.loadFileAsString();

        auto isProjucerTemplateFile = [file, content]
        {
            return file.getFullPathName().contains ("Templates")
                  && content.contains ("%""%") && content.contains ("//[");
        }();

        if (isProjucerTemplateFile)
            return;

        StringArray lines;
        lines.addLines (content);
        bool anyTabsRemoved = false;

        for (int i = 0; i < lines.size(); ++i)
        {
            String& line = lines.getReference (i);

            if (options.removeTabs && line.containsChar ('\t'))
            {
                anyTabsRemoved = true;

                for (;;)
                {
                    const int tabPos = line.indexOfChar ('\t');

                    if (tabPos < 0)
                        break;

                    const int spacesPerTab = 4;
                    const int spacesNeeded = spacesPerTab - (tabPos % spacesPerTab);
                    line = line.replaceSection (tabPos, 1, String::repeatedString (" ", spacesNeeded));
                }
            }

            if (options.fixDividerComments)
            {
                auto afterIndent = line.trim();

                if (afterIndent.startsWith ("//") && afterIndent.length() > 20)
                {
                    afterIndent = afterIndent.substring (2);

                    if (afterIndent.containsOnly ("=")
                          || afterIndent.containsOnly ("/")
                          || afterIndent.containsOnly ("-"))
                    {
                        line = line.substring (0, line.indexOfChar ('/'))
                                  + "//" + String::repeatedString ("=", 78);
                    }
                }
            }

            line = line.trimEnd();
        }

        if (options.removeTabs && ! anyTabsRemoved)
            return;

        auto newText = joinLinesIntoSourceFile (lines);

        if (newText != content && newText != content + getPreferredLineFeed())
            replaceFile (file, newText, options.removeTabs ? "Removing tabs in: "
                                                           : "Cleaning file: ");
    }

    static void scanFilesForCleanup (const ArgumentList& args, CleanupOptions options)
    {
        args.checkMinNumArguments (2);

        for (auto it = args.arguments.begin() + 1; it < args.arguments.end(); ++it)
        {
            auto target = it->resolveAsFile();

            Array<File> files;

            if (target.isDirectory())
                files = findAllSourceFiles (target);
            else
                files.add (target);

            for (int i = 0; i < files.size(); ++i)
                cleanWhitespace (files.getReference (i), options);
        }
    }

    static void cleanWhitespace (const ArgumentList& args, bool replaceTabs)
    {
        CleanupOptions options = { replaceTabs, false };
        scanFilesForCleanup (args, options);
    }

    static void tidyDividerComments (const ArgumentList& args)
    {
        CleanupOptions options = { false, true };
        scanFilesForCleanup (args, options);
    }

    //==============================================================================
    static File findSimilarlyNamedHeader (const Array<File>& allFiles, const String& name, const File& sourceFile)
    {
        File result;

        for (auto& f : allFiles)
        {
            if (f.getFileName().equalsIgnoreCase (name) && f != sourceFile)
            {
                if (result.exists())
                    return {}; // multiple possible results, so don't change it!

                result = f;
            }
        }

        return result;
    }

    static void fixIncludes (const File& file, const Array<File>& allFiles)
    {
        const String content (file.loadFileAsString());

        StringArray lines;
        lines.addLines (content);
        bool hasChanged = false;

        for (auto& line : lines)
        {
            if (line.trimStart().startsWith ("#include \""))
            {
                auto includedFile = line.fromFirstOccurrenceOf ("\"", true, false)
                                        .upToLastOccurrenceOf ("\"", true, false)
                                        .trim()
                                        .unquoted();

                auto target = file.getSiblingFile (includedFile);

                if (! target.exists())
                {
                    auto header = findSimilarlyNamedHeader (allFiles, target.getFileName(), file);

                    if (header.exists())
                    {
                        line = line.upToFirstOccurrenceOf ("#include \"", true, false)
                                  + header.getRelativePathFrom (file.getParentDirectory())
                                          .replaceCharacter ('\\', '/')
                                  + "\"";

                        hasChanged = true;
                    }
                }
            }
        }

        if (hasChanged)
        {
            auto newText = joinLinesIntoSourceFile (lines);

            if (newText != content && newText != content + getPreferredLineFeed())
                replaceFile (file, newText, "Fixing includes in: ");
        }
    }

    static void fixRelativeIncludePaths (const ArgumentList& args)
    {
        args.checkMinNumArguments (2);
        auto target = args[1].resolveAsExistingFolder();
        auto files = findAllSourceFiles (target);

        for (int i = 0; i < files.size(); ++i)
            fixIncludes (files.getReference (i), files);
    }

    //==============================================================================
    static String getStringConcatenationExpression (Random& rng, int start, int length)
    {
        jassert (length > 0);

        if (length == 1)
            return "s" + String (start);

        int breakPos = jlimit (1, length - 1, (length / 3) + rng.nextInt (jmax (1, length / 3)));

        return "(" + getStringConcatenationExpression (rng, start, breakPos)
                + " + " + getStringConcatenationExpression (rng, start + breakPos, length - breakPos) + ")";
    }

    static void generateObfuscatedStringCode (const ArgumentList& args)
    {
        args.checkMinNumArguments (2);
        auto originalText = args[1].text.unquoted();

        struct Section
        {
            String text;
            int position, index;

            void writeGenerator (MemoryOutputStream& out) const
            {
                String name ("s" + String (index));

                out << "    String " << name << ";  " << name;

                auto escapeIfSingleQuote = [] (const String& s) -> String
                {
                    if (s == "\'")
                        return "\\'";

                    return s;
                };

                for (int i = 0; i < text.length(); ++i)
                    out << " << '" << escapeIfSingleQuote (String::charToString (text[i])) << "'";

                out << ";" << preferredLineFeed;
            }
        };

        Array<Section> sections;
        String text = originalText;
        Random rng;

        while (text.isNotEmpty())
        {
            int pos = jmax (0, text.length() - (1 + rng.nextInt (6)));
            Section s = { text.substring (pos), pos, 0 };
            sections.insert (0, s);
            text = text.substring (0, pos);
        }

        for (int i = 0; i < sections.size(); ++i)
            sections.getReference (i).index = i;

        for (int i = 0; i < sections.size(); ++i)
            sections.swap (i, rng.nextInt (sections.size()));

        MemoryOutputStream out;

        out << "String createString()" << preferredLineFeed
            << "{" << preferredLineFeed;

        for (int i = 0; i < sections.size(); ++i)
            sections.getReference (i).writeGenerator (out);

        out << preferredLineFeed
            << "    String result = " << getStringConcatenationExpression (rng, 0, sections.size()) << ";" << preferredLineFeed
            << preferredLineFeed
            << "    jassert (result == " << originalText.quoted() << ");" << preferredLineFeed
            << "    return result;" << preferredLineFeed
            << "}" << preferredLineFeed;

        std::cout << out.toString() << std::endl;
    }

    static void scanFoldersForTranslationFiles (const ArgumentList& args)
    {
        args.checkMinNumArguments (2);

        StringArray translations;

        for (auto it = args.arguments.begin() + 1; it != args.arguments.end(); ++it)
        {
            auto directoryToSearch = it->resolveAsExistingFolder();
            TranslationHelpers::scanFolderForTranslations (translations, directoryToSearch);
        }

        std::cout << TranslationHelpers::mungeStrings (translations) << std::endl;
    }

    static void createFinishedTranslationFile (const ArgumentList& args)
    {
        args.checkMinNumArguments (3);

        auto preTranslated  = args[1].resolveAsExistingFile().loadFileAsString();
        auto postTranslated = args[2].resolveAsExistingFile().loadFileAsString();

        auto localisedContent = (args.size() > 3 ? args[3].resolveAsExistingFile().loadFileAsString() : String());
        auto localised        = LocalisedStrings (localisedContent, false);

        using TH = TranslationHelpers;
        std::cout << TH::createFinishedTranslationFile (TH::withTrimmedEnds (TH::breakApart (preTranslated)),
                                                        TH::withTrimmedEnds (TH::breakApart (postTranslated)),
                                                        localised) << std::endl;
    }

    //==============================================================================
    static void encodeBinary (const ArgumentList& args)
    {
        args.checkMinNumArguments (3);
        auto source = args[1].resolveAsExistingFile();
        auto target = args[2].resolveAsFile();

        MemoryOutputStream literal;
        size_t dataSize = 0;

        {
            MemoryBlock data;
            FileInputStream input (source);
            input.readIntoMemoryBlock (data);
            build_tools::writeDataAsCppLiteral (data, literal, true, true);
            dataSize = data.getSize();
        }

        auto variableName = build_tools::makeBinaryDataIdentifierName (source);

        MemoryOutputStream header, cpp;

        header << "// Auto-generated binary data by the Projucer" << preferredLineFeed
               << "// Source file: " << source.getRelativePathFrom (target.getParentDirectory()) << preferredLineFeed
               << preferredLineFeed;

        cpp << header.toString();

        if (target.hasFileExtension (headerFileExtensions))
        {
            header << "static constexpr unsigned char " << variableName << "[] =" << preferredLineFeed
                   << literal.toString() << preferredLineFeed
                   << preferredLineFeed;

            replaceFile (target, header.toString(), "Writing: ");
        }
        else if (target.hasFileExtension (cppFileExtensions))
        {
            header << "extern const char*  " << variableName << ";" << preferredLineFeed
                   << "const unsigned int  " << variableName << "Size = " << (int) dataSize << ";" << preferredLineFeed
                   << preferredLineFeed;

            cpp << CodeHelpers::createIncludeStatement (target.withFileExtension (".h").getFileName()) << preferredLineFeed
                << preferredLineFeed
                << "static constexpr unsigned char " << variableName << "_local[] =" << preferredLineFeed
                << literal.toString() << preferredLineFeed
                << preferredLineFeed
                << "const char* " << variableName << " = (const char*) " << variableName << "_local;" << preferredLineFeed;

            replaceFile (target, cpp.toString(), "Writing: ");
            replaceFile (target.withFileExtension (".h"), header.toString(), "Writing: ");
        }
        else
        {
            ConsoleApplication::fail ("You need to specify a .h or .cpp file as the target");
        }
    }

    //==============================================================================
    static bool isThisOS (const String& os)
    {
        auto targetOS = TargetOS::unknown;

        if      (os == "osx")        targetOS = TargetOS::osx;
        else if (os == "windows")    targetOS = TargetOS::windows;
        else if (os == "linux")      targetOS = TargetOS::linux;

        if (targetOS == TargetOS::unknown)
            ConsoleApplication::fail ("You need to specify a valid OS! Use osx, windows or linux");

        return targetOS == TargetOS::getThisOS();
    }

    static bool isValidPathIdentifier (const String& id, const String& os)
    {
        return id == "vstLegacyPath" || (id == "aaxPath" && os != "linux") || id == "araPath"
            || id == "androidSDKPath" || id == "defaultJuceModulePath" || id == "defaultUserModulePath";
    }

    static void setGlobalPath (const ArgumentList& args)
    {
        args.checkMinNumArguments (3);

        if (! isValidPathIdentifier (args[2].text, args[1].text))
            ConsoleApplication::fail ("Identifier " + args[2].text + " is not valid for the OS " + args[1].text);

        auto userAppData = File::getSpecialLocation (File::userApplicationDataDirectory);

       #if JUCE_MAC
        userAppData = userAppData.getChildFile ("Application Support");
       #endif

        auto settingsFile = userAppData.getChildFile ("Projucer").getChildFile ("Projucer.settings");
        auto xml = parseXML (settingsFile);

        if (xml == nullptr)
            ConsoleApplication::fail ("Settings file not valid!");

        auto settingsTree = ValueTree::fromXml (*xml);

        if (! settingsTree.isValid())
            ConsoleApplication::fail ("Settings file not valid!");

        ValueTree childToSet;

        if (isThisOS (args[1].text))
        {
            childToSet = settingsTree.getChildWithProperty (Ids::name, "PROJECT_DEFAULT_SETTINGS")
                                     .getOrCreateChildWithName ("PROJECT_DEFAULT_SETTINGS", nullptr);
        }
        else
        {
            childToSet = settingsTree.getChildWithProperty (Ids::name, "FALLBACK_PATHS")
                                     .getOrCreateChildWithName ("FALLBACK_PATHS", nullptr)
                                     .getOrCreateChildWithName (args[1].text + "Fallback", nullptr);
        }

        if (! childToSet.isValid())
            ConsoleApplication::fail ("Failed to set the requested setting!");

        childToSet.setProperty (args[2].text, args[3].resolveAsFile().getFullPathName(), nullptr);

        settingsFile.replaceWithText (settingsTree.toXmlString());
    }

    static void createProjectFromPIP (const ArgumentList& args)
    {
        args.checkMinNumArguments (3);

        auto pipFile = args[1].resolveAsFile();

        if (! pipFile.existsAsFile())
            ConsoleApplication::fail ("PIP file doesn't exist.");

        auto outputDir = args[2].resolveAsFile();

        if (! outputDir.exists())
        {
            auto res = outputDir.createDirectory();
            std::cout << "Creating directory " << outputDir.getFullPathName() << std::endl;
        }

        File juceModulesPath, userModulesPath;

        if (args.size() > 3)
        {
            juceModulesPath = args[3].resolveAsFile();

            if (! juceModulesPath.exists())
                ConsoleApplication::fail ("Specified JUCE modules directory doesn't exist.");

            if (args.size() == 5)
            {
                userModulesPath = args[4].resolveAsFile();

                if (! userModulesPath.exists())
                    ConsoleApplication::fail ("Specified JUCE modules directory doesn't exist.");
            }
        }

        PIPGenerator generator (pipFile, outputDir, juceModulesPath, userModulesPath);

        auto createJucerFileResult = generator.createJucerFile();

        if (! createJucerFileResult)
            ConsoleApplication::fail (createJucerFileResult.getErrorMessage());

        auto createMainCppResult = generator.createMainCpp();

        if (! createMainCppResult)
            ConsoleApplication::fail (createMainCppResult.getErrorMessage());
    }

    //==============================================================================
    static void showHelp()
    {
        hideDockIcon();

        auto appName = JUCEApplication::getInstance()->getApplicationName();

        std::cout << appName << std::endl
                  << std::endl
                  << "Usage: " << std::endl
                  << std::endl
                  << " " << appName << " --resave project_file" << std::endl
                  << "    Resaves all files and resources in a project. Add the \"--fix-missing-dependencies\" option to automatically fix any missing module dependencies." << std::endl
                  << std::endl
                  << " " << appName << " --resave-resources project_file" << std::endl
                  << "    Resaves just the binary resources for a project." << std::endl
                  << std::endl
                  << " " << appName << " --clear-maingroup project_file" << std::endl
                  << "    Removes all resource file references from a project." << std::endl
                  << std::endl
                  << " " << appName << " --add-file project_file path_to_file_to_add" << std::endl
                  << "    Adds an existing file or directory to a project." << std::endl
                  << std::endl
                  << " " << appName << " --get-version project_file" << std::endl
                  << "    Returns the version number of a project." << std::endl
                  << std::endl
                  << " " << appName << " --set-version version_number project_file" << std::endl
                  << "    Updates the version number in a project." << std::endl
                  << std::endl
                  << " " << appName << " --bump-version project_file" << std::endl
                  << "    Updates the minor version number in a project by 1." << std::endl
                  << std::endl
                  << " " << appName << " --git-tag-version project_file" << std::endl
                  << "    Invokes 'git tag' to attach the project's version number to the current git repository." << std::endl
                  << std::endl
                  << " " << appName << " --status project_file" << std::endl
                  << "    Displays information about a project." << std::endl
                  << std::endl
                  << " " << appName << " --buildmodule target_folder module_folder" << std::endl
                  << "    Zips a module into a downloadable file format." << std::endl
                  << std::endl
                  << " " << appName << " --buildallmodules target_folder module_folder" << std::endl
                  << "    Zips all modules in a given folder and creates an index for them." << std::endl
                  << std::endl
                  << " " << appName << " --trim-whitespace target_folder" << std::endl
                  << "    Scans the given folder for C/C++ source files (recursively), and trims any trailing whitespace from their lines, as well as normalising their line-endings to CR-LF." << std::endl
                  << std::endl
                  << " " << appName << " --remove-tabs target_folder" << std::endl
                  << "    Scans the given folder for C/C++ source files (recursively), and replaces any tab characters with 4 spaces." << std::endl
                  << std::endl
                  << " " << appName << " --tidy-divider-comments target_folder" << std::endl
                  << "    Scans the given folder for C/C++ source files (recursively), and normalises any juce-style comment division lines (i.e. any lines that look like //===== or //------- or /////////// will be replaced)." << std::endl
                  << std::endl
                  << " " << appName << " --fix-broken-include-paths target_folder" << std::endl
                  << "    Scans the given folder for C/C++ source files (recursively). Where a file contains an #include of one of the other filenames, it changes it to use the optimum relative path. Helpful for auto-fixing includes when re-arranging files and folders in a project." << std::endl
                  << std::endl
                  << " " << appName << " --obfuscated-string-code string_to_obfuscate" << std::endl
                  << "    Generates a C++ function which returns the given string, but in an obfuscated way." << std::endl
                  << std::endl
                  << " " << appName << " --encode-binary source_binary_file target_cpp_file" << std::endl
                  << "    Converts a binary file to a C++ file containing its contents as a block of data. Provide a .h file as the target if you want a single output file, or a .cpp file if you want a pair of .h/.cpp files." << std::endl
                  << std::endl
                  << " " << appName << " --trans target_folders..." << std::endl
                  << "    Scans each of the given folders (recursively) for any NEEDS_TRANS macros, and generates a translation file that can be used with Projucer's translation file builder" << std::endl
                  << std::endl
                  << " " << appName << " --trans-finish pre_translated_file post_translated_file optional_existing_translation_file" << std::endl
                  << "    Creates a completed translations mapping file, that can be used to initialise a LocalisedStrings object. This allows you to localise the strings in your project" << std::endl
                  << std::endl
                  << " " << appName << " --set-global-search-path os identifier_to_set new_path" << std::endl
                  << "    Sets the global path for a specified os and identifier. The os should be either osx, windows or linux and the identifiers can be any of the following: "
                  << "defaultJuceModulePath, defaultUserModulePath, vstLegacyPath, aaxPath (not valid on linux), or androidSDKPath. " << std::endl
                  << std::endl
                  << " " << appName << " --create-project-from-pip path/to/PIP path/to/output path/to/JUCE/modules (optional) path/to/user/modules (optional)" << std::endl
                  << "    Generates a folder containing a JUCE project in the specified output path using the specified PIP file. Use the optional JUCE and user module paths to override "
                     "the global module paths." << std::endl
                  << std::endl
                  << "Note that for any of the file-rewriting commands, add the option \"--lf\" if you want it to use LF linefeeds instead of CRLF" << std::endl
                  << std::endl;
    }
}

//==============================================================================
int performCommandLine (const ArgumentList& args)
{
    return ConsoleApplication::invokeCatchingFailures ([&]() -> int
    {
        if (args.containsOption ("--lf"))
            preferredLineFeed = "\n";

        auto command = args[0];

        auto matchCommand = [&] (StringRef name) -> bool
        {
            return command == name || command.isLongOption (name);
        };

        if (matchCommand ("help"))                     { showHelp();                            return 0; }
        if (matchCommand ("h"))                        { showHelp();                            return 0; }
        if (matchCommand ("resave"))                   { resaveProject (args, false);           return 0; }
        if (matchCommand ("clear-maingroup"))          { clearMainGroup (args);                 return 0; }
        if (matchCommand ("add-file"))                 { addFile (args);                        return 0; }
        if (matchCommand ("resave-resources"))         { resaveProject (args, true);            return 0; }
        if (matchCommand ("get-version"))              { getVersion (args);                     return 0; }
        if (matchCommand ("set-version"))              { setVersion (args);                     return 0; }
        if (matchCommand ("bump-version"))             { bumpVersion (args);                    return 0; }
        if (matchCommand ("git-tag-version"))          { gitTag (args);                         return 0; }
        if (matchCommand ("buildmodule"))              { buildModules (args, false);            return 0; }
        if (matchCommand ("buildallmodules"))          { buildModules (args, true);             return 0; }
        if (matchCommand ("status"))                   { showStatus (args);                     return 0; }
        if (matchCommand ("trim-whitespace"))          { cleanWhitespace (args, false);         return 0; }
        if (matchCommand ("remove-tabs"))              { cleanWhitespace (args, true);          return 0; }
        if (matchCommand ("tidy-divider-comments"))    { tidyDividerComments (args);            return 0; }
        if (matchCommand ("fix-broken-include-paths")) { fixRelativeIncludePaths (args);        return 0; }
        if (matchCommand ("obfuscated-string-code"))   { generateObfuscatedStringCode (args);   return 0; }
        if (matchCommand ("encode-binary"))            { encodeBinary (args);                   return 0; }
        if (matchCommand ("trans"))                    { scanFoldersForTranslationFiles (args); return 0; }
        if (matchCommand ("trans-finish"))             { createFinishedTranslationFile (args);  return 0; }
        if (matchCommand ("set-global-search-path"))   { setGlobalPath (args);                  return 0; }
        if (matchCommand ("create-project-from-pip"))  { createProjectFromPIP (args);           return 0; }

        if (command.isLongOption() || command.isShortOption())
            ConsoleApplication::fail ("Unrecognised command: " + command.text.quoted());

        return commandLineNotPerformed;
    });
}
