// =============================================================================
// Tests for cpp_navigation_game
//
// The only public surface on battlefieldGame is play() and manhattanDistance().
// parseMap() and aStarSearch() are private, so almost everything below drives
// the code through play(): write a temp JSON map, capture stdout, assert on the
// exit code and the reported path.
//
// Fixture maps are written as ASCII for readability and converted to Tiled-style
// layer data by makeMapJson():
//
//   'S' -> CELL_START    (0)
//   'T' -> CELL_TARGET   (8)
//   '#' -> CELL_ELEVATED (3)
//   '.' -> CELL_OPEN     (-1)
//   '?' -> 99, an unrecognised value the parser ignores
//
// Define NAVGAME_TEST_INTERNALS (and give the test access to parseMap) to also
// build the direct parser tests at the bottom of the file.
// =============================================================================
 
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
 
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
 
#include "lib.hpp"
 
using Catch::Matchers::ContainsSubstring;
 
// -----------------------------------------------------------------------------
// Test helpers
// -----------------------------------------------------------------------------
 
namespace {
 
using Cell = battlefieldGame::gameCell;
 
int charToCell(char c)
{
  switch (c) {
    case 'S': return battlefieldGame::CELL_START;
    case 'T': return battlefieldGame::CELL_TARGET;
    case '#': return battlefieldGame::CELL_ELEVATED;
    case '.': return battlefieldGame::CELL_OPEN;
    default:  return 99;  // deliberately unrecognised
  }
}
 
/// Build Tiled-style JSON from ASCII rows. Row 0 is y == 0, x increases left to
/// right, matching the row-major ordering parseMap() assumes.
std::string makeMapJson(std::vector<std::string> const& rows)
{
  int const width  = rows.empty() ? 0 : static_cast<int>(rows.front().size());
  int const height = static_cast<int>(rows.size());
 
  std::ostringstream out;
  out << R"({"width":)" << width << R"(,"height":)" << height
      << R"(,"layers":[{"data":[)";
 
  bool first = true;
  for (auto const& row : rows) {
    for (char const c : row) {
      if (!first) { out << ","; }
      out << charToCell(c);
      first = false;
    }
  }
 
  out << "]}]}";
  return out.str();
}
 
/// Writes text to a uniquely named file under the system temp directory and
/// removes it on destruction.
class TempJson
{
public:
  explicit TempJson(std::string const& contents)
      : path_ {nextPath()}
  {
    std::ofstream out {path_};
    out << contents;
  }
 
  TempJson(TempJson const&)            = delete;
  TempJson& operator=(TempJson const&) = delete;
 
  ~TempJson()
  {
    std::error_code ec;
    std::filesystem::remove(path_, ec);
  }
 
  [[nodiscard]] std::string path() const { return path_.string(); }
 
private:
  static std::filesystem::path nextPath()
  {
    static int counter = 0;
    return std::filesystem::temp_directory_path()
        / ("navgame_test_" + std::to_string(counter++) + ".json");
  }
 
  std::filesystem::path path_;
};
 
/// Redirects std::cout for the lifetime of the object.
class CoutCapture
{
public:
  CoutCapture()
      : previous_ {std::cout.rdbuf(buffer_.rdbuf())}
  {
  }
 
  CoutCapture(CoutCapture const&)            = delete;
  CoutCapture& operator=(CoutCapture const&) = delete;
 
  ~CoutCapture() { std::cout.rdbuf(previous_); }
 
  [[nodiscard]] std::string str() const { return buffer_.str(); }
 
private:
  std::ostringstream buffer_;
  std::streambuf*    previous_;
};
 
struct PlayResult
{
  int         exit_code {};
  std::string output;
};
 
PlayResult playMap(std::vector<std::string> const& rows)
{
  TempJson const map {makeMapJson(rows)};
  auto           game = battlefieldGame {};
 
  CoutCapture capture;
  int const   code = game.play(map.path());
  return PlayResult {code, capture.str()};
}
 
PlayResult playRawJson(std::string const& contents)
{
  TempJson const map {contents};
  auto           game = battlefieldGame {};
 
  CoutCapture capture;
  int const   code = game.play(map.path());
  return PlayResult {code, capture.str()};
}
 
/// Pulls every "(x,y)" coordinate out of play()'s output, in order.
std::vector<Cell> parseCells(std::string const& text)
{
  std::vector<Cell> cells;
  std::size_t       pos = 0;
 
  while ((pos = text.find('(', pos)) != std::string::npos) {
    auto const comma = text.find(',', pos);
    auto const close = text.find(')', pos);
    if (comma == std::string::npos || close == std::string::npos || comma > close) { break; }
 
    cells.emplace_back(std::stoi(text.substr(pos + 1, comma - pos - 1)),
                       std::stoi(text.substr(comma + 1, close - comma - 1)));
    pos = close + 1;
  }
 
  return cells;
}
 
}  // namespace
 
