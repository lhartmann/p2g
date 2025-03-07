#pragma once
#include <pcb2gcode.hpp>

namespace pcb2gcode {

static void metapath_sort_anneal_nodeswap(metapaths_t::iterator begin, metapaths_t::iterator end) {
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
    size_t iterlimit = n*n*1000;
    size_t changedelay = n;
    for (size_t  i = 0; i < iterlimit; i++) {
        size_t a = rand() * n / RAND_MAX;
        size_t b = size_t(rand() * n * double(iterlimit-i) / iterlimit / RAND_MAX + 1) % n;

        double oldcost = cost(a,(a+1)%n) + cost((a+1)%n,(a+2)%n) + cost(b,(b+1)%n) + cost((b+1)%n,(b+2)%n);
        double newcost = cost(a,(b+1)%n) + cost((b+1)%n,(a+2)%n) + cost(b,(a+1)%n) + cost((a+1)%n,(b+2)%n);
        if (newcost < oldcost || exp(-(newcost-oldcost) / i * iterlimit) >= double(rand()) / RAND_MAX) {
            changedelay = n;
            std::swap(*(begin+(a+1)%n), *(begin+(b+1)%n));
        }
    }
    DEBUG("      Cost after: " << totalCost() << "\x1B[K");
}

}
