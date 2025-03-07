#include <pcb2gcode.hpp>

namespace pcb2gcode {
    
static cv::Mat handle_fill_odd(cv::Mat mOutline) {
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
    return board - holes + mOutline;
}

static cv::Mat handle_fill_solid(cv::Mat mOutline) {
    cv::Mat tmp   = mOutline.clone();
    cv::floodFill(tmp, cv::Point(0,0), 255);
    
    return ~(mOutline ^ tmp);
}

static void handle_dilate(cv::Mat &layer, double d, double ppmm) {
    int i = std::fabs(d) * ppmm + 0.5;
    
    if (!i) return;
    auto se = cv::getStructuringElement(cv::MORPH_ELLIPSE,{i,i});
    
    if (d>0)
        cv::dilate(layer, layer, se);
    else
        cv::erode(layer, layer, se);
}

static void merge_or     (cv::Mat &a, const cv::Mat &b) { a |=  b; }
static void merge_and    (cv::Mat &a, const cv::Mat &b) { a &=  b; }
static void merge_and_not(cv::Mat &a, const cv::Mat &b) { a &= ~b; }
static void merge_xor    (cv::Mat &a, const cv::Mat &b) { a ^=  b; }

std::pair<const char *, std::function<void(cv::Mat&, const cv::Mat&)>> merger[] = {
    { "union",        merge_or },
    { "or",           merge_or },
    { "add",          merge_or },
    { "intersection", merge_and },
    { "intersect",    merge_and },
    { "product",      merge_and },
    { "and",          merge_and },
    { "difference",   merge_and_not },
    { "subtraction",  merge_and_not },
    { "subtract",     merge_and_not },
    { "sub",          merge_and_not },
    { "zor",          merge_xor },    
};

static bool handle_merge(cv::Mat &a, const cv::Mat &b, std::string mode) {
    if (a.empty()) {
        // Starts with a blank image
        a = b.clone();
        a -= b;
    }
    
    for (auto m : merger) {
        if (mode != m.first)
            continue;
        
        m.second(a,b);
        return true;
    }
    
    return false;
}

/* 
job:
    JOB_NAME:
        inputs:
            - LAYER_NAME: INPUT_NAME
              fill: none              # none, odd, solid
              dilate: 0
              mode: union             # union, intersection, subtraction, 
              inverted: false
            - LAYER_NAME: INPUT_NAME
            - LAYER_NAME: INPUT_NAME
*/
cv::Mat job_input_layer(context_t context, std::string jobName, std::string layerName, cv::Mat layer) {
    YAML::Node jobInputs = context.yaml["jobs"][jobName]["inputs"];
    if (!jobInputs.IsDefined()) throw error("Missing inputs on job " + jobName + ".");
    
    for (auto inf : jobInputs) {
        std::string inputName = inf[layerName].as<std::string>("");
        if (context.inputs.count(inputName)) {
            cv::Mat input = context.inputs[inputName].clone();
            if (input.empty()) continue;
            
            // Handle Fill
            std::string fill = inf["fill"].as<std::string>("none");
            if (fill == "solid")
                input = handle_fill_solid(input);
            else if (fill == "odd")
                input = handle_fill_odd(input);
            else if (fill != "none")
                throw error("invalid fill on job " + jobName + ", layer " + layerName + ", input " + inputName + ": " + fill);
            
            // Handle dilate, or erode if < 0 (default: 0)
            double d = inf["dilate"].as<double>(0.);
            handle_dilate(input, d, context.ppmm);
            
            // Handle invert (default: false)
            if (inf["invert"].as<bool>(false))
                input = ~input;                

            // Handle boolean modes (defaults: union)
            std::string mode = inf["mode"].as<std::string>("union");
            if (!handle_merge(layer, input, mode))
                throw error("invalid mode on job " + jobName + ", layer " + layerName + ", input " + inputName + ": " + mode);
        }
    }

    return layer;
}

}
