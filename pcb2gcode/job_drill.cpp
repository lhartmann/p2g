#include <pcb2gcode.hpp>
#include <pcb2gcode/removable_copper_area.hpp>
#include <pcb2gcode/isolation_primary_tool.hpp>
#include <pcb2gcode/isolation_bulk_tool.hpp>
#include <pcb2gcode/isolation_detail_tool.hpp>

namespace pcb2gcode {

// Requires tool list to be organized:
//   drills small...large, single mill.
bool job_drill(context_t &context, std::string jobName) {
    DEBUG("Starting drill job " << jobName << "...");
    tool_paths_t tool_paths;

    DEBUG("  Loading layers...");
    YAML::Node jobInputs = context.yaml["jobs"][jobName]["inputs"];
    if (!jobInputs.IsDefined())
        throw error("Missing inputs for job " + jobName + ".");

    cv::Mat mDrill  = job_input_layer(context, jobName, "drill");

    if (mDrill.empty()) {
        DEBUG("  Missing drill layers on job " + jobName + ". Skip.");
        return false;
    }

    // Load tool list
    struct tool_aux {
        double maxd;
        std::string name;
        tool_t *tool;
    };

    std::vector<tool_aux> tools;
    for (auto t : context.yaml["jobs"][jobName]["tools"]) {
        std::string toolName = t.as<std::string>("");
        if (!context.tools.count(toolName))
            throw error("Undefined tool " + toolName + " requested on job " + jobName + ".");

        tool_aux aux;
        aux.tool = &context.tools[toolName];
        aux.name = toolName;
        tools.push_back(aux);
    }

    // Sort by diameter
    std::sort(tools.begin(), tools.end(),
        [](const tool_aux &a, const tool_aux &b) {
            if (a.tool->type == b.tool->type)
                return a.tool->diameter < b.tool->diameter;

            return a.tool->type == tool_t::drill;
        }
    );

    DEBUG("  Drilling with " << tools.size() << " tools:");

    // Auxiliary functions:

    // Define maximum hole size captured by each tool.
    for (size_t i=0; i<tools.size()-1; ++i) {
        double d0 = tools[i].tool->diameter;
        double d1 = tools[i+1].tool->diameter;
        if (tools[i+1].tool->type == tool_t::mill)
            tools[i].maxd = std::max(d0,d1);
        else
            tools[i].maxd = d0 + 0.75*(d1-d0);
        DEBUG("    " << tools[i].maxd << "mm or less -> " << tools[i].tool->description << "(" << tools[i].tool->diameter << ")");
    }
    DEBUG("    otherwise -> " << tools.back().tool->description);

    // Get tool index for a hole diameter
    auto toolid = [&tools](double d) {
        size_t i = 0;
        while (i < tools.size()-1 && tools[i].maxd < d) i++;
        return i;
    };

    // Find drilll contours
    paths_t drills = findContours(mDrill);
    //cv::findContours(mDrill, drills, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

    for (auto &hole : drills) {
        auto ellipse = cv::fitEllipse(hole.points);
        double d = (ellipse.size.width + ellipse.size.height)/context.ppmm/2;
        //double aspect_error = std::fabs(double(ellipse.size.width) / ellipse.size.height - 1);

        size_t t = toolid(d);

        if (tools[t].tool->type == tool_t::mill) {
            // Mill-hole, extract region of interest
            auto bounds = cv::boundingRect(hole.points);
            bounds.x--;
            bounds.y--;
            bounds.width += 2;
            bounds.height += 2;
            cv::Mat region = mDrill
                    .rowRange(bounds.y, bounds.y+bounds.height)
                    .colRange(bounds.x, bounds.x+bounds.width)
                    .clone();
//            DebugImageSave("milldrill-ROI", region);

            // Erode by tool diameter/2
            double td = tools[t].tool->diameter * context.ppmm;
            cv::erode(region, region, cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(td,td)));
//            DebugImageSave("milldrill-ROI-eroded", region);

            // Find remaing paths
            paths_t paths_here = findContours(region, bounds.tl());
            //cv::findContours(region, paths_here, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE, bounds.tl());

            // Close paths
            for (auto &path : paths_here) {
                points_t &points = path.points;
                if (points.size() > 1)
                    points.push_back(points.front());
            }

            std::move(paths_here.begin(), paths_here.end(), std::back_inserter(tool_paths[tools[t].name]));
        } else {
            // Simple hole, append as a single-point path.
            path_t path;
            path.points.emplace_back(ellipse.center);
            tool_paths[tools[t].name].push_back(path);
        }
    }

    context.job_tool_paths[jobName] = tool_paths;
    return true;
}

}
