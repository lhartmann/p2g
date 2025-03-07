#include <pcb2gcode.hpp>
#include <pcb2gcode/removable_copper_area.hpp>
#include <pcb2gcode/isolation_primary_tool.hpp>
#include <pcb2gcode/isolation_bulk_tool.hpp>
#include <pcb2gcode/isolation_detail_tool.hpp>
#include <opencv2/imgproc.hpp>

namespace pcb2gcode {

bool job_paint(context_t &context, std::string jobName) {
	DEBUG("Starting paint job " << jobName << "...");
	tool_paths_t tool_paths;

	//
	YAML::Node jobInputs = context.yaml["jobs"][jobName]["inputs"];
	if (!jobInputs.IsDefined())
		throw error("Missing inputs for job " + jobName + ".");
	DEBUG("  Loading layers...");
	cv::Mat mArea = job_input_layer(context, jobName, "area");

	if (mArea.empty()) {
		DEBUG("  Missing area layers on job " + jobName + ". Skip.");
		return false;
	}

	// Isolation paths:
	tool_t *primary_tool=0;
	DEBUG("  Isolation milling...");
	for (auto t : context.yaml["jobs"][jobName]["tools"]) {
		std::string toolName = t.as<std::string>("");
		if (!context.tools.count(toolName))
			throw error("Undefined tool " + toolName + " requested on job " + jobName + ".");

		auto &tool = context.tools[toolName];
		int d = tool.diameter * context.ppmm + 0.5;
		
		cv::erode(mArea, mArea, cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(d,d)));
		paths_t pl = findContours(mArea);
		//cv::findContours(mArea,pl,cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

		std::move(pl.begin(), pl.end(), std::back_inserter(tool_paths[toolName]));
	}
	
	// Draw all paths:
	if (true) {
		cv::Mat mAllPaths;
		cv::cvtColor(mArea, mAllPaths, cv::COLOR_GRAY2BGR);
		for (auto i : tool_paths) {
			auto tool  = context.tools[i.first];
			auto tool_paths = i.second;

			drawContours(mAllPaths, tool_paths, -1, tool_color(tool), 2);
		}
		DebugImageSave("paint_paths_thin", mAllPaths);
	}
	if (true) {
		cv::Mat mAllPaths;
		cv::cvtColor(mArea, mAllPaths, cv::COLOR_GRAY2BGR);
		for (auto i : tool_paths) {
			auto tool  = context.tools[i.first];
			auto tool_paths = i.second;

			drawContours(mAllPaths, tool_paths, -1,
				tool_color(tool), int(tool.diameter*context.ppmm));
		}
		DebugImageSave("paint_paths", mAllPaths);
	}

	// Algorithms all give opencv polygons, but paths need closing.
	for (auto &tp : tool_paths) {
		for (auto &path : tp.second) {
			auto &points = path.points;
			if (points.size() > 1)
				points.push_back(points.front());
		}
	}

	context.job_tool_paths[jobName] = tool_paths;
	return true;
}

}
