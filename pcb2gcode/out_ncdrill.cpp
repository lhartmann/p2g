#include <pcb2gcode.hpp>

using namespace std;
namespace pcb2gcode {

//int DebugImageSave_counter=0;

#define fail(s) (throw std::string(s))

//#include "hilbert.h"

std::vector<std::string> out_ncdrill(context_t &context, const metapaths_t &paths, bool mirror) {
	if (paths.empty())
		return {};

	std::vector<std::string> r;
	double mmpp	= 1/context.ppmm;

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

	// Format pixel coordinates -> 10.000's of an inch in board space.
	auto X = [&](double x) -> int {
		return (x*mmpp + context.bounds.x) / 25.4 * 10000 + 0.5;
	};
	auto Y = [&](double y) -> int {
		return (-y*mmpp + context.bounds.y + context.bounds.height) * (mirror ? -1 : +1) / 25.4 * 10000 + 0.5;
	};

	// Header, HACK: Hardcoded for bungard CNC
	F("M48     ; Start of header");
	F("INCH,TZ ; Inches, with trailing zero suppression");

	// Tool list, in-memory
	std::vector<tool_t *> tools;
	// ID like ncdrill, 1-based.
	auto get_tool_id = [&](tool_t *p) -> size_t {
		if (p->type != tool_t::drill) {
			// WARNING: Attempted to nc-drill with a mill
			DEBUG("WARNING: Attempted to nc-drill with a mill: " << p->description);
			return 0;
		}

		for (int id=0; id < tools.size(); id++)
			if (tools[id] == p)
				return id+1;
		tools.push_back(p);
		return tools.size();
	};
	for (auto &path: paths)
		get_tool_id(path.tool);

	// Tool list, on file
	for (int id=0; id<tools.size(); id++) {
		// Bungard tools are weird. Tools are in mm, even though rest of file is inches.
		F("T%dC%f ; %s") % (id+1) % (tools[id]->diameter) % tools[id]->description;
	}

	// Header
	F("%%   ; End of header");
	F("M72 ; Inches, again");
	F("G05 ; Drill mode");
	F("G90 ; Absolute coordinates");

	int last_tool_id = -1;
	for (auto &metapath : paths) {
		int tool_id = get_tool_id(metapath.tool);
		if (!tool_id) continue;
		if (last_tool_id != tool_id)
			F("T%d") % tool_id;
		last_tool_id = tool_id;

		auto &points = metapath.path->points;
		auto &tool = *metapath.tool;
		bool backwards = metapath.backwards;
		double depth = 0;

		if (backwards)
			for (auto point = points.rbegin(); point != points.rend(); point++)
				F("X%dY%d") % X(point->x) % Y(point->y);
		else
			for (auto point = points.begin(); point != points.end(); point++)
				F("X%dY%d") % X(point->x) % Y(point->y);
	}
	F("M30 ; End of file");

	return vs;
}

}
