cmake_minimum_required(VERSION 3.22)

# Pull the lora component from the parent project's components/ directory
set(EXTRA_COMPONENT_DIRS ../../components ../../managed_components)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Suppress overloaded-virtual warnings from RadioLib (derived classes hide base virtuals)
idf_build_set_property(CXX_COMPILE_OPTIONS "-Wno-overloaded-virtual" APPEND)

project(lora_unit_tests)
