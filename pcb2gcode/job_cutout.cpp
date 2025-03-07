#include <pcb2gcode.hpp>
#include <pcb2gcode/mask_paths.hpp>
#include <opencv2/ximgproc.hpp>

namespace pcb2gcode {

bool job_cutout(context_t &context, std::string jobName) {
    DEBUG("Starting cutout job " << jobName << "...");
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

    if (tool.type != tool_t::mill)
        throw error("Tool should be a mill on cutout job " + jobName + ".");

    // Paths to be returned
    paths_t paths;

    // The challenging thing here is to remove the thickness of the outline.
    DEBUG("  Pre-processing outline...");
    DebugImageSave("outline-original", mOutline);
    cv::ximgproc::thinning(mOutline, mOutline);
    DebugImageSave("outline-thinned", mOutline);

    std::vector<cv::Mat> outlines;
    while (cv::countNonZero(mOutline)) {
        // mOutline begins with white contours on black background
        // a gets thin black contours on white background
        // b should be a solid white fill, but may still contain black contours inside
        cv::Mat a=~mOutline, b;
        cv::floodFill(a, cv::Point(0,0), 0);

        // If b is completely black, then there were no more contours

        // OR with the white contours in mOutline will fill the gaps on b.
        b = a | mOutline;
        outlines.push_back(b);

        //
        mOutline = ~a;
        cv::floodFill(mOutline, cv::Point(0,0), 0);
    }

    double d = tool.diameter * context.ppmm;

    cv::Mat mTabs = job_input_layer(context, jobName, "tabs");
    if (!mTabs.empty()) {
        DEBUG("  Calculating tabs exclusion areas...");
        cv::dilate(mTabs, mTabs, cv::getStructuringElement(cv::MORPH_ELLIPSE,
            cv::Size(d, d)
        ));
        DebugImageSave("cutout-tabs", mTabs);
    }

    int priority = 0;
    while (outlines.size()) {
        cv::Mat &m = outlines.back();
        DebugImageSave("cutout-contour", m);

        if (outlines.size() % 2) {
            DEBUG("  Cutting outside...");
            cv::dilate(m,m,cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(d,d)));
            DebugImageSave("cutout-contour-outside", m);
        } else {
            DEBUG("  Cutting inside...");
            cv::erode(m,m,cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(d,d)));
            DebugImageSave("cutout-contour-inside", m);
        }

        paths_t perimeters = findContours(m);
        //cv::findContours(m, perimeters, cv::RETR_LIST, cv::CHAIN_APPROX_NONE);

        // Close paths
        for (auto &path : perimeters) {
            points_t &points = path.points;
            if (points.size() > 1)
                points.push_back(points.front());
        }

        // Create tabs
        if (!mTabs.empty()) {
            DEBUG("    Creating tabs, " << perimeters.size() << " paths before...");
            perimeters = mask_paths(perimeters, [&mTabs](const point_t &pt) -> bool {
                    if (pt.x < 1 || pt.x > mTabs.cols-2) return false;
                    if (pt.y < 1 || pt.y > mTabs.rows-2) return false;
                    return mTabs.at<uint8_t>(pt.y,pt.x) < 128;
                }
            );
            DEBUG("    " << perimeters.size() << " paths after.");
        }

        // Simplify
        for (auto &path : perimeters)
            path = simplify_path(path);

        for (auto &path : perimeters)
            path.priority += priority;
        priority++;

        std::move(perimeters.begin(), perimeters.end(), std::back_inserter(paths));

        outlines.pop_back();
    }

    // Draw outlines for debugging
    cv::Mat cutouts;
    cv::threshold(mOriginalOutline, mOriginalOutline, 1, 100, cv::THRESH_BINARY);
    cv::threshold(mOriginalOutline, cutouts, 1, 255, cv::THRESH_BINARY);
    if (!mTabs.empty())
        cutouts += job_input_layer(context, jobName, "tabs") * 0.2;

    drawContours(cutouts, paths, -1, 64, tool.diameter*context.ppmm);
    DebugImageSave("cutout", cutouts);

    context.job_tool_paths[jobName][toolName] = paths;

    return true;
}

}
