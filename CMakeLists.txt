cmake_minimum_required(VERSION 3.14)
 
include(cmake/prelude.cmake)
 
project(
    cpp_navigation_game
    VERSION 0.1.0
    DESCRIPTION "Terminal UI navigation game"
    HOMEPAGE_URL "https://example.com/"
    LANGUAGES CXX
)
 
include(cmake/project-is-top-level.cmake)
include(cmake/variables.cmake)
 
# ---- Declare library ----
 
add_library(
    cpp_navigation_game_lib OBJECT
    source/lib.cpp
)
 
target_include_directories(
    cpp_navigation_game_lib ${warning_guard}
    PUBLIC
    "\$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/source>"
)
 
target_compile_features(cpp_navigation_game_lib PUBLIC cxx_std_17)
 
find_package(fmt REQUIRED)
target_link_libraries(cpp_navigation_game_lib PRIVATE fmt::fmt)
 
find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(cpp_navigation_game_lib PRIVATE nlohmann_json::nlohmann_json)
 
# ---- Declare executable ----
 
add_executable(cpp_navigation_game_exe source/main.cpp)
add_executable(cpp_navigation_game::exe ALIAS cpp_navigation_game_exe)
 
set_property(TARGET cpp_navigation_game_exe PROPERTY OUTPUT_NAME cpp_navigation_game)
 
target_compile_features(cpp_navigation_game_exe PRIVATE cxx_std_17)
 
target_link_libraries(cpp_navigation_game_exe PRIVATE cpp_navigation_game_lib)
 
# ---- Install rules ----
 
if(NOT CMAKE_SKIP_INSTALL_RULES)
  include(cmake/install-rules.cmake)
endif()
 
# ---- Developer mode ----
 
if(NOT cpp_navigation_game_DEVELOPER_MODE)
  return()
elseif(NOT PROJECT_IS_TOP_LEVEL)
  message(
      AUTHOR_WARNING
      "Developer mode is intended for developers of cpp_navigation_game"
  )
endif()
 
# ---- Test mode ----

option(
    NAVGAME_TEST_INTERNALS
    "Expose battlefieldGame's private members to the test suite"
    ON
)
 
if(NAVGAME_TEST_INTERNALS)
  target_compile_definitions(cpp_navigation_game_lib PUBLIC NAVGAME_TEST_INTERNALS)
endif()
 
include(cmake/dev-mode.cmake)
 
