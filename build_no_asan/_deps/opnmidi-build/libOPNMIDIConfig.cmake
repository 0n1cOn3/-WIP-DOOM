include(FeatureSummary)
set_package_properties(libOPNMIDI PROPERTIES
    URL "https://github.com/Wohlstand/libOPNMIDI"
    DESCRIPTION "A Software MIDI Synthesizer library with OPN2 (YM2612) emulator"
)


####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was libOPNMIDIConfig.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)

macro(set_and_check _var _file)
  set(${_var} "${_file}")
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
  endif()
endmacro()

macro(check_required_components _NAME)
  foreach(comp ${${_NAME}_FIND_COMPONENTS})
    if(NOT ${_NAME}_${comp}_FOUND)
      if(${_NAME}_FIND_REQUIRED_${comp})
        set(${_NAME}_FOUND FALSE)
      endif()
    endif()
  endforeach()
endmacro()

####################################################################################

if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/libOPNMIDI-shared-targets.cmake")
    include("${CMAKE_CURRENT_LIST_DIR}/libOPNMIDI-shared-targets.cmake")
endif()
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/libOPNMIDI-static-targets.cmake")
    include("${CMAKE_CURRENT_LIST_DIR}/libOPNMIDI-static-targets.cmake")
endif()

if(TARGET libOPNMIDI::OPNMIDI_shared)
    if(CMAKE_VERSION VERSION_LESS "3.18")
        add_library(libOPNMIDI::OPNMIDI_IF INTERFACE IMPORTED)
        set_target_properties(libOPNMIDI::OPNMIDI_IF PROPERTIES INTERFACE_LINK_LIBRARIES "libOPNMIDI::OPNMIDI_shared")
    else()
        add_library(libOPNMIDI::OPNMIDI_IF ALIAS libOPNMIDI::OPNMIDI_shared)
    endif()
else()
    if(CMAKE_VERSION VERSION_LESS "3.18")
        add_library(libOPNMIDI::OPNMIDI_IF INTERFACE IMPORTED)
        set_target_properties(libOPNMIDI::OPNMIDI_IF PROPERTIES INTERFACE_LINK_LIBRARIES "libOPNMIDI::OPNMIDI_static")
        add_library(libOPNMIDI::OPNMIDI_IF_STATIC INTERFACE IMPORTED)
        set_target_properties(libOPNMIDI::OPNMIDI_IF_STATIC PROPERTIES INTERFACE_LINK_LIBRARIES "libOPNMIDI::OPNMIDI_static")
    else()
        add_library(libOPNMIDI::OPNMIDI_IF ALIAS libOPNMIDI::OPNMIDI_static)
        add_library(libOPNMIDI::OPNMIDI_IF_STATIC ALIAS libOPNMIDI::OPNMIDI_static)
    endif()
endif()