// -----------------------------------------------------------------------------
// library
// -----------------------------------------------------------------------------
 
TEST_CASE("Name is cpp_navigation_game", "[library]")
{
  auto const lib = library {};
  REQUIRE(lib.name == "cpp_navigation_game");
}
 
// -----------------------------------------------------------------------------
// manhattanDistance
// -----------------------------------------------------------------------------
 
TEST_CASE("Distance from a cell to itself is zero", "[heuristic]")
{
  REQUIRE(battlefieldGame::manhattanDistance({3, 7}, {3, 7}) == 0);
}
 
TEST_CASE("Distance sums the axis deltas", "[heuristic]")
{
  CHECK(battlefieldGame::manhattanDistance({0, 0}, {3, 0}) == 3);
  CHECK(battlefieldGame::manhattanDistance({0, 0}, {0, 4}) == 4);
  CHECK(battlefieldGame::manhattanDistance({0, 0}, {3, 4}) == 7);
}
 
TEST_CASE("Distance is symmetric", "[heuristic]")
{
  auto const a = GENERATE(Cell {0, 0}, Cell {2, 5}, Cell {-3, 1});
  auto const b = GENERATE(Cell {4, 4}, Cell {-1, -1}, Cell {0, 9});
 
  REQUIRE(battlefieldGame::manhattanDistance(a, b)
          == battlefieldGame::manhattanDistance(b, a));
}
 
TEST_CASE("Distance is never negative", "[heuristic]")
{
  CHECK(battlefieldGame::manhattanDistance({-5, -5}, {5, 5}) == 20);
  CHECK(battlefieldGame::manhattanDistance({5, 5}, {-5, -5}) == 20);
}
 
TEST_CASE("Distance obeys the triangle inequality", "[heuristic]")
{
  auto const a = Cell {0, 0};
  auto const b = Cell {2, 3};
  auto const c = Cell {5, 1};
 
  REQUIRE(battlefieldGame::manhattanDistance(a, c)
          <= battlefieldGame::manhattanDistance(a, b)
              + battlefieldGame::manhattanDistance(b, c));
}
 
TEST_CASE("Adjacent cells are one apart, matching the unit step cost", "[heuristic]")
{
  // A* uses a step cost of 1, so the heuristic must never exceed 1 between
  // neighbours or it stops being admissible.
  auto const dir = GENERATE(Cell {1, 0}, Cell {-1, 0}, Cell {0, 1}, Cell {0, -1});
  auto const origin = Cell {4, 4};
  auto const neighbour = Cell {origin.first + dir.first, origin.second + dir.second};
 
  REQUIRE(battlefieldGame::manhattanDistance(origin, neighbour) == 1);
}
 
// -----------------------------------------------------------------------------
// play() — input validation
// -----------------------------------------------------------------------------
 
TEST_CASE("A missing file fails", "[parse]")
{
  auto game = battlefieldGame {};
 
  CoutCapture capture;
  int const   code = game.play("definitely_not_a_real_map_file.json");
 
  REQUIRE(code == 1);
  REQUIRE_THAT(capture.str(), ContainsSubstring("could not open"));
}
 
TEST_CASE("Malformed JSON fails", "[parse]")
{
  auto const result = playRawJson("{ this is not json ");
 
  REQUIRE(result.exit_code == 1);
  REQUIRE_THAT(result.output, ContainsSubstring("malformed"));
}
 
