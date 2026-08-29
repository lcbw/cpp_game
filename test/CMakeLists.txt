# Parent project does not export its library target, so this CML implicitly
# depends on being added from it, i.e. the testing is done only from the build
# tree and is not feasible from an install location

project(cpp_navigation_gameTests LANGUAGES CXX)

# ---- Dependencies ----

find_package(Catch2 REQUIRED)
include(Catch)

# ---- Tests ----

add_executable(cpp_navigation_game_test source/cpp_navigation_game_test.cpp)
target_link_libraries(
    cpp_navigation_game_test PRIVATE
    cpp_navigation_game_lib
    Catch2::Catch2WithMain
)
target_compile_features(cpp_navigation_game_test PRIVATE cxx_std_17)

catch_discover_tests(cpp_navigation_game_test)

# ---- End-of-file commands ----

add_folders(Test)
