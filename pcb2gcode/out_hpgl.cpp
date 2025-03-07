#include <pcb2gcode.hpp>
#include <boost/range/adaptor/reversed.hpp>

using namespace std;
namespace pcb2gcode {

//int DebugImageSave_counter=0;

#define fail(s) (throw std::string(s))

//#include "hilbert.h"

std::vector<std::string> out_hpgl(context_t &context, const metapaths_t &paths, bool mirror) {
	if (paths.empty())
		return {};

	std::vector<std::string> r;
	double mmpp	= 1/context.ppmm;
	double zsafe   = context.yaml["zsafe"].as<double>(25);
	double ztravel = context.yaml["ztavel"].as<double>(3);

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
		return int((x*mmpp + context.bounds.x) / 0.025 + 0.5);
	};
	auto Y = [&](double y) {
		return int((-y*mmpp + context.bounds.y + context.bounds.height) * (mirror ? -1 : +1) / 0.025 + 0.5);
	};

	F("IN;");
	F("PW1,%f;") % (paths[0].tool->diameter * 100000); // HACK: 100k needed by a bungard bug.
	F("SP1;");

	bool pen_down = false;
	point_t lastpos(-1,-1);
	for (auto &metapath : paths) {
		points_t &points = metapath.path->points;
		if (!points.size()) continue;

		auto &tool = *metapath.tool;
		if (tool.type != tool_t::mill)
			continue;

		bool backwards = metapath.backwards;

		if (backwards) {
			F("PU%d,%d;") % X(points.back().x) % Y(points.back().y);
			F("PD;");
			for (const auto &point : boost::adaptors::reverse(points))
				F("PA%d,%d;") % X(point.x) % Y(point.y);
		} else {
			F("PU%d,%d;") % X(points.front().x) % Y(points.front().y);
			F("PD;");
			for (const auto &point : points)
				F("PA%d,%d;") % X(point.x) % Y(point.y);
		}
	}

	F("PU0,0;");
	F("SP;");
	// Do not go home! Gerbers are frequently far from origin.

	return vs;
}

}
