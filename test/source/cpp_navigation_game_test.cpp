#include <catch2/catch_test_macros.hpp>

#include "lib.hpp"

TEST_CASE("Name is cpp_navigation_game", "[library]")
{
  auto const lib = library {};
  REQUIRE(lib.name == "cpp_navigation_game");
}