TEST_CASE("Valid JSON with the wrong shape fails", "[parse]")
{
  SECTION("no layers key") {
    REQUIRE(playRawJson(R"({"width":3,"height":3})").exit_code == 1);
  }
  SECTION("empty layers array") {
    REQUIRE(playRawJson(R"({"width":3,"height":3,"layers":[]})").exit_code == 1);
  }
  SECTION("no data key in the layer") {
    REQUIRE(playRawJson(R"({"width":3,"height":3,"layers":[{}]})").exit_code == 1);
  }
  SECTION("no width key") {
    REQUIRE(playRawJson(R"({"height":1,"layers":[{"data":[0,8]}]})").exit_code == 1);
  }
  SECTION("data is not a list of ints") {
    REQUIRE(playRawJson(R"({"width":2,"layers":[{"data":["a","b"]}]})").exit_code == 1);
  }
}
 
TEST_CASE("A non-positive width fails", "[parse]")
{
  auto const width = GENERATE(0, -1, -10);
  auto const json =
      R"({"width":)" + std::to_string(width) + R"(,"layers":[{"data":[0,8]}]})";
 
  auto const result = playRawJson(json);
 
  REQUIRE(result.exit_code == 1);
  REQUIRE_THAT(result.output, ContainsSubstring("invalid width"));
}
 
TEST_CASE("An empty tile list fails", "[parse]")
{
  auto const result = playRawJson(R"({"width":3,"height":0,"layers":[{"data":[]}]})");
 
  REQUIRE(result.exit_code == 1);
  REQUIRE_THAT(result.output, ContainsSubstring("tile map is empty"));
}
 
TEST_CASE("A map is rejected unless both a start and a target exist", "[parse]")
{
  SECTION("start but no target") {
    auto const result = playMap({"S.."});
    REQUIRE(result.exit_code == 1);
    REQUIRE_THAT(result.output, ContainsSubstring("unset"));
  }
 
  SECTION("target but no start") {
    auto const result = playMap({"T.."});
    REQUIRE(result.exit_code == 1);
    REQUIRE_THAT(result.output, ContainsSubstring("unset"));
  }
 
  SECTION("neither") {
    auto const result = playMap({"..."});
    REQUIRE(result.exit_code == 1);
    REQUIRE_THAT(result.output, ContainsSubstring("unset"));
  }
}
 
TEST_CASE("A map of only unrecognised tiles is treated as empty", "[parse]")
{
  auto const result = playMap({"???", "???"});
 
  REQUIRE(result.exit_code == 1);
  REQUIRE_THAT(result.output, ContainsSubstring("tile map is empty"));
}
 
// -----------------------------------------------------------------------------
// play() — pathfinding
// -----------------------------------------------------------------------------
 
TEST_CASE("Adjacent start and target is a single move", "[path]")
{
  auto const result = playMap({"ST"});
 
  REQUIRE(result.exit_code == 0);
  REQUIRE_THAT(result.output, ContainsSubstring("1 moves"));
  REQUIRE(parseCells(result.output) == std::vector<Cell> {{0, 0}, {1, 0}});
}
 
TEST_CASE("An open grid is crossed in Manhattan-distance moves", "[path]")
{
  auto const result = playMap({"S..", "...", "..T"});
 
  REQUIRE(result.exit_code == 0);
  REQUIRE_THAT(result.output, ContainsSubstring("valid path found"));
  REQUIRE_THAT(result.output, ContainsSubstring("4 moves"));
 
  auto const cells = parseCells(result.output);
  REQUIRE(cells.size() == 5);  // 4 moves plus the start cell
  CHECK(cells.front() == Cell {0, 0});
  CHECK(cells.back() == Cell {2, 2});
}
 
TEST_CASE("The path routes around elevated terrain", "[path]")
{
  // Straight line is 2 moves; the wall forces the long way round the right edge.
  auto const result = playMap({"S..", "##.", "T.."});
 
  REQUIRE(result.exit_code == 0);
  REQUIRE_THAT(result.output, ContainsSubstring("6 moves"));
 
  auto const cells = parseCells(result.output);
  CHECK(cells.front() == Cell {0, 0});
  CHECK(cells.back() == Cell {0, 2});
 
  for (auto const& cell : cells) {
    CHECK(cell != Cell {0, 1});
    CHECK(cell != Cell {1, 1});
  }
}
 
