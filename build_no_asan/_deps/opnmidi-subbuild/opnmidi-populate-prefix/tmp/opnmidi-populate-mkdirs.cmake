# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/hx/Github/DOOM/build_no_asan/_deps/opnmidi-src")
  file(MAKE_DIRECTORY "/home/hx/Github/DOOM/build_no_asan/_deps/opnmidi-src")
endif()
file(MAKE_DIRECTORY
  "/home/hx/Github/DOOM/build_no_asan/_deps/opnmidi-build"
  "/home/hx/Github/DOOM/build_no_asan/_deps/opnmidi-subbuild/opnmidi-populate-prefix"
  "/home/hx/Github/DOOM/build_no_asan/_deps/opnmidi-subbuild/opnmidi-populate-prefix/tmp"
  "/home/hx/Github/DOOM/build_no_asan/_deps/opnmidi-subbuild/opnmidi-populate-prefix/src/opnmidi-populate-stamp"
  "/home/hx/Github/DOOM/build_no_asan/_deps/opnmidi-subbuild/opnmidi-populate-prefix/src"
  "/home/hx/Github/DOOM/build_no_asan/_deps/opnmidi-subbuild/opnmidi-populate-prefix/src/opnmidi-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/hx/Github/DOOM/build_no_asan/_deps/opnmidi-subbuild/opnmidi-populate-prefix/src/opnmidi-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/hx/Github/DOOM/build_no_asan/_deps/opnmidi-subbuild/opnmidi-populate-prefix/src/opnmidi-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
