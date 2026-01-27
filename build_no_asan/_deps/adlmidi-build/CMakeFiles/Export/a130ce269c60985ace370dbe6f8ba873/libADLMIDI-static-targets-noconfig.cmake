#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "libADLMIDI::ADLMIDI_static" for configuration ""
set_property(TARGET libADLMIDI::ADLMIDI_static APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(libADLMIDI::ADLMIDI_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_NOCONFIG "C;CXX"
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib64/libADLMIDI.a"
  )

list(APPEND _cmake_import_check_targets libADLMIDI::ADLMIDI_static )
list(APPEND _cmake_import_check_files_for_libADLMIDI::ADLMIDI_static "${_IMPORT_PREFIX}/lib64/libADLMIDI.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
