# JUCE Documentation

This directory contains files documenting the JUCE Module Format, and the JUCE
CMake API.

The JUCE modules themselves can be found in the `modules` subdirectory of the
JUCE repository.

CMake example projects are located in the `examples/CMake` directory.

The JUCE API itself is documented inline, but HTML docs can be generated from
the source code using the `doxygen` tool. These HTML docs can be [found
online](https://juce.com/learn/documentation), or you can generate a local copy
which can be used without an internet connection. For instructions on generating
offline docs, see below.

# Generating Offline HTML Documentation

## Dependencies

- doxygen
- python (version 3.x)
- make
- graphviz (to generate inheritance diagrams)

Make sure that all the dependencies can be found on your PATH.

## Building

- cd into the `doxygen` directory on the command line
- run `make`

Doxygen will create a new subdirectory "doc". Open doc/index.html in your browser
to access the generated HTML documentation.
