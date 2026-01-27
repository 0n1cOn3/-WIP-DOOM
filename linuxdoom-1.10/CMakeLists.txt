cmake_minimum_required(VERSION 3.16)

project(linuxdoom-legacy LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

set(SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR})

file(GLOB DOOM_SOURCES
    ${SOURCE_DIR}/*.c)

list(APPEND DOOM_SOURCES ${SOURCE_DIR}/i_music_convert.cpp)

list(REMOVE_ITEM DOOM_SOURCES
    ${SOURCE_DIR}/i_video_sdl2.c
    ${SOURCE_DIR}/i_net.c
    ${SOURCE_DIR}/net_harness.c)

set(VIDEO_SOURCES)
set(VIDEO_LIBS)
set(NET_SOURCES)
set(NET_LIBS m)
set(NET_INCLUDE_DIRS)
set(NET_COMPILE_DEFS)

include(FetchContent)

set(libADLMIDI_STATIC ON CACHE BOOL "" FORCE)
set(libADLMIDI_SHARED OFF CACHE BOOL "" FORCE)
set(WITH_ADLMIDI2 OFF CACHE BOOL "" FORCE)

set(libOPNMIDI_STATIC ON CACHE BOOL "" FORCE)
set(libOPNMIDI_SHARED OFF CACHE BOOL "" FORCE)
set(WITH_MIDIPLAY OFF CACHE BOOL "" FORCE)
set(WITH_MIDI2VGM OFF CACHE BOOL "" FORCE)
set(WITH_VLC_PLUGIN OFF CACHE BOOL "" FORCE)
set(WITH_MIDI_SEQUENCER ON CACHE BOOL "" FORCE)

FetchContent_Declare(adlmidi
  GIT_REPOSITORY https://github.com/Wohlstand/libADLMIDI.git
  GIT_TAG v1.6.1
)
FetchContent_Declare(opnmidi
  GIT_REPOSITORY https://github.com/Wohlstand/libOPNMIDI.git
  GIT_TAG v1.6.1
)
FetchContent_MakeAvailable(adlmidi opnmidi)

find_package(SDL2 REQUIRED)
list(APPEND VIDEO_SOURCES ${SOURCE_DIR}/i_video_sdl2.c)
list(APPEND VIDEO_LIBS SDL2::SDL2)

# Network backend: SDL2_net
# If system SDL2_net is not available, use internal compatibility stub
find_package(PkgConfig QUIET)
find_package(SDL2_net QUIET)

if(NOT SDL2_net_FOUND AND PkgConfig_FOUND)
  pkg_check_modules(SDL2NET SDL2_net)
  if(SDL2NET_FOUND)
    add_library(SDL2_net::SDL2_net INTERFACE IMPORTED)
    target_include_directories(SDL2_net::SDL2_net INTERFACE ${SDL2NET_INCLUDE_DIRS})
    target_link_libraries(SDL2_net::SDL2_net INTERFACE ${SDL2NET_LIBRARIES})
  endif()
endif()

if(NOT TARGET SDL2_net::SDL2_net)
  set(STUB_INCLUDE_ROOT ${CMAKE_BINARY_DIR}/generated_stub_includes)
  file(MAKE_DIRECTORY ${STUB_INCLUDE_ROOT}/SDL2)
  configure_file(
    ${SOURCE_DIR}/sdl_net_stub/SDL_net.h
    ${STUB_INCLUDE_ROOT}/SDL2/SDL_net.h
    COPYONLY
  )
  configure_file(
    ${SOURCE_DIR}/sdl_net_stub/SDL_net.h
    ${STUB_INCLUDE_ROOT}/SDL_net.h
    COPYONLY
  )

  add_library(SDL2_net_stub STATIC
    ${SOURCE_DIR}/sdl_net_stub/SDL_net_stub.c)
  target_include_directories(SDL2_net_stub PUBLIC ${STUB_INCLUDE_ROOT})
  add_library(SDL2_net::SDL2_net ALIAS SDL2_net_stub)
  set(NET_INCLUDE_DIRS ${STUB_INCLUDE_ROOT})
endif()

list(APPEND NET_SOURCES ${SOURCE_DIR}/i_net.c)
list(APPEND NET_LIBS SDL2_net::SDL2_net)

add_executable(linuxdoom
  ${DOOM_SOURCES}
  ${VIDEO_SOURCES}
  ${NET_SOURCES}
)

target_include_directories(linuxdoom PRIVATE ${SOURCE_DIR})
target_include_directories(linuxdoom PRIVATE ${NET_INCLUDE_DIRS})
target_include_directories(linuxdoom PRIVATE ${adlmidi_SOURCE_DIR}/src ${adlmidi_SOURCE_DIR}/utils/mus2mid)
target_compile_definitions(linuxdoom PRIVATE NORMALUNIX LINUX ${NET_COMPILE_DEFS})

find_package(Threads REQUIRED)
find_package(ALSA QUIET)

if(TARGET ADLMIDI)
  target_compile_definitions(linuxdoom PRIVATE HAVE_ADLMIDI)
  target_link_libraries(linuxdoom PRIVATE ADLMIDI)
endif()

if(TARGET OPNMIDI_IF)
  target_compile_definitions(linuxdoom PRIVATE HAVE_OPNMIDI)
  target_link_libraries(linuxdoom PRIVATE OPNMIDI_IF)
endif()

if(ALSA_FOUND)
  target_compile_definitions(linuxdoom PRIVATE HAVE_ALSA_SEQ)
  target_include_directories(linuxdoom PRIVATE ${ALSA_INCLUDE_DIRS})
  target_link_libraries(linuxdoom PRIVATE ${ALSA_LIBRARIES})
endif()

target_link_libraries(linuxdoom PRIVATE ${VIDEO_LIBS} ${NET_LIBS} Threads::Threads)

add_executable(net_harness
  ${SOURCE_DIR}/net_harness.c
  ${SOURCE_DIR}/m_argv.c
  ${NET_SOURCES}
)

target_include_directories(net_harness PRIVATE ${SOURCE_DIR})
target_include_directories(net_harness PRIVATE ${NET_INCLUDE_DIRS})
target_compile_definitions(net_harness PRIVATE NORMALUNIX LINUX ${NET_COMPILE_DEFS})

target_link_libraries(net_harness PRIVATE ${NET_LIBS} SDL2::SDL2)

# Build artifacts live next to the legacy Makefile output.
set_target_properties(linuxdoom PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

set_target_properties(net_harness PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
