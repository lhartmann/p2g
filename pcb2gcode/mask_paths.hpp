#pragma once
#include <pcb2gcode.hpp>

namespace pcb2gcode {

// Remove all points where predicate is false.
static paths_t mask_paths(const paths_t &src, std::function<bool(const point_t&)> predicate) {
    paths_t paths;
    paths.reserve(src.size());
    for (auto &p : src) {
        path_t path = p.metacopy();
        points_t &points = path.points;
        points.reserve(p.points.size());
        for (auto &pt : p.points) {
            if (predicate(pt)) {
                points.push_back(pt);
            } else if (!points.empty()) {
                paths.push_back(path);
                points.clear();
            }
        }
        if (!points.empty())
            paths.push_back(path);
    }

    return paths;
}

}
