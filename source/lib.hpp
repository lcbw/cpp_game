#pragma once
#include <array>
#include <set>
#include <utility>
#include <vector>
#include <string>

/**
 * @brief The core implementation of the executable
 *
 * This class makes up the library part of the executable, which means that the
 * main logic is implemented here. This kind of separation makes it easy to
 * test the implementation for the executable, because the logic is nicely
 * separated from the command-line logic implemented in the main function.
 */
class library
{
public:
    library();
    std::string name;
};


class battlefieldGame
{
public:
    using gameCell = std::pair<int,int>;
    using heuristic = int (*)(const gameCell&, const gameCell&);

    // in case these values ever need to change for a user
    static constexpr int CELL_START    =  0;
    static constexpr int CELL_TARGET   =  8;
    static constexpr int CELL_ELEVATED =  3;
    static constexpr int CELL_OPEN     = -1;

    struct battlefieldMap
    {   
        gameCell start_pose {};
        gameCell target_pose {};
        std::set<gameCell> elevated_terrain;
        std::set<gameCell> game_bounds;
        bool inBounds(const gameCell & querycell) const
        {
            return game_bounds.find(querycell) != game_bounds.end();
        }
    // struct battlefield map - has start pose (pair int int), target pose (pair int int),
    // elevated terrain set(pair(int,int)), reachable terrain set(pair(int,int)) 
    };

    // Main ex - parses the provided map in json form, searches the map, and couts the result. 
    // Returns 0 on success, 1 on failure with cout reported error state. 
    int play(const std::string& json_file);

    static int manhattanDistance(const gameCell& potential_path,
                                 const gameCell& target_pose);

#ifdef NAVGAME_TEST_INTERNALS
public:
#else
private:
#endif
    bool parseMap(const std::string& json_file, battlefieldMap& out_map);

    std::vector<gameCell> aStarSearch(const battlefieldMap& map,
                                      const gameCell& start_pose,
                                      const gameCell& target_pose,
                                      heuristic h) const;

private:
    battlefieldMap gameMap_;

    static constexpr std::array<gameCell, 4> moveDirs {{
        {1, 0}, {-1, 0}, {0, 1}, {0, -1}
    }};
};
