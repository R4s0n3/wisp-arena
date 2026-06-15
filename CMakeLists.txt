cmake_minimum_required(VERSION 3.21)

project(wisp-arena VERSION 0.1.0 LANGUAGES CXX)

add_executable(wisp-arena client/main.cpp)

target_compile_features(wisp-arena PRIVATE cxx_std_17)

find_package(raylib CONFIG REQUIRED)
find_package(ixwebsocket CONFIG REQUIRED)

if(TARGET raylib)
  set(WISP_RAYLIB_TARGET raylib)
elseif(TARGET raylib::raylib)
  set(WISP_RAYLIB_TARGET raylib::raylib)
else()
  message(FATAL_ERROR "raylib was found, but no known CMake target was exported")
endif()

if(TARGET ixwebsocket::ixwebsocket)
  set(WISP_IXWEBSOCKET_TARGET ixwebsocket::ixwebsocket)
elseif(TARGET ixwebsocket)
  set(WISP_IXWEBSOCKET_TARGET ixwebsocket)
elseif(TARGET IXWebSocket::ixwebsocket)
  set(WISP_IXWEBSOCKET_TARGET IXWebSocket::ixwebsocket)
else()
  message(FATAL_ERROR "ixwebsocket was found, but no known CMake target was exported")
endif()

target_link_libraries(wisp-arena PRIVATE ${WISP_RAYLIB_TARGET} ${WISP_IXWEBSOCKET_TARGET})

if(WIN32)
  target_compile_definitions(wisp-arena PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX NOGDI NOUSER)
  target_link_libraries(wisp-arena PRIVATE bcrypt)

  add_custom_command(
    TARGET wisp-arena
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:wisp-arena>
            $<TARGET_FILE_DIR:wisp-arena>
    COMMAND_EXPAND_LISTS
  )
endif()

install(TARGETS wisp-arena RUNTIME DESTINATION .)
