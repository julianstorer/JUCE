SHELL := /bin/bash

SOURCE_FILES := $(shell find ../../modules -type f -name "juce_*.h" -or -name "juce_*.dox" | sed 's/ /\\ /g')

.PHONEY: clean

doc/index.html: build/juce_modules.dox Doxyfile
	doxygen

build/juce_modules.dox: process_source_files.py $(SOURCE_FILES)
	python3 $< ../../modules build

clean:
	rm -rf build doc

