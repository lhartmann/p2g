#include <pcb2gcode.hpp>
#include <boost/range/adaptor/reversed.hpp>

using namespace std;

namespace pcb2gcode {

int DebugImageSave_counter=0;

struct StatisticsCollector {
    static double distance(point_t from, point_t to) {
        double dx = (from.x - to.x);
        double dy = (from.y - to.y);
        return sqrt(dx * dx + dy * dy);
    }

    double mill_time {0};
    double mill_distance {0};
    double fast_distance {0};

    double x{NAN}, y, z;
    tool_t tool;

    static double motion_time(double d, double vm=1000, double am=60000){
        d = fabs(d); // Convert distance to mm
        double tm = vm / am; // Time to max speed
        double dm = vm * vm / 2 / am; // Distance to max speed

        d /= 2;
        if (d < dm)
            return 2 * (sqrt(2 * d / am));
        else
            return 2 * (tm + (d - dm) / vm);
    }

    void operator()(double nx, double ny, double nz) {
        double dx = nx-x;
        double dy = ny-y;
        double dz = nz-z;
        double d = sqrt(dx*dx+dy*dy);

        bool fast = !tool.feed || isnan(dz) || z>0 && nz>0;

        x = nx;
        y = ny;
        z = nz;

        if (!isnan(d)) {
            mill_time += motion_time(d, fast ? 1000 : tool.feed);
            if (fast)
                fast_distance += d;
            else
                mill_distance += d;
        }

        if (!isnan(dz))
            mill_time += motion_time(dz, tool.plunge);
    }
    void operator()(double nx, double ny) {
        (*this)(nx,ny,z);
    }
    void operator()(double nz) {
        (*this)(x,y,nz);
    }
};

#define fail(s) (throw std::string(s))

std::vector<std::string> out_gcode(context_t &context, const metapaths_t &paths, bool mirror) {
    if (paths.empty())
        return {};

    std::vector<std::string> r;
    const double mmpp    = 1/context.ppmm;
    const double zsafe   = context.yaml["zsafe"].as<double>(25);
    const double ztravel = context.yaml["ztravel"].as<double>(3);
    StatisticsCollector st;

    std::vector<std::string> vs;

    // vs.push_back(str(boost::format( ... ) % ... )); // would be very verbose.
    // F(...) % ...; // is shorter.
    struct aux : public boost::format {
        vector<string> &vs;
        aux(vector<string> &_vs, std::string s) : boost::format(s), vs(_vs) {}
        ~aux() { vs.push_back(str()); }
    };
    auto F = [&vs](std::string s) {
        return aux(vs, s);
    };

    auto X = [&](double x) {
        return x*mmpp + context.bounds.x;
    };
    auto Y = [&](double y) {
        return (-y*mmpp + context.bounds.y + context.bounds.height) * (mirror ? -1 : +1);
    };

    F("G90 ;  Absolute positioning");
    F("G21 ; Metric, mm");
    F("G17 ; XY plane");
    F("G94 ; units/minute feed rates");
//     F("G00 Z%f ; Safe height") % zsafe;

    bool pen_down = false;
    point_t lastpos(-1,-1);
    for (auto &metapath : paths) {
        auto &path = *metapath.path;
        auto &tool = *metapath.tool;
        bool backwards = metapath.backwards;
        double depth = 0;
        st.tool = tool;
        F("M3 S%f ; Tool speed") % tool.speed;

        do {
            depth = min(depth + tool.infeed, tool.depth);
            point_t entry = backwards ? path.points.back() : path.points.front();

            // Move to start of path
            if (entry != lastpos) {
                st(ztravel);
                F("G00 Z%f") % ztravel;
                pen_down = false;
            }
            if (!pen_down) {
                st(X(entry.x), Y(entry.y));
                F("G00 X%f Y%f") % X(entry.x) % Y(entry.y);
            }

            // Plunge
            st(-depth);
            F("G01 Z%f F%f") % -depth % tool.plunge;
            pen_down = true;

            // Trace path (only for mills)
            if (tool.type == tool_t::mill) {
                F("F%f") % tool.feed; // Linear feed for XY moves

                if (backwards) {
                    for (const auto &point : boost::adaptors::reverse(path.points)) {
                        st(X(point.x), Y(point.y));
                        F("G01 X%f Y%f") % X(point.x) % Y(point.y);
                    }
                } else {
                    for (const auto &point : path.points) {
                        st(X(point.x), Y(point.y));
                        F("G01 X%f Y%f") % X(point.x) % Y(point.y);
                    }
                }
            }

            lastpos = backwards ? path.points.front() : path.points.back();

            // Open paths must be reversed every pass. Closed ones don't.
            if (path.points.front() != path.points.back())
                backwards = !backwards;
        } while (depth != tool.depth);
    }

    st(zsafe);
    F("G00 Z%f   ; Safe Height") % zsafe;
    F("M5        ; Stop spindle");
    // Do not go home! Gerbers are frequently far from origin.

    // Output statistics
    F("; Total mill distance: %f") % st.mill_distance;
    F("; Total fast distance: %f") % st.fast_distance;
    F("; Expected duration:   %f") % st.mill_time;

    return vs;
}

}
