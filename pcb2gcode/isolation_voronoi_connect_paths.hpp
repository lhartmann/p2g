#pragma once

#include <pcb2gcode.hpp>

namespace pcb2gcode {

// Paths are considered joinable if there are exactly 2 paths ending at a given point.
// If they are, make them the same
paths_t connect_paths(paths_t paths) {
    // Paths connecting to each point
    auto indexof = [](point_t p) -> uint64_t {
        return p.y*0x100000000ULL + p.x;
    };
    // map[indexof(point)] -> vector of path indexes
    std::map< uint64_t, std::vector<int> > points;

    // Build an index of endpoints->paths
    for (size_t i=0; i<paths.size(); i++) {
        // Add each path to the endpoints
        points[indexof(paths[i].points.front())].push_back(i);
        points[indexof(paths[i].points.back())].push_back(i);
    }

    // Iterate endpoints
    for (auto &pt : points) {
        // Paths are joinable if only 2 end at the same point
        if (pt.second.size() != 2)
            continue;

        // Ignore closed paths
        if (pt.second[0] == pt.second[1]) {
            pt.second.clear();
            continue;
        }

        // Aliases
        points_t &p0 = paths[pt.second[0]].points;
        points_t &p1 = paths[pt.second[1]].points;
        point_t otherPoint;

        // The joined path, minus the duplicate point
        points_t tmp(p0.size() + p1.size() -1);

        if (p0.back() == p1.front()) {
            // The easy join, just concatenate
            std::copy(p0.begin(), p0.end(), tmp.begin());
            std::copy(p1.rbegin(), p1.rend()-1, tmp.rbegin());
            otherPoint = p1.back();

        } else if (p0.back() == p1.back()) {
            // Reverse the second path while joining
            std::copy(p0.begin(), p0.end(), tmp.begin());
            std::copy(p1.begin(), p1.end()-1, tmp.rbegin());
            otherPoint = p1.front();

        } else if (p0.front() == p1.front()) {
            // Reverse the first path
            std::copy(p0.rbegin(), p0.rend(), tmp.begin());
            std::copy(p1.rbegin(), p1.rend()-1, tmp.rbegin());
            otherPoint = p1.back();

        } else if (p0.front() == p1.back()) {
            // reverse both
            std::copy(p0.rbegin(), p0.rend(), tmp.begin());
            std::copy(p1.begin(), p1.end()-1, tmp.rbegin());
            otherPoint = p1.front();

        } else {
            DEBUG("Error joining paths?");
            continue;
        }

        // Store joined paths on first position, clear second.
        p0 = tmp;
        p1.clear();

        // update indexes
        for (auto &index : points[indexof(otherPoint)]) {
            if (index == pt.second[1])
                index = pt.second[0];
        }
        pt.second.clear();
    }

    // Remove remaining empty paths
    paths_t res;
    for (auto &path : paths)
        if (path.points.size())
            res.push_back(path);
    return res;
}

}
