#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "libOPNMIDI::OPNMIDI_static" for configuration ""
set_property(TARGET libOPNMIDI::OPNMIDI_static APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(libOPNMIDI::OPNMIDI_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_NOCONFIG "C;CXX"
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib64/libOPNMIDI.a"
  )

list(APPEND _cmake_import_check_targets libOPNMIDI::OPNMIDI_static )
list(APPEND _cmake_import_check_files_for_libOPNMIDI::OPNMIDI_static "${_IMPORT_PREFIX}/lib64/libOPNMIDI.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
