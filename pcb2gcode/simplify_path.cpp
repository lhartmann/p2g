#include <pcb2gcode.hpp>

namespace pcb2gcode {

points_t simplify_path(const points_t &src, double tol) {
    using std::min;

    const auto triarea = [](point_t a, point_t b, point_t c) -> int64_t {
        int64_t A =
            + (int64_t(a.x) - int64_t(c.x)) * (int64_t(b.y) - int64_t(a.y))
            - (int64_t(a.x) - int64_t(b.x)) * (int64_t(c.y) - int64_t(a.y));
        return A >= 0 ? +A : -A;
    };

    // Reduce path keeping only:
    points_t res;

    // - First point.
    res.push_back(src.front());

    // - some others

    if (true) {
        for (auto &pt : src) {
            int dx = pt.x - res.back().x;
            int dy = pt.y - res.back().y;
            if (dx*dx + dy*dy > tol*tol) {
                res.push_back(pt);
            }
        }
    }

    int prev = 0;
    int colinear = 0;
    if (false) for (size_t i=1; i<src.size()-1; i++) {
        int64_t area = triarea(src[prev], src[i], src[(prev+i) / 2]);
        if (!area) colinear++;
        if (area >= tol) {

            if (colinear > tol) {
                res.push_back(src[i-1]);
                prev = i-1;
            } else {
                res.push_back(src[i]);
                prev = i;
            }

            colinear = 0;
        }
    }

    // - Last point.
    res.push_back(src.back());

    return res;
}

path_t simplify_path(const path_t &src, double tol) {
    path_t r = src.metacopy();
    r.points = simplify_path(src.points, tol);
    return r;
}

}
