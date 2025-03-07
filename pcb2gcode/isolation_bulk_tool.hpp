#include <pcb2gcode.hpp>

namespace pcb2gcode {

// mLayer is true where is MUST NOT mill.
// mBadCopper is true where it SHOULD mill.
static paths_t bulk_tool_iteration(const tool_t &primary, const tool_t &bulk, cv::Mat mLayer, cv::Mat mBadCopper, double ppmm) {
//    DEBUG("Calculating bulk tool isolation paths (d=" << bulk.diameter << "mm)...");

    // Bulk tooling needs large erode and dilate, which are slow.
    // Resize the image down instead, so that tool is about 25 pixels.
    int refsz = 25;
    double scale = fmin(refsz / (bulk.diameter * ppmm), 1.);

    cv::Mat mLayerS, mBadCopperS;
    cv::resize(mLayer, mLayerS, {0,0}, scale, scale, cv::INTER_AREA);
    cv::resize(mBadCopper, mBadCopperS, {0,0}, scale, scale, cv::INTER_AREA);

    cv::threshold(mLayerS, mLayerS, 1, 255, cv::THRESH_BINARY);
    cv::threshold(mBadCopperS, mBadCopperS, 254, 255, cv::THRESH_BINARY);

    // 50% overlap means tool rides the leftover copper contours.
    // Other values need correction.
    if (bulk.overlap > 0.5) {
        cv::dilate(mBadCopperS, mBadCopperS, cv::getStructuringElement(
               cv::MORPH_ELLIPSE,
               cv::Size2d{
                   bulk.diameter * (bulk.overlap-0.5) * ppmm * scale,
                   bulk.diameter * (bulk.overlap-0.5) * ppmm * scale
               }
            )
        );
    }
    if (bulk.overlap < 0.5) {
        cv::erode(mBadCopperS, mBadCopperS, cv::getStructuringElement(
               cv::MORPH_ELLIPSE,
               cv::Size2d{
                   bulk.diameter * (0.5-bulk.overlap) * ppmm * scale,
                   bulk.diameter * (0.5-bulk.overlap) * ppmm * scale
               }
            )
        );
    }

    // Bulk tools are not supposed to do surface finish, so give some extra clearance.
    cv::Mat mKeepOut;
    cv::dilate(
        mLayerS, mKeepOut, cv::getStructuringElement(
            cv::MORPH_ELLIPSE,
            cv::Size2d{
                (bulk.diameter + primary.diameter) * ppmm * scale,
                (bulk.diameter + primary.diameter) * ppmm * scale
            }
        )
    );

    mLayerS.release();

    // Traces are easy to find, just remove the keepout from the remaining copper.
    cv::Mat mTemp = mBadCopperS - mKeepOut;

    mBadCopperS.release();
    mKeepOut.release();

//    DebugImageSave("bulk_isolation_source", mTemp);

    // Get the contours as tool paths
    paths_t pl = findContours(mTemp);
    //cv::findContours(mTemp, pl, cv::RETR_LIST, cv::CHAIN_APPROX_NONE);

    for (auto &path : pl) {
        path = simplify_path(path);
        for (auto &point : path.points) {
            point.x /= scale;
            point.y /= scale;
        }
    }

    return pl;
}

}