TEST_CASE("Every returned path is walkable", "[path]")
{
  // The reported route has to be a real one: contiguous, no repeats, correct ends.
  auto const rows = GENERATE(std::vector<std::string> {"S..", "...", "..T"},
                             std::vector<std::string> {"S..", "##.", "T.."},
                             std::vector<std::string> {"S...", ".##.", ".#..", "...T"});
 
  auto const result = playMap(rows);
  REQUIRE(result.exit_code == 0);
 
  auto const cells = parseCells(result.output);
  REQUIRE(cells.size() >= 2);
 
  for (std::size_t i = 1; i < cells.size(); ++i) {
    auto const& previous = cells[i - 1];
    auto const& current  = cells[i];
    INFO("step " << i << " from (" << previous.first << "," << previous.second << ") to ("
                 << current.first << "," << current.second << ")");
    CHECK(battlefieldGame::manhattanDistance(previous, current) == 1);
  }
 
  std::set<Cell> const unique {cells.begin(), cells.end()};
  CHECK(unique.size() == cells.size());  // no cell visited twice
}
 
TEST_CASE("A walled-off target reports no path but is not an error", "[path]")
{
  auto const result = playMap({"S#T", ".#.", ".#."});
 
  REQUIRE(result.exit_code == 0);  // unreachable is a valid outcome, not a failure
  REQUIRE_THAT(result.output, ContainsSubstring("no valid path"));
  REQUIRE(parseCells(result.output).empty());
}
 
TEST_CASE("Unrecognised tiles are holes, not walls", "[path]")
{
  // '?' cells never enter game_bounds, so they block movement the same way the
  // edge of the map does.
  auto const result = playMap({"S?T", "???"});
 
  REQUIRE(result.exit_code == 0);
  REQUIRE_THAT(result.output, ContainsSubstring("no valid path"));
}
 
TEST_CASE("A start boxed in by walls has no path", "[path]")
{
  auto const result = playMap({"S#.", "#..", "..T"});
 
  REQUIRE(result.exit_code == 0);
  REQUIRE_THAT(result.output, ContainsSubstring("no valid path"));
}
 
TEST_CASE("Movement is orthogonal only", "[path]")
{
  // Diagonal neighbours are not adjacent: with (1,0) and (0,1) walled off, the
  // target at (1,1) is unreachable even though it touches the start corner.
  auto const result = playMap({"S#", "#T"});
 
  REQUIRE(result.exit_code == 0);
  REQUIRE_THAT(result.output, ContainsSubstring("no valid path"));
}
 
TEST_CASE("A wide corridor still yields the shortest route", "[path]")
{
  auto const result = playMap({"S........T"});
 
  REQUIRE(result.exit_code == 0);
  REQUIRE_THAT(result.output, ContainsSubstring("9 moves"));
}
 
TEST_CASE("Larger maps stay optimal", "[path]")
{
  // A 10x10 spiral-ish map: only the bottom row connects the two halves.
  auto const result = playMap({"S.........",
                               "#########.",
                               ".........."
                               ,"#########."
                               ,".........."
                               ,"#########."
                               ,".........."
                               ,"#########."
                               ,".........."
                               ,"T........."});
 
  REQUIRE(result.exit_code == 0);
 
  auto const cells = parseCells(result.output);
  CHECK(cells.front() == Cell {0, 0});
  CHECK(cells.back() == Cell {0, 9});
 
  for (std::size_t i = 1; i < cells.size(); ++i) {
    CHECK(battlefieldGame::manhattanDistance(cells[i - 1], cells[i]) == 1);
  }
}
 
// -----------------------------------------------------------------------------
// play() — object reuse
// -----------------------------------------------------------------------------
 
TEST_CASE("Replaying on the same object uses the new map", "[library]")
{
  auto game = battlefieldGame {};
 
  TempJson const first {makeMapJson({"S..", "...", "..T"})};
  TempJson const second {makeMapJson({"ST"})};
 
  std::string second_output;
  {
    CoutCapture capture;
    REQUIRE(game.play(first.path()) == 0);
    REQUIRE(game.play(second.path()) == 0);
    second_output = capture.str();
  }
 
  // The second run's route must come from the second map, not the first.
  REQUIRE_THAT(second_output, ContainsSubstring("1 moves"));
}
 
TEST_CASE("A failed parse does not run a search on the previous map", "[library]")
{
  auto game = battlefieldGame {};
 
  TempJson const good {makeMapJson({"S..", "...", "..T"})};
 
  std::string output;
  {
    CoutCapture capture;
    REQUIRE(game.play(good.path()) == 0);
    REQUIRE(game.play("no_such_file.json") == 1);
    output = capture.str();
  }
 
  // Only one route should have been printed across the two calls.
  auto const first_route  = output.find("valid path found");
  auto const second_route = output.find("valid path found", first_route + 1);
  REQUIRE(second_route == std::string::npos);
}
 
