#include "lib.hpp"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdlib>
#include <string>
#include <set>
#include <utility>
#include <map>
#include <queue>
#include <functional>
#include <fmt/core.h>
#include <cstdlib>
#include <cmath>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

library::library()
    : name {fmt::format("{}", "cpp_navigation_game")}
{
}

bool battlefieldGame::parseMap (const std::string & json_file, battlefieldMap & gameMap)
{
    std::ifstream input {json_file};
    if (!input.is_open())
    {
        std::cout << "is your path wrong? could not open: " << json_file << "\n";
        return false;
    }

    json data;
    battlefieldMap tempGameMap;
    std::pair<bool,bool> start_pose_target_pose_set = {false,false};
    try {
        data = json::parse(input);
        std::vector<int> cells = data.at("layers").at(0).at("data").get<std::vector<int>>();
        // I wasn't told how the data is ordered, so I'm assuming 
        // my data will start at pose (0,0) and will increment x first. 
        // I'm assuming the data is 'complete' - i.e. for every cell in my grid
        // There is an item in my data vector. This means I don't 
        // need to check if my goal or start pose is in bounds, because 
        // these values were provided *from* the complete dataset.
        // I'm incorrectly assuming this to be square
        int num_cols = data.at("tilesets").at(0).at("tilewidth").get<int>();
        if (num_cols <= 0)
        {
            std::cout << "empty map or invalid width: " << num_cols << "\n";
            return false;
        }
        for (std::size_t i = 0; i < cells.size(); ++i)
        {    
            int x = static_cast<int>(i) % num_cols;
            int y = static_cast<int>(i) / num_cols;
            int cell_value = cells[i];

            // if for some reason there are cells that don't match these values, 
            // we will just ignore it.
            switch (cell_value) {
                case CELL_START: 
                    { 
                        gameCell start_pose = {x,y};
                        tempGameMap.start_pose = start_pose;
                        start_pose_target_pose_set.first = true;
                        tempGameMap.game_bounds.insert({x,y});
                        break;
                    };
                case CELL_TARGET: 
                    {
                        tempGameMap.game_bounds.insert({x,y});
                        gameCell target_pose = {x,y};
                        tempGameMap.target_pose = target_pose;
                        start_pose_target_pose_set.second = true;
                        break;
                    }                
                case CELL_ELEVATED: //obstacle
                    {
                        tempGameMap.game_bounds.insert({x,y});
                        tempGameMap.elevated_terrain.insert({x,y});
                        break;
                    }
                case CELL_OPEN:
                    {   
                        tempGameMap.game_bounds.insert({x,y});
                        break;
                    }
                
            }
    }
}

    catch (const json::exception & e)
    {
        std::cout << "json file was not able to be parsed - is it malformed?"
            << e.what() << "\n";
        return false;
    }  

    if (tempGameMap.game_bounds.size() <= 0)
        {
        std::cout << "tile map is empty - is your json malformed?" << "\n";
        return false;
        }

    if (!start_pose_target_pose_set.first || !start_pose_target_pose_set.second)
        {
        std::cout << "start pose and/or target pose unset - is your json malformed?" << "\n";
        return false;
        }
    
    gameMap = std::move(tempGameMap); 
    return true;
}


int battlefieldGame::manhattanDistance (const gameCell & potential_path, const gameCell & target_pose)
{
    // euclidean distance loses meaning in a discretized grid with movements constrained to 'up/down, left/right' 
    // manhattan is slightly cheaper and remains low cost as our grid expands in size
    int manhattan_distance = std::abs(target_pose.first - potential_path.first) + std::abs(target_pose.second - potential_path.second);
    return manhattan_distance;
}

std::vector<battlefieldGame::gameCell> battlefieldGame::aStarSearch (const battlefieldMap& map, const gameCell & start_pose, 
    const gameCell & target_pose, 
    heuristic h) const
    {
    // (f_score, cell). std::greater turns the max-heap into a min-heap, so
    // top() is the most promising cell.
    using cellAndCost = std::pair<int,gameCell>;
    std::priority_queue<cellAndCost, std::vector<cellAndCost>, std::greater<cellAndCost>> open_set;
    std::vector<gameCell> path; // storing our potential path
 
    std::map<gameCell, int>      g_score;    // track cost from start
    std::map<gameCell, gameCell> came_from;  // for rebuilding the path
    std::set<gameCell>           visited;     // visited cells
 
    g_score[start_pose] = 0;
    open_set.push({h(start_pose, target_pose), start_pose});
 
    while (!open_set.empty())
    {
        gameCell current = open_set.top().second;
        open_set.pop();
 
        if (current == target_pose)
        {
            // Go through our 'came_from' chain and flip it once we're back at the 
            // start
            for (gameCell c = current; c != start_pose; c = came_from.at(c))
            {
                path.push_back(c);
            }
            path.push_back(start_pose);
            std::reverse(path.begin(), path.end());
            return path;
        }
 
        // ignore stale cells
        if (!visited.insert(current).second)
        {
            continue;
        }
 
        for (const gameCell& dir : moveDirs)
        {
            gameCell neighbor {current.first + dir.first,
                               current.second + dir.second};
 
            if (!map.inBounds(neighbor))                    continue;
            if (map.elevated_terrain.count(neighbor) != 0)  continue;
 
            int tentative_g = g_score.at(current) + 1; 
 
            auto it = g_score.find(neighbor);
            // if we find the neighbor has a g_score and it's less than our new score 
            // then we don't update it (our heuristic value doesn't change for the cell ever)
            if (it != g_score.end() && tentative_g >= it->second)
            {
                continue;  
            }
 
            g_score[neighbor]   = tentative_g;
            came_from[neighbor] = current;
            open_set.push({tentative_g + h(neighbor, target_pose), neighbor});
        }
    }
 
    return {};  // investigated all valid neighbors without reaching the target
}

int battlefieldGame::play(const std::string& json_file)
{
    if (!battlefieldGame::parseMap(json_file, gameMap_))
        {
            std::cout << "json file not found or file is malformed, exiting game" << "\n";
            return 1;
        }

    std::vector<gameCell> path_to_goal {};

    path_to_goal = battlefieldGame::aStarSearch(gameMap_, gameMap_.start_pose, gameMap_.target_pose, &manhattanDistance);
    
    if (path_to_goal.empty())
        {
            std::cout << "no valid path was found - target unreachable" << "\n";
            return 0; 
        }
    
    std::cout << "valid path found - " << path_to_goal.size() - 1 << " moves:\n";
    for (const gameCell& cell : path_to_goal)
    {
        std::cout << "(" << cell.first << "," << cell.second << ") ";
    }
    std::cout << "\n";

    return 0;
}






