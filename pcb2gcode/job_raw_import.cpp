#include <pcb2gcode.hpp>
#include <pcb2gcode/removable_copper_area.hpp>
#include <pcb2gcode/isolation_primary_tool.hpp>
#include <pcb2gcode/isolation_bulk_tool.hpp>
#include <pcb2gcode/isolation_detail_tool.hpp>
#include <opencv2/imgproc.hpp>

#include <fstream>
#include <sstream>

namespace pcb2gcode {
/* RAW XY path importer
 *
 * Path file contains simply ASCII encoded X Y coordinates.
 * Multiple pairs on the same text line correspond to points over a path.
 * Separate text lines corresponde to separate paths.
 * Anything after # is a comment
 *
 * E.G. to make a square and a triangle on top (little house):
 *
 * 0 0 0 1 1 1 1 0
 * 1 0 1 1 0.5 1.5
 *
 * On cocnfig.p2g use

jobs:
	job_id:
		type: raw_import
		paths:
			- tool_id: input_id
			- tool_id: input_id

*/

bool job_raw_import(context_t &context, std::string jobName) {
	DEBUG("Starting raw_import job " << jobName << "...");
	tool_paths_t tool_paths;

	// Raw coodinates are expected to be in mm, but internally we work in pixels.
	// This will be used to convert points to internal representation
	auto make_point = [&context](double x, double y) {
		return point_t(
			(+x - context.bounds.x) * context.ppmm + 0.5,
			(-y + context.bounds.y + context.bounds.height) * context.ppmm + 0.5
		);
	};

	// Paths:
	DEBUG("  Importing...");
	// Paths are an array
	for (const auto &pathSpec : context.yaml["jobs"][jobName]["paths"]) {
		// Each pathspec is "tool_id: input_id", but we don't know the tool_id beforehand.
		// Using the begin() we can get to it as a pair.
		std::string toolName = pathSpec.begin()->first.as<std::string>("");
		std::string input_id = pathSpec.begin()->second.as<std::string>("");

		if (!context.tools.count(toolName))
			throw error("Undefined tool " + toolName + " requested on job " + jobName + ".");

		// Figure the actual file name
		std::string filename = context.yaml["inputs"][input_id].as<std::string>(input_id);
		if (filename.empty())
			throw error("Undefined input " + input_id + " requested on job " + jobName + ".");

		// Open file for reading
		std::ifstream in(filename.c_str());
		if (!in) {
			DEBUG("  Could not open Input file "+filename+" for tool "+toolName+". Skip.");
			continue;
		}

		// Read one line at a time
		std::string line;
		while(getline(in,line)) {
			// Remove comments
//			line = std::string(line, line.find_first_of('#'));
			std::stringstream ss(line);

			path_t path;
			while(true) {
				double x, y;
				ss >> x >> y;
				if (!ss) break;
				path.points.push_back(make_point(x,y));
			}

			if (path.points.size())
				tool_paths[toolName].push_back(path);
		}
	}

	context.job_tool_paths[jobName] = tool_paths;
	return true;
}

}
