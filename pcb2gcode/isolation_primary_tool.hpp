#include <pcb2gcode.hpp>

namespace pcb2gcode {

// mLayer is true where it MUST NOT mill.
inline paths_t primary_tool_iteration(const tool_t &primary, cv::Mat mLayer, double ppmm) {
    // Erode the remaining copper by half the tool diameter
    cv::Mat mTemp;
    cv::dilate(
        mLayer, mTemp,
        cv::getStructuringElement(
            cv::MORPH_ELLIPSE,
            cv::Size2d{primary.diameter * ppmm, primary.diameter * ppmm}
        )
    );

    // Remove stray pixels
    if (true) {
        cv::Mat mTemp2;
        cv::dilate(mTemp, mTemp2, cv::getStructuringElement(cv::MORPH_RECT,{3,3}));
        cv::erode(mTemp2, mTemp2, cv::getStructuringElement(cv::MORPH_RECT,{3,3}));
        mTemp |= mTemp2;
    }

    // Get the contours as tool paths
    paths_t pl = findContours(mTemp);
    // cv::findContours(mTemp, pl, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

    return pl;
}

}
