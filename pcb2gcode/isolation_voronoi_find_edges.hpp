#pragma once

#include <pcb2gcode.hpp>

namespace pcb2gcode {

paths_t isolation_voronoi_find_edges(
    const cv::Mat &src,
    std::function<void(size_t, size_t)> progress = [](size_t,size_t){}
) {
    // List of open paths for milling.
    // Closed paths should have front() == back()
    paths_t paths;

    // Indexes of open-path-ends seen of pixels above, per x coordinate
    std::vector< int > path_up(src.cols+1);
    for (auto &p : path_up) p = -1;

    // Index for path with an open end coming from the left
    int path_left = -1;

    // DEBUG: A couple of paths to hold all from-to point pairs
// 	paths.push_back(path_t()); // paths[0][i] = from[i]
// 	paths.push_back(path_t()); // paths[1][i] = to[i]

    // Getpixel, with boundary checking
    auto at = [&](int x, int y) -> int32_t {
        if (x < 0 || x >= src.cols || y < 0 || y >= src.rows)
            return -1;
        return src.at<int32_t>(y,x);
    };

    for (size_t y = 0; y <= (unsigned)src.rows; y++) {
        progress(y, src.rows);
        for (size_t x = 0; x <= (unsigned)src.cols; x++) {
            int32_t tl = at(x-1, y-1);
            int32_t tr = at(x,   y-1);
            int32_t br = at(x,   y  );
            int32_t bl = at(x-1, y  );

            // Paths are detected by the difference of color between pair of pixels.
            // There may be paths in 2-4 directionsup, down, left and/or right.
            // A single direction is not possible.
            bool top    = tl != tr;
            bool left   = tl != bl;
            bool bottom = br != bl;
            bool right  = br != tr;

            point_t p(x,y);

            // DEBUG: Only 2 directions need to be saved:
            // Bottom now will be top of y+1, and right now will be left of x+1
// 			if (bottom) {
// 				paths[0].points.push_back(p);
// 				paths[1].push_back(point_t(x,y+1));
// 			}
// 			if (right) {
// 				paths[0].points.push_back(p);
// 				paths[1].push_back(point_t(x+1,y));
// 			}

            if (top + left + bottom + right >= 3 || (top && left) || (bottom && right)) {
                // All the conditions tha open/close a path are here:
                //   3 or more ends: break all paths apart.
                //   Top && left: Close both paths separately
                //   Bottom && right: Opens 2 new paths

                // Close top path
                if (top) {
                    if (path_up[x] == -1) {
//						cerr << "BROKEN PATH, up, at (" << x << "," << y << ")!" << endl;
                    } else {
                        paths[path_up[x]].points.push_back(p);
                    }
                    path_up[x] = -1;
                }

                // Close left path
                if (left) {
                    if (path_left == -1) {
//						cerr << "BROKEN PATH, left, at (" << x << "," << y << ")!" << endl;
                    } else {
                        paths[path_left].points.push_back(p);
                    }
                    path_left = -1;
                }

                // Open new path to the right
                if (right) {
                    path_left = paths.size();
                    paths.push_back(path_t());
                    paths[path_left].points.push_back(p);
                }

                // Open new path to bottom
                if (bottom) {
                    path_up[x] = paths.size();
                    paths.push_back(path_t());
                    paths[path_up[x]].points.push_back(p);
                }

            } else if (left && bottom) {
                // redirect path
                if (path_left == -1) {
//					cerr << "BROKEN PATH, left, at (" << x << "," << y << ")!" << endl;
                } else {
                    paths[path_left].points.push_back(p);
                    path_up[x] = path_left;
                }
                path_left = -1;

            } else if (top && right) {
                // Redirect path
                if (path_up[x] == -1) {
//					cerr << "BROKEN PATH, up, at (" << x << "," << y << ")!" << endl;
                } else {
                    paths[path_up[x]].points.push_back(p);
                    path_left = path_up[x];
                }
                path_up[x] = -1;

            } else if (left && right) {
                // pass straight through
                if (path_left == -1) {
//					cerr << "BROKEN PATH, left, at (" << x << "," << y << ")!" << endl;
                } else {
                    paths[path_left].points.push_back(p);
                }
            } else if (top && bottom) {
                // pass straight through
                if (path_up[x] == -1) {
//					cerr << "BROKEN PATH, up, at (" << x << "," << y << ")!" << endl;
                } else {
                    paths[path_up[x]].points.push_back(p);
                }
            }
        }
    }
    progress(src.rows, src.rows);

    // Remove empty paths
    std::vector< path_t > res;
    for (auto path : paths) {
        if (path.points.size()) {
            res.push_back(path);
        }
    }
    return res;
}

}