// -----------------------------------------------------------------------------
// Direct parser tests — need parseMap to be reachable from the test binary.
// See the notes accompanying this file for the one-line seam.
// -----------------------------------------------------------------------------
 
#ifdef NAVGAME_TEST_INTERNALS
 
TEST_CASE("parseMap records start, target, terrain and bounds", "[parse][internal]")
{
  TempJson const file {makeMapJson({"S.#", ".#.", "..T"})};
 
  auto                            game = battlefieldGame {};
  battlefieldGame::battlefieldMap map;
 
  REQUIRE(game.parseMap(file.path(), map));
 
  CHECK(map.start_pose == Cell {0, 0});
  CHECK(map.target_pose == Cell {2, 2});
  CHECK(map.elevated_terrain == std::set<Cell> {{2, 0}, {1, 1}});
  CHECK(map.game_bounds.size() == 9);
}
 
TEST_CASE("parseMap leaves the output map untouched on failure", "[parse][internal]")
{
  auto                            game = battlefieldGame {};
  battlefieldGame::battlefieldMap map;
  map.start_pose = Cell {7, 7};
 
  TempJson const bad {"{ not json"};
  REQUIRE_FALSE(game.parseMap(bad.path(), map));
 
  CHECK(map.start_pose == Cell {7, 7});
}
 
TEST_CASE("Elevated tiles are in bounds but not walkable", "[parse][internal]")
{
  TempJson const file {makeMapJson({"S#T"})};
 
  auto                            game = battlefieldGame {};
  battlefieldGame::battlefieldMap map;
 
  REQUIRE(game.parseMap(file.path(), map));
 
  CHECK(map.inBounds({1, 0}));
  CHECK(map.elevated_terrain.count({1, 0}) == 1);
}
 
TEST_CASE("Unrecognised tiles are excluded from bounds", "[parse][internal]")
{
  TempJson const file {makeMapJson({"S?T"})};
 
  auto                            game = battlefieldGame {};
  battlefieldGame::battlefieldMap map;
 
  REQUIRE(game.parseMap(file.path(), map));
 
  CHECK_FALSE(map.inBounds({1, 0}));
  CHECK(map.game_bounds.size() == 2);
}
 
TEST_CASE("Duplicate start tiles: the last one wins", "[parse][internal]")
{
  TempJson const file {makeMapJson({"S.S", "..T"})};
 
  auto                            game = battlefieldGame {};
  battlefieldGame::battlefieldMap map;
 
  REQUIRE(game.parseMap(file.path(), map));
  CHECK(map.start_pose == Cell {2, 0});
}
 
TEST_CASE("A tile count that disagrees with width is accepted", "[parse][internal]")
{
  auto const     json = R"({"width":3,"height":2,"layers":[{"data":[0,-1,-1,-1,8]}]})";
  TempJson const file {json};
 
  auto                            game = battlefieldGame {};
  battlefieldGame::battlefieldMap map;
 
  REQUIRE(game.parseMap(file.path(), map));
  CHECK(map.target_pose == Cell {1, 1});
  CHECK(map.game_bounds.size() == 5);
}
 
// ---- aStarSearch, driven directly ----
//
// Assumed signature:
//
//   std::vector<gameCell> aStarSearch(const battlefieldMap& map,
//                                     const gameCell& start_pose,
//                                     const gameCell& target_pose,
//                                     heuristic h) const;
//
// Called through an instance below, which compiles whether the function stayed
// a const member or became static.
 
