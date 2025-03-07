#include <pcb2gcode.hpp>
#include <pcb2gcode/isolation_voronoi_find_edges.hpp>
#include <pcb2gcode/isolation_voronoi_connect_paths.hpp>
#include <pcb2gcode/mask_paths.hpp>
#include <opencv2/ximgproc.hpp>

namespace pcb2gcode {

static unsigned long count_points(const paths_t &vp) {
    unsigned long count = 0;
    for (auto &p : vp)
        count += p.points.size();
    return count;
}

bool job_voronoi(context_t &context, std::string jobName) {
    DEBUG("Starting voronoi isolation job " << jobName << "...");
    tool_paths_t tool_paths;
    struct toollist_item_t {
        int dist_min, dist_max;
        std::string name;
    };
    std::vector<toollist_item_t> tools;

    for (auto jobtool : context.yaml["jobs"][jobName]["tools"]) {
        std::string toolName = jobtool.as<std::string>();
        if (!context.tools.count(toolName))
            throw error("Undefined tool " + toolName + " requested on job " + jobName + ".");

        toollist_item_t tool;
        tool.name = toolName;
        tool.dist_min = context.tools[toolName].diameter*context.ppmm/2;
        tool.dist_max = std::numeric_limits<int>::max();

        if (tools.size())
            tools.back().dist_max = tool.dist_min * 1.05;
        else
            tool.dist_min = 0;

        tools.push_back(tool);
    }

    if (tools.empty())
        throw error("Missing tools list for job " + jobName + ".");

    //
    YAML::Node jobInputs = context.yaml["jobs"][jobName]["inputs"];
    if (!jobInputs.IsDefined())
        throw error("Missing inputs for job " + jobName + ".");
    DEBUG("  Loading layers...");
    cv::Mat mCopper  = job_input_layer(context, jobName, "copper");
    cv::Mat mOutline = job_input_layer(context, jobName, "outline");

    if (mCopper.empty()) {
        DEBUG("  Missing copper layers on job " + jobName + ". Skip.");
        return false;
    }
    if (mOutline.empty()) {
        DEBUG("  Missing outline layers on job " + jobName + ". Skip.");
        return false;
    }

    DebugImageSave("voronoi-copper-source", mCopper);

    DEBUG("  Building voronoi domains and distance transform...");
    cv::Mat distance, domains;
    mCopper = ~mCopper;
    cv::distanceTransform(mCopper, distance, domains, cv::DIST_L2, 5);

    DebugImageSave("voronoi-distance", distance);

    size_t before, after;

    DEBUG("  Extracting paths... ");
    paths_t paths = isolation_voronoi_find_edges(domains);
    DEBUG("    " << paths.size() << " path fragments found.");

    domains.release();

    DEBUG("  Joining paths... ");
    paths = connect_paths(paths);
    DEBUG("    " << paths.size() << " paths.");

    // Paint paths
    if (true) {
        cv::Mat last = mCopper / 4 + mOutline / 4;
        point_t old;
        for (const auto &path : paths) {
            auto &points = path.points;
            for (size_t  j=1; j<points.size(); j++)
                cv::line(last, points[j-1], points[j], 255, 1, cv::LINE_AA);
        }
        DebugImageSave("voronoi-paths-unmasked", last);
    }

    // Voronoi toolpaths will extend beyond board outline, so we need a mask to trim them.
    DEBUG("  Building mask...");
    cv::Mat mask;
    if (true) {
        cv::Mat tmp   = mOutline.clone();
        cv::Mat board = tmp.clone();
        cv::Mat holes = tmp.clone();

//        DebugImageSave("tmp", tmp);
        while(cv::countNonZero(tmp)) {
            cv::floodFill(tmp, cv::Point(0,0), 255);
//            DebugImageSave("tmp-flood", tmp);
            holes |= tmp - board; // Outside, add
//            DebugImageSave("holes", holes);

            cv::floodFill(tmp, cv::Point(0,0), 0);
            cv::floodFill(tmp, cv::Point(0,0), 255);
//            DebugImageSave("tmp-flood", tmp);

            board |= tmp - holes;
//            DebugImageSave("board", board);

            cv::floodFill(tmp, cv::Point(0,0), 0);
        }
        mask = board - holes + mOutline;
    }

    // Extend paths beyond board outline:
    double extend = context.yaml["jobs"][jobName]["extend"].as<double>(0);
    if (extend) {
        auto se = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(fabs(extend*context.ppmm), fabs(extend*context.ppmm)));
        if (extend > 0)
            cv::dilate(mask, mask, se);
        else
            cv::erode(mask, mask, se);
    }
    
    // Apply additional masking as selected specified by user
    if (true) {
        cv::Mat usermask = job_input_layer(context, jobName, "mask");
        if (!usermask.empty())
            mask &= usermask;
    }
    
    DebugImageSave("voronoi-mask", mask);

    DEBUG("  Masking paths... ");
    before = count_points(paths);
    paths = mask_paths(paths, [&mask](const point_t &pt) -> bool {
            if (pt.x < 1 || pt.x > mask.cols-2) return false;
            if (pt.y < 1 || pt.y > mask.rows-2) return false;
            return mask.at<bool>(pt.y,pt.x);
        }
    );
    after = count_points(paths);;
    DEBUG("    " << (before-after) << " points dropped, " << after << " remain.");

    // Paint paths
    if (true) {
        cv::Mat last = mCopper / 4 + mOutline / 4;
        point_t old;
        for (const path_t &path : paths) {
            const points_t &points = path.points;
            for (size_t  j=1; j<points.size(); j++)
                cv::line(last, points[j-1], points[j], 255, 1, cv::LINE_AA);
        }
        DebugImageSave("voronoi-paths-thin", last);
    }

    for (auto &tool : tools) {
        DEBUG("  Filtering paths for " << tool.name << "... ");
        paths_t masked = mask_paths(paths, [&tool, &distance](const point_t &pt) {
                float d = distance.at<float>(pt.y, pt.x);
                return tool.dist_min <= d && d <= tool.dist_max;
            }
        );
        DEBUG("    Simplifying... ");
        for (auto &p : masked) p = simplify_path(p,10);
        DEBUG("    " << masked.size() << " paths, " << count_points(masked) << " total points.");

        tool_paths[tool.name] = masked;
    }

    if (true) {
        cv::Mat last = mCopper / 4 + mOutline / 4;
        point_t old;
        for (auto &tp : tool_paths) {
            int w = context.tools[tp.first].diameter * context.ppmm;
            for (auto path : tp.second) {
                const points_t &points = path.points;
                for (size_t j=1; j<points.size(); j++)
                    cv::line(last, points[j-1], points[j], 255, w, cv::LINE_AA);
            }
        }
        DebugImageSave("voronoi-paths", last);
    }

    context.job_tool_paths[jobName] = tool_paths;
    return true;
}

}
