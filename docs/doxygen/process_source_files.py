#!/usr/bin/env python3

import os
import shutil
import re
import argparse


def get_curly_brace_scope_end(string, start_pos):
    """Given a string and a starting position of an opening brace, find the
       position of the closing brace.
    """
    start_pos += 1
    string_end = len(string)
    bracket_counter = 1
    while start_pos < string_end:
        if string[start_pos] == "{":
            bracket_counter += 1
        elif string[start_pos] == "}":
            bracket_counter -= 1
            if bracket_counter == 0:
                return start_pos
        start_pos += 1
    return -1


def remove_juce_namespaces(source):
    """Return a string of source code with any juce namespaces removed.
    """
    namespace_regex = re.compile(r"\s+namespace\s+juce\s*{")

    match = namespace_regex.search(source)
    while (match is not None):
        source = source[:match.start()] + source[match.end():]
        end = get_curly_brace_scope_end(source, match.start() - 1)
        if end != -1:
            source = source[:end] + source[end + 1:]
            match = namespace_regex.search(source)
            continue
        else:
            raise ValueError("failed to find the end of the "
                             + match.group(1) + " namespace")
    return source


def add_doxygen_group(path, group_name):
    """Add a Doxygen group to the file at 'path'.

       The addition of juce namespacing code to all of the source files breaks
       backwards compatibility by changing the doc URLs, so we need to remove
       the namespaces.
    """

    filename = os.path.basename(path)
    if re.match(r"^juce_.*\.(h|dox)", filename):
        with open(path, "r") as f:
            content = f.read()
        with open(path, "w") as f:
            f.write("\r\n/** @weakgroup " + group_name + "\r\n *  @{\r\n */\r\n")
            f.write(remove_juce_namespaces(content))
            f.write("\r\n/** @}*/\r\n")


###############################################################################

if __name__ == "__main__":

    parser = argparse.ArgumentParser()
    parser.add_argument("source_dir",
                        help="the directory to search for source files")
    parser.add_argument("dest_dir",
                        help="the directory in which to place processed files")
    parser.add_argument("--subdirs",
                        help="if specified, only include these comma separated"
                             "subdirectories")
    args = parser.parse_args()

    try:
        shutil.rmtree(args.dest_dir)
    except OSError:
        pass
    except FileNotFoundError:
        pass

    # Get the list of JUCE modules to include.
    if args.subdirs:
        juce_modules = args.subdirs.split(",")
    else:
        juce_modules = []
        for item in os.listdir(args.source_dir):
            if os.path.isdir(os.path.join(args.source_dir, item)):
                juce_modules.append(item)

    # Copy the JUCE modules to the temporary directory, and process the source
    # files.
    module_definitions = []
    for module_name in juce_modules:

        # Copy the required modules.
        original_module_dir = os.path.join(args.source_dir, module_name)
        module_path = os.path.join(args.dest_dir, module_name)
        shutil.copytree(original_module_dir, module_path)

        # Parse the module header to get module information.
        module_header = os.path.join(module_path, module_name + ".h")
        with open(module_header, "r") as f:
            content = f.read()
        block_info_result = re.match(r".*BEGIN_JUCE_MODULE_DECLARATION"
                                     "(.*)"
                                     "END_JUCE_MODULE_DECLARATION.*",
                                     content,
                                     re.DOTALL)
        detail_lines = []
        for line in block_info_result.group(1).split("\n"):
            stripped_line = line.strip()
            if stripped_line:
                result = re.match(r"^.*?description:\s*(.*)$", stripped_line)
                if result:
                    short_description = result.group(1)
                else:
                    detail_lines.append(stripped_line)

        # The module header causes problems for Doxygen, so delete it.
        os.remove(module_header)

        # Create a Doxygen group definition for the module.
        module_definiton = []
        module_definiton.append("/** @defgroup {n} {n}".format(n=module_name))
        module_definiton.append("    {d}".format(d=short_description))
        module_definiton.append("")
        for line in detail_lines:
            module_definiton.append("    - {l}".format(l=line))
        module_definiton.append("")
        module_definiton.append("    @{")
        module_definiton.append("*/")

        # Create a list of the directories in the module that we can use as
        # subgroups and create the Doxygen group hierarchy string.
        dir_contents = os.listdir(module_path)
        # Ignore "native" folders as these are excluded by doxygen.
        try:
            dir_contents.remove("native")
        except ValueError:
            pass
        subdirs = []
        for item in dir_contents:
            if (os.path.isdir(os.path.join(module_path, item))):
                subdirs.append(item)
        module_groups = {}
        for subdir in subdirs:
            subgroup_name = "{n}-{s}".format(n=module_name, s=subdir)
            module_groups[subgroup_name] = os.path.join(module_path, subdir)
            module_definiton.append("")
            module_definiton.append(
                "/** @defgroup {tag} {n} */".format(tag=subgroup_name, n=subdir)
            )
        module_definiton.append("")
        module_definiton.append("/** @} */")

        module_definitions.append("\r\n".join(module_definiton))

        # Put the top level files into the main group.
        for filename in (set(dir_contents) - set(subdirs)):
            add_doxygen_group(os.path.join(module_path, filename), module_name)

        # Put subdirectory files into their respective groups.
        for group_name in module_groups:
            for dirpath, dirnames, filenames in os.walk(module_groups[group_name]):
                for filename in filenames:
                    filepath = os.path.join(dirpath, filename)
                    add_doxygen_group(filepath, group_name)

    # Create an extra header file containing the module hierarchy.
    with open(os.path.join(args.dest_dir, "juce_modules.dox"), "w") as f:
        f.write("\r\n\r\n".join(module_definitions))
