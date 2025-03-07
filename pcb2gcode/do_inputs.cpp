#include <pcb2gcode.hpp>
#include <boost/process.hpp>
#include <boost/format.hpp>
#include <unistd.h>

namespace pcb2gcode {

// This allows direct loading of input image files. Sizes must match or hell breaks loose.
cv::Mat load_image(std::string infile) {
    cv::Mat image = cv::imread(infile);
    if (!image.data)
        throw error("Failed to load image.");
    cv::cvtColor(image, image, cv::COLOR_BGR2GRAY);
    cv::threshold(image, image, 127, 255, cv::THRESH_BINARY);
    return image;
}

cv::Mat load_gerber(std::string infile, cv::Rect2d bounds, double ppmm) {
    if (access(infile.c_str(), R_OK) == -1)
        return {};

    namespace bp = boost::process;
    std::string tmpfile = infile;
    for (auto &c : tmpfile)
        if (!isalnum(c)) c = '_';
    bool keep_temps = access("./pcb2gcode-debug-out/.", W_OK) != -1;
    if (!keep_temps)
        tmpfile = "/tmp/p2g-" + infile + ".png";    
    else
        tmpfile = "./p2g-debug-out/pcb2gcode-" + infile + ".png";

    auto gerbv = bp::search_path("gerbv");
    if (gerbv.empty())
        throw error("gerbv not found.");

    int ret = bp::system(gerbv, boost::process::env["LC_ALL"]="C",
        "-D", str(boost::format("%f") % int(25.4*ppmm)),
        "-x",  "png", "-b", "#000000", "-f", "#FFFFFFFF",
        str(boost::format("--origin=%06fx%06f") % (bounds.x / 25.4) % (bounds.y / 25.4)),
        str(boost::format("--window_inch=%06fx%06f") % (bounds.width / 25.4) % (bounds.height / 25.4)),
        "-o", tmpfile , infile,
        bp::std_out > "/dev/null", bp::std_err > "/dev/null"
    );
    if (ret)
        throw error("gerbv failed.");

    cv::Mat image = load_image(tmpfile);

    if (!keep_temps)
        unlink(tmpfile.c_str());

    return image;
}

cv::Mat load_input(std::string infile, cv::Rect2d bounds, double ppmm) {
    for (const auto ext : { ".png" , ".jpg", ".tiff", ".pgm", ".pbm" })
        if (infile.ends_with(ext))
            return load_image(infile);
    return load_gerber(infile, bounds, ppmm);
}

bool do_inputs(context_t &context) {
    context.ppmm = context.yaml["ppmm"].as<double>(100);

    DEBUG("Load inputs at " << context.ppmm << " pixels/mm...");
    DEBUG("  Identifying boundaries of gerber-space...");
    double left   = +INFINITY;
    double right  = -INFINITY;
    double bottom = +INFINITY;
    double top    = -INFINITY;
    double margin = context.yaml["margin"].as<double>(4);

    const auto &bounds  = context.yaml["bounds"];
    if (bounds.IsDefined()) {
        DEBUG("  Bounding box explicitly set on config file.");
        left   = bounds["left"  ].as<double>(+INFINITY);
        right  = bounds["right" ].as<double>(-INFINITY);
        bottom = bounds["bottom"].as<double>(+INFINITY);
        top    = bounds["top"   ].as<double>(-INFINITY);
    } else for (const auto &inf : context.yaml["inputs"]) {
        try {
            auto b = gerber_bounds(inf.second.as<std::string>());

            left   = fmin(left,   b.x           );
            right  = fmax(right,  b.x + b.width );
            bottom = fmin(bottom, b.y           );
            top    = fmax(top,    b.y + b.height);
            DEBUG("    " + inf.second.as<std::string>() + ": ok");
        } catch (error e) {
            DEBUG("    " + inf.second.as<std::string>() + ": " + e);
            // Nevermind, usually tried to read a NCDrill as GERBER.
        }
    }

    if (isinf(left) || isinf(right) || isinf(bottom) || isinf(top))
        throw error("Failed to identify boundaries.");

    DEBUG("    X = ["<<left<<", "<<right<<"]mm, Y = ["<<bottom<<", "<<top<<"]mm");
    DEBUG("    width = " << (right-left) << "mm, height = " << (top-bottom) << "mm");
    context.bounds = cv::Rect2d(left-margin, bottom-margin, right-left+2*margin, top-bottom+2*margin);

    DEBUG("  Loading bitmaps...");
    for (const auto &inf : context.yaml["inputs"]) {
        std::string layer = inf.first.as<std::string>();
        std::string file  = inf.second.as<std::string>();
        DEBUG("    Loading " << layer << " from " << file << "...");

        file = getRealPath(file);

        try {
            cv::Mat m = load_gerber(file, context.bounds, context.ppmm);
            context.inputs[layer] = m;
        } catch (error e) {
            DEBUG("      ERROR: " << e);

            // Try failing gracefully by leaving an empty Mat in place.
            context.inputs[layer] = cv::Mat();
        }
    }

    // Senity check: requires at least one layer
    if (!context.inputs.size()) {
        DEBUG("  ERROR: No layers loaded.");
        return false;
    }

    // Sanity check: All sizes must match
    bool sane = true;
    auto first_layer = context.inputs.begin();
    if (false) for (auto layer : context.inputs) {
        if (first_layer->second.size() != layer.second.size()) {
            DEBUG("  ERROR: Image size mismatch between layers " << first_layer->first << "(" << first_layer->second.size() << ") and " << layer.first << " (" << layer.second.size() << ").");
            sane = false;
        }
    }

    // HACK: zero lower left
    context.bounds = cv::Rect2d(0, bottom-top, right-left, top-bottom);

    return sane;
}

}
