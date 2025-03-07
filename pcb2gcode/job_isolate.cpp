#include <pcb2gcode.hpp>
#include <pcb2gcode/removable_copper_area.hpp>
#include <pcb2gcode/isolation_primary_tool.hpp>
#include <pcb2gcode/isolation_bulk_tool.hpp>
#include <pcb2gcode/isolation_detail_tool.hpp>
#include <opencv2/imgproc.hpp>

namespace pcb2gcode {

bool job_isolate(context_t &context, std::string jobName) {
	DEBUG("Starting isolation job " << jobName << "...");
	tool_paths_t tool_paths;

	//
	DEBUG("  Loading layers...");
	cv::Mat mCopper = job_input_layer(context, jobName, "copper");
	cv::Mat mEdge   = job_input_layer(context, jobName, "outline");
	cv::Mat mDrill  = job_input_layer(context, jobName, "drill");

	if (mCopper.empty()) {
		DEBUG("  Missing copper layers for job " + jobName + ". Skip.");
		return false;
	}
	if (mEdge.empty()) {
		DEBUG(  "Missing outline layers for job " + jobName + ". Skip.");
		return false;
	}

	// Drill layer is optional, removes from copper.
	if (!mDrill.empty())
		mCopper -= mDrill;
	mDrill.release();

	// Calculate where to remove copper.
	// White means copper to be removed, black means let it be.
	DEBUG("  Calculating required copper removal area...");
	cv::Mat mRest = removable_copper_area(mEdge, context.ppmm);
	mRest -= mCopper;
	DebugImageSave("copper", mRest);

	// Isolation paths:
	tool_t *primary_tool=0;
	DEBUG("  Isolation milling...");
	for (auto t : context.yaml["jobs"][jobName]["tools"]) {
		std::string toolName = t.as<std::string>("");
		if (!context.tools.count(toolName))
			throw error("Undefined tool " + toolName + " requested on job " + jobName + ".");

		auto &tool = context.tools[toolName];

		if (!primary_tool) {
			DEBUGL("	Primary tool " + toolName + "... ");
			primary_tool = &tool;
			paths_t pl = primary_tool_iteration(*primary_tool, mCopper, context.ppmm);
			DEBUG(pl.size() << " paths.");

			drawContours(mRest, pl, -1, 0, int(primary_tool->diameter*context.ppmm+0.5));
			DebugImageSave("copper", mRest);

			std::move(pl.begin(), pl.end(), std::back_inserter(tool_paths[toolName]));
		} else {
			paths_t pl;
			int backoffs = 3;
			if (tool.diameter >= primary_tool->diameter)
				DEBUGL("	Bulk tool " << toolName << "... ");
			else
				DEBUGL("	Detail tool " << toolName << "... ");
			for (size_t run=0; run < tool.runs; ++run) {
				paths_t tpl;
				if (tool.diameter >= primary_tool->diameter) {
					tpl = bulk_tool_iteration(*primary_tool, tool, mCopper, mRest, context.ppmm);

					// On final bulk pass try a larger overlap. This helps cleaning small dots.
					if (!tpl.size() && backoffs) {
						backoffs--;
						double overlap_save = (tool.overlap+3) / 4;
						std::swap(tool.overlap, overlap_save);
						tpl = bulk_tool_iteration(*primary_tool, tool, mCopper, mRest, context.ppmm);
						std::swap(tool.overlap, overlap_save);
					}
				} else {
					tpl = detail_tool_iteration(*primary_tool, tool, mCopper, mRest, context.ppmm, run==0);
				}
				DEBUGL("+" << tpl.size() << " ");

				if (!tpl.size())
					break;

				drawContours(mRest, tpl, -1, 0, int(tool.diameter*context.ppmm+0.5));

				DebugImageSave(("copper_" + toolName + "_" + NUMBER_TO_STR(run)).c_str(), mRest);

				pl.insert(pl.end(), tpl.begin(), tpl.end());
			}
			DEBUG("= " << pl.size() << " paths.");
			std::move(pl.begin(), pl.end(), std::back_inserter(tool_paths[toolName]));
		}
	}
	
	// Draw all paths:
	if (true) {
		cv::Mat mAllPaths;
		cv::cvtColor(mCopper, mAllPaths, cv::COLOR_GRAY2BGR);
		for (auto i : tool_paths) {
			auto tool  = context.tools[i.first];
			auto tool_paths = i.second;

			drawContours(mAllPaths, tool_paths, -1, tool_color(tool), 2);
		}
		DebugImageSave("paths_thin", mAllPaths);
	}
	if (true) {
		cv::Mat mAllPaths;
		cv::cvtColor(mCopper, mAllPaths, cv::COLOR_GRAY2BGR);
		for (auto i : tool_paths) {
			auto tool  = context.tools[i.first];
			auto tool_paths = i.second;

			drawContours(mAllPaths, tool_paths, -1,
				tool_color(tool), int(tool.diameter*context.ppmm));
		}
		DebugImageSave("paths_true_width", mAllPaths);
	}
	if (true) {
		cv::Mat mMilled = mCopper & ~mCopper;
		cv::cvtColor(mMilled, mMilled, cv::COLOR_GRAY2BGR);
		for (auto i : tool_paths) {
			auto tool  = context.tools[i.first];
			auto tool_paths = i.second;

			drawContours(mMilled, tool_paths, -1, {255,255,255}, int(tool.diameter*context.ppmm));
		}
		DebugImageSave("milled", mMilled);
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
