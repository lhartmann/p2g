#pragma once
#include <pcb2gcode.hpp>

namespace pcb2gcode {

static void metapath_sort_greed_insert(metapaths_t::iterator begin, metapaths_t::iterator end) {
    DEBUG("    Sorting for priority " << begin->priority << "...");
    using std::max, std::abs, std::sqrt;
    auto cost = [&](int a, int b) {
        double dx = (begin+a)->entry.x - (begin+b)->entry.x;
        double dy = (begin+a)->entry.y - (begin+b)->entry.y;

        // Euclidian
//        return sqrt(dx*dx + dy*dy);

        // Quasi-Euclidian
//        return dx*dx + dy*dy;

        // Manhattan
//        return fabs(fdx) + abs(fdy);

        // CNC
//        return fmax(fabs(dx), (dy));

        // CNC modified
        return fmax(fabs(dx), fabs(dy)) + 0.3*fmin(abs(dx), abs(dy)) ;
    };

    size_t n = std::distance(begin, end);

    auto totalCost = [&]() {
        double c=0;
        for (size_t i=1; i<n; i++) {
            c += cost(i-1,i);
        }
        return c + cost(0, n-1);
    };

    DEBUG("      Cost before: " << totalCost());

    // Randomized initial conditon gives better results
    std::random_shuffle(begin, end);

    // Stage 1: Greed/selection
    size_t g = 1;
    while (g < n*3/4) {
        size_t bi = g;
        double bc = cost(g-1, g);
        for (size_t i=g+1; i<n; i++) {
            double c = cost(g-1, i);

            if (bc > c) {
                bc = c;
                bi = i;
            }
        }

        if (g != bi) std::swap(*(begin+bi), *(begin+g));

        g++;
    }

    // Stage 2: Insert
    while (g < n-1) {
        size_t bi = 1;
        double bc = cost(0, g) + cost(g,1) - cost(0,1);
        for (size_t i=1; i<g-1; i++) {
            double c = cost(i-1, g) + cost(g,i) - cost(i-1,i);

            if (bc > c) {
                bc = c;
                bi = i;
            }
        }

        while (bi<g)
            std::swap(*(begin + bi++), *(begin + g));

        g++;
    }

    DEBUG("      Cost after: " << totalCost() << "\x1B[K");
}

}
