#include <pcb2gcode.hpp>
#include <pcb2gcode/mask_paths.hpp>
#include <opencv2/ximgproc.hpp>
#include <algorithm>

namespace pcb2gcode {

bool job_alignment_holes(context_t &context, std::string jobName) {
    DEBUG("Starting alignment_holes job " << jobName << "...");
    tool_paths_t tool_paths;

    //
    YAML::Node jobInputs = context.yaml["jobs"][jobName]["inputs"];
    if (!jobInputs.IsDefined())
        throw error("Missing inputs for job " + jobName + ".");
    DEBUG("  Loading layers...");
    cv::Mat mOutline = job_input_layer(context, jobName, "outline");
    if (mOutline.empty()) {
        DEBUG("  Missing outline layers on job " + jobName + ". Skip.");
        return false;
    }

    cv::Mat mOriginalOutline = mOutline.clone();

    std::string toolName = context.yaml["jobs"][jobName]["tools"][0].as<std::string>();
    tool_t tool = context.tools[toolName];

    if (tool.type != tool_t::drill)
        throw error("Tool should be a drill on alignment_holes job " + jobName + ".");

    // KISS MODE: Just 4 holes in the corners of the bounding box
    paths_t perimeters = findContours(mOutline);
    int minx = std::numeric_limits<int>::max();
    int maxx = std::numeric_limits<int>::min();
    int miny = std::numeric_limits<int>::max();
    int maxy = std::numeric_limits<int>::min();
    for (const auto &perimeter : perimeters) {
        for (const auto &point : perimeter.points) {
            minx = std::min(minx, point.x);
            maxx = std::max(maxx, point.x);
            miny = std::min(miny, point.y);
            maxy = std::max(maxy, point.y);
        }
    }

    // Paths to be returned
    paths_t paths;
    paths.emplace_back();
    paths.back().points.emplace_back(minx, miny);
    paths.emplace_back();
    paths.back().points.emplace_back(minx, maxy);
    paths.emplace_back();
    paths.back().points.emplace_back(maxx, miny);
    paths.emplace_back();
    paths.back().points.emplace_back(maxx, maxy);

    context.job_tool_paths[jobName][toolName] = paths;

    return true;
}

}