namespace {
 
/// Builds a battlefieldMap from ASCII rows without touching the filesystem.
battlefieldGame::battlefieldMap makeMap(std::vector<std::string> const& rows)
{
  battlefieldGame::battlefieldMap map;
 
  for (std::size_t y = 0; y < rows.size(); ++y) {
    for (std::size_t x = 0; x < rows[y].size(); ++x) {
      Cell const cell {static_cast<int>(x), static_cast<int>(y)};
 
      switch (rows[y][x]) {
        case 'S':
          map.start_pose = cell;
          map.game_bounds.insert(cell);
          break;
        case 'T':
          map.target_pose = cell;
          map.game_bounds.insert(cell);
          break;
        case '#':
          map.game_bounds.insert(cell);
          map.elevated_terrain.insert(cell);
          break;
        case '.':
          map.game_bounds.insert(cell);
          break;
        default:
          break;  // '?' and friends stay out of bounds entirely
      }
    }
  }
 
  return map;
}
 
int zeroHeuristic(Cell const&, Cell const&)
{
  return 0;  // reduces A* to Dijkstra
}
 
int inflatedHeuristic(Cell const& a, Cell const& b)
{
  return 5 * battlefieldGame::manhattanDistance(a, b);  // deliberately inadmissible
}
 
/// Asserts the route is one a piece could actually walk.
void checkWalkable(battlefieldGame::battlefieldMap const& map, std::vector<Cell> const& path)
{
  REQUIRE_FALSE(path.empty());
  CHECK(path.front() == map.start_pose);
  CHECK(path.back() == map.target_pose);
 
  for (auto const& cell : path) {
    INFO("cell (" << cell.first << "," << cell.second << ")");
    CHECK(map.inBounds(cell));
    CHECK(map.elevated_terrain.count(cell) == 0);
  }
 
  for (std::size_t i = 1; i < path.size(); ++i) {
    INFO("step " << i);
    CHECK(battlefieldGame::manhattanDistance(path[i - 1], path[i]) == 1);
  }
 
  std::set<Cell> const unique {path.begin(), path.end()};
  CHECK(unique.size() == path.size());
}
 
std::vector<Cell> search(battlefieldGame::battlefieldMap const& map,
                         battlefieldGame::heuristic            h = &battlefieldGame::manhattanDistance)
{
  auto const game = battlefieldGame {};
  return game.aStarSearch(map, map.start_pose, map.target_pose, h);
}
 
}  // namespace
 
TEST_CASE("aStarSearch crosses an open grid in the fewest moves", "[search][internal]")
{
  auto const map  = makeMap({"S....", ".....", ".....", ".....", "....T"});
  auto const path = search(map);
 
  REQUIRE(path.size() == 9);  // 8 moves
  checkWalkable(map, path);
}
 
TEST_CASE("aStarSearch returns optimal routes around terrain", "[search][internal]")
{
  SECTION("adjacent cells")
  {
    auto const map  = makeMap({"ST"});
    auto const path = search(map);
    REQUIRE(path.size() == 2);
    checkWalkable(map, path);
  }
 
  SECTION("a wall forces the long way round")
  {
    // Straight line is 2 moves; the wall makes it 6.
    auto const map  = makeMap({"S..", "##.", "T.."});
    auto const path = search(map);
    REQUIRE(path.size() == 7);
    checkWalkable(map, path);
  }
 
  SECTION("a small maze")
  {
    auto const map  = makeMap({"S...", ".##.", ".#..", "...T"});
    auto const path = search(map);
    REQUIRE(path.size() == 7);
    checkWalkable(map, path);
  }
 
  SECTION("a long snaking corridor")
  {
    auto const map = makeMap({"S.........",
                              "#########.",
                              "..........",
                              ".#########",
                              "..........",
                              "#########.",
                              "..........",
                              ".#########",
                              "..........",
                              "T........."});
    auto const path = search(map);
    REQUIRE(path.size() == 46);  // 45 moves
    checkWalkable(map, path);
  }
}
 
TEST_CASE("aStarSearch returns nothing when the target is unreachable", "[search][internal]")
{
  SECTION("a wall divides the map")
  {
    auto const map = makeMap({"S#T", ".#.", ".#."});
    REQUIRE(search(map).empty());
  }
 
  SECTION("the target is walled in")
  {
    auto const map = makeMap({"S....", ".###.", ".#T#.", ".###.", "....."});
    REQUIRE(search(map).empty());
  }
 
  SECTION("the start is walled in")
  {
    auto const map = makeMap({"S#.", "#..", "..T"});
    REQUIRE(search(map).empty());
  }
 
  SECTION("unrecognised tiles are holes, not walls")
  {
    auto const map = makeMap({"S?T", "???"});
    REQUIRE(search(map).empty());
  }
 
  SECTION("diagonals are not adjacency")
  {
    auto const map = makeMap({"S#", "#T"});
    REQUIRE(search(map).empty());
  }
}
 
