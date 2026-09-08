#include <iostream>
#include <string>

#include "lib.hpp"

auto main(int argc, char* argv[]) -> int
{
  auto const lib = library {};
  std::cout << "Loading: " << lib.name << "!\n";

  if (argc != 2)
  {
    std::cout << "usage: " << argv[0] << " <map.json>\n";
    return 1;
  }

  auto game = battlefieldGame {};
  return game.play(std::string {argv[1]});
}