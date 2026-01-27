include(FeatureSummary)
set_package_properties(libADLMIDI PROPERTIES
  URL "https://github.com/Wohlstand/libADLMIDI"
  DESCRIPTION "libADLMIDI is a free Software MIDI synthesizer library with OPL3 emulation"
)


####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was libADLMIDIConfig.cmake.in                            ########

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

if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/libADLMIDI-shared-targets.cmake")
    include("${CMAKE_CURRENT_LIST_DIR}/libADLMIDI-shared-targets.cmake")
endif()
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/libADLMIDI-static-targets.cmake")
    include("${CMAKE_CURRENT_LIST_DIR}/libADLMIDI-static-targets.cmake")
endif()

if(TARGET libADLMIDI::ADLMIDI_shared)
    if(CMAKE_VERSION VERSION_LESS "3.18")
        add_library(libADLMIDI::ADLMIDI INTERFACE IMPORTED)
        set_target_properties(libADLMIDI::ADLMIDI PROPERTIES INTERFACE_LINK_LIBRARIES "libADLMIDI::ADLMIDI_shared")
    else()
        add_library(libADLMIDI::ADLMIDI ALIAS libADLMIDI::ADLMIDI_shared)
    endif()
else()
    if(CMAKE_VERSION VERSION_LESS "3.18")
        add_library(libADLMIDI::ADLMIDI INTERFACE IMPORTED)
        set_target_properties(libADLMIDI::ADLMIDI PROPERTIES INTERFACE_LINK_LIBRARIES "libADLMIDI::ADLMIDI_static")
    else()
        add_library(libADLMIDI::ADLMIDI ALIAS libADLMIDI::ADLMIDI_static)
    endif()
endif()
