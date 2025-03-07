#include <pcb2gcode.hpp>

namespace pcb2gcode {

// mLayer is true where is MUST NOT mill.
// mBadCopper is true where it SHOULD mill.
static paths_t detail_tool_iteration(const tool_t &/*primary*/, const tool_t &detail, cv::Mat mLayer, cv::Mat mBadCopper, double ppmm, bool firstRun) {
//    DEBUG("Calculating detail tool isolation paths (d=" << detail.diameter << "mm)...");
    auto mTool = [&](double scale) {
        scale *= detail.diameter * ppmm;
        return cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size2d{scale, scale});
    };

    cv::Mat mTemp;

    if (firstRun) {
        // On the first run enlarge the bad copper area a little
        cv::dilate(mBadCopper, mTemp, mTool(1.5));
        mTemp = mTemp - mLayer;
        cv::erode(mTemp, mTemp, mTool(1));
    } else {
        // Detail tools can do surface finish just like primary ones.
        cv::Mat mKeepOut;
        cv::dilate(mLayer, mKeepOut, mTool(1));

        // Traces are easy to find, just remove the keepout from the remaining copper.
        mTemp = mBadCopper - mKeepOut;
    }

    // Remove single pixels
    if (true) {
        cv::Mat mTemp2;
        cv::dilate(mTemp, mTemp2, cv::getStructuringElement(cv::MORPH_RECT,{3,3}));
        cv::erode(mTemp2, mTemp2, cv::getStructuringElement(cv::MORPH_RECT,{3,3}));
        mTemp |= mTemp2;
    }

//    DebugImageSave("detail_isolation_source", mTemp);

    // Get the contours as tool paths
    paths_t pl = findContours(mTemp);
    //cv::findContours(mTemp, pl, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

    return pl;
}

}