TEST_CASE("A start already on the target is a path of one cell", "[search][internal]")
{
  // Not expressible in the JSON format, since a tile holds a single value.
  auto map = makeMap({"S.."});
  map.target_pose = map.start_pose;
 
  auto const path = search(map);
 
  REQUIRE(path.size() == 1);
  REQUIRE(path.front() == map.start_pose);
}
 
TEST_CASE("The choice of admissible heuristic does not change the cost", "[search][internal]")
{
  auto const rows = GENERATE(std::vector<std::string> {"S....", ".....", "....T"},
                             std::vector<std::string> {"S..", "##.", "T.."},
                             std::vector<std::string> {"S...", ".##.", ".#..", "...T"});
 
  auto const map = makeMap(rows);
 
  auto const with_manhattan = search(map, &battlefieldGame::manhattanDistance);
  auto const with_zero      = search(map, &zeroHeuristic);
 
  REQUIRE_FALSE(with_manhattan.empty());
  REQUIRE(with_manhattan.size() == with_zero.size());  // both must be optimal
  checkWalkable(map, with_zero);
}
 
TEST_CASE("Repeated searches give identical results", "[search][internal]")
{
  auto const map = makeMap({"S...", ".##.", ".#..", "...T"});
 
  REQUIRE(search(map) == search(map));
}
 
// ---- Behaviour at the edges of the contract ----
// These record what the search currently does with inputs parseMap can't
// produce. Each is a decision worth making on purpose.
 
TEST_CASE("A target on elevated terrain is unreachable", "[search][internal][contract]")
{
  // Neighbours are filtered before the goal test, so an elevated goal is never
  // reached even with open ground all around it.
  auto map = makeMap({"S.T"});
  map.elevated_terrain.insert(map.target_pose);
 
  REQUIRE(search(map).empty());
}
 
TEST_CASE("A start on elevated terrain is still searched from", "[search][internal][contract]")
{
  // The start is never checked against elevated_terrain, only its neighbours,
  // so a piece standing on a wall can walk off it.
  auto map = makeMap({"S.T"});
  map.elevated_terrain.insert(map.start_pose);
 
  auto const path = search(map);
 
  REQUIRE(path.size() == 3);
  CHECK(map.elevated_terrain.count(path.front()) == 1);  // route begins on a wall
}
 
TEST_CASE("A start outside the map is not rejected", "[search][internal][contract]")
{
  // inBounds is applied to neighbours only, so an out-of-bounds start that
  // happens to touch the grid produces a path whose first cell is off-map.
  auto const map = makeMap({"S..", "...", "..T"});
 
  auto const game = battlefieldGame {};
  auto const path =
      game.aStarSearch(map, Cell {-1, 0}, map.target_pose, &battlefieldGame::manhattanDistance);
 
  REQUIRE_FALSE(path.empty());
  CHECK(path.front() == Cell {-1, 0});
  CHECK_FALSE(map.inBounds(path.front()));
}
 
TEST_CASE("An empty map still resolves a zero-length search", "[search][internal][contract]")
{
  battlefieldGame::battlefieldMap map;  // no bounds, no terrain
  map.start_pose  = Cell {0, 0};
  map.target_pose = Cell {0, 0};
 
  REQUIRE(search(map).size() == 1);
}
 
// Hidden tag: run deliberately with `./tests "[inadmissible]"`.
//
// An overestimating heuristic breaks the guarantee the reconstruction loop
// relies on. g_score and came_from are rewritten for neighbours that have
// already been expanded, and with an inconsistent heuristic that rewrite can
// point a cell back through its own ancestor. The `for (c = current; c !=
// start_pose; c = came_from.at(c))` loop has no cycle guard, so it can spin
// forever rather than fail. Treat a hang here as the finding.
TEST_CASE("An inadmissible heuristic may return a suboptimal route",
          "[.][search][internal][inadmissible]")
{
  auto const map = makeMap({"S..", "##.", "T.."});
 
  auto const optimal  = search(map, &battlefieldGame::manhattanDistance);
  auto const inflated = search(map, &inflatedHeuristic);
 
  REQUIRE_FALSE(inflated.empty());
  CHECK(inflated.size() >= optimal.size());
  checkWalkable(map, inflated);  // must at least still be a real route
}
 
#endif  // NAVGAME_TEST_INTERNALS
