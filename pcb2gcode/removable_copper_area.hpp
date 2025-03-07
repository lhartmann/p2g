#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <pcb2gcode.hpp>

namespace pcb2gcode {

// Return white where stray copper should be removed.
inline cv::Mat removable_copper_area(cv::Mat mEdge, double extra) {
    // mEdge should have the edges as white lines on black background.
    cv::Mat mCopper = mEdge.clone();

    // Request full copper removal inside pcb edges.
    cv::Mat tmp;
    cv::bitwise_not(mCopper, tmp);

    cv::floodFill(tmp, cv::Point{0,0}, 0);
    mCopper = mCopper | tmp;

    // Request copper removal extending beyond the PCB edges
    if (false) {
        cv::dilate(mCopper, mCopper,
            cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size2d{extra, extra})
        );
    } else {
        cv::dilate(mCopper, mCopper,
            cv::getStructuringElement(cv::MORPH_RECT, cv::Size2d{1, extra})
        );
        cv::dilate(mCopper, mCopper,
            cv::getStructuringElement(cv::MORPH_RECT, cv::Size2d{extra, 1})
        );
    }

    return mCopper;
}

}
