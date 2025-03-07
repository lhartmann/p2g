#include <pcb2gcode.hpp>

namespace pcb2gcode {

bool do_jobs(context_t &context) {
    for (const auto &inf : context.yaml["jobs"]) {
        if (!inf.second["enabled"].as<bool>(true))
            continue;

        std::string jobName = inf.first.as<std::string>("");
        std::string jobType = inf.second["type"].as<std::string>("");

        if (!inf.second["enabled"].as<bool>(true))
            continue;

        if (jobType == "isolate")
            job_isolate(context, jobName);
        if (jobType == "paint")
            job_paint(context, jobName);
        if (jobType == "voronoi")
            job_voronoi(context, jobName);
        if (jobType == "drill")
            job_drill(context, jobName);
        if (jobType == "cutout")
            job_cutout(context, jobName);
        if (jobType == "alignment_holes")
            job_alignment_holes(context, jobName);
        if (jobType == "raw_import")
            job_raw_import(context, jobName);

        // Predrilling
        auto &tool_paths = context.job_tool_paths[jobName];
        for (auto &toolName : context.tool_predrill_order) {
            if (!tool_paths.count(toolName))
                continue;

            auto &predrill = context.tools[toolName].predrill;

            for (auto &path : tool_paths[toolName]) {
                path_t drill;
                drill.points.push_back(path.points.front());

                tool_paths[predrill].push_back(drill);

                path.reversible = false;
            }
        }
    }

    return true;
}

}
