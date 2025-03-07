#ifndef PCB2GODE_H
#define PCB2GODE_H

#include <vector>
#include <string>
#include <opencv2/opencv.hpp>
#include <yaml-cpp/yaml.h>
#include <boost/format.hpp>

// debugging
#include <iostream>
#include <thread>
#define DEBUG(x)  ( std::cerr << x << std::endl )
#define DEBUGL(x) ( std::cerr << x << std::flush )
#define NUMBER_TO_STR(d) str(boost::format("%06f") % (d) )

namespace pcb2gcode {

extern int DebugImageSave_counter;
inline void DebugImageSave(std::string name, cv::Mat image) {
	int n = DebugImageSave_counter++;
	std::string N = str(boost::format("p2g-debug-out/%04d") % n);
	cv::imwrite(N + "-" + name + ".png", image);
}

struct error : public std::string {
	template<typename ...Args>
	error(Args... args) : std::string(args... ){}
};

inline std::string getRealPath(std::string path) {
	if (path.find_first_of('/') != std::string::npos)
		throw error("Unsafe path: " + path);
	return path;
};
inline std::string dirname(std::string path) {
	return std::string(path, 0, path.find_last_of('/'));
};

// Tool definition
struct tool_t {
	std::string description;
	enum type_e {
		undefined=0, mill, drill
	} type{undefined};
	double diameter{0.12}; // mm
	double angle{0}; // degrees, V-shape, 0=parallel
	double speed{10000}; // rpm
	double feed{120}; // mm/s
	double plunge{120}; // mm/s
	double infeed{0}; // mm/pass
	double depth{0}; // mm
	bool climb{false}; // Milling direction
	size_t runs{3}; // How many times to evaluate this tool before trying next one
	double overlap{0.5}; // Ratio of the path that gets cut twice on parallel cuts
	std::string predrill; // Use other tool to drill before this plunges
};

// Map of tools: Name -> object
typedef std::map< std::string, tool_t > tool_map_t;

// A single point
typedef cv::Point point_t;

// A path, no metadatametadata
typedef std::vector<point_t> points_t;

// A path, with metadata
struct path_t  {
	bool reversible;
	int priority;
	points_t points;

	path_t(size_t size=0) {
		reversible = true;
		priority = 0;
		points.resize(size);
	}
	path_t(const points_t &other) : points(other) {
		reversible = true;
		priority = 0;
	}
	path_t(const points_t &&other) : points(other) {
		reversible = true;
		priority = 0;
	}

	path_t metacopy() const {
		path_t path;
		path.reversible = reversible;
		path.priority = priority;
		return path;
	}
};

// A list of paths
typedef std::vector< path_t > paths_t;

// Paths for a job: Tool -> paths
typedef std::map<std::string, paths_t> tool_paths_t;

// Paths for the entire code: Job -> {Tool -> paths}
typedef std::map<std::string, tool_paths_t> job_tool_paths_t;

inline void iterate_job_tool_paths(
	job_tool_paths_t &jtp, 
	std::function<void(std::string, std::string, paths_t &p)> f
) {
	// Iterate over all job/tool pairs
	for (auto &j : jtp) {
		std::string job = j.first;
		for (auto &jt: j.second) {
			std::string tool = jt.first;
			f(job, tool, jt.second);
		}
	}
}

// Keeps extra info required during and after sorting
struct metapath_t {
	int priority;
	bool reversible;
	bool backwards;
	cv::Point2d entry, exit;
	cv::Point2d rentry, rexit;

	tool_t *tool;
	path_t *path;

	metapath_t() : priority(0), backwards(false), tool(0), path(0) {};
	void reverse() {
		if (!reversible) return;
		swap(entry, rentry);
		swap(exit, rexit);
		backwards = !backwards;
	}
};
typedef std::vector< metapath_t > metapaths_t;

//
struct context_t {
	std::string fileName;
	double ppmm;
	cv::Rect2d bounds;

	tool_map_t tools;
	std::vector<std::string> tool_predrill_order;

	std::map<std::string, cv::Mat> inputs;
	job_tool_paths_t job_tool_paths;

	YAML::Node yaml;
};

// Get the bounding box from a gcode file
cv::Rect2d bounding_rectangle(std::string edgeFileName);

// Output: One list of paths per tool
bool job_isolate(context_t &context, std::string jobName);
bool job_paint(context_t &context, std::string jobName);
bool job_voronoi(context_t &context, std::string jobName);
bool job_drill(context_t &context, std::string jobName);
bool job_cutout(context_t &context, std::string jobName);
bool job_alignment_holes(context_t &context, std::string jobName);
bool job_raw_import(context_t &context, std::string jobName);

bool load_tools(context_t &context);
cv::Rect2d gerber_bounds(std::string edgeFileName);
bool do_inputs(context_t &context);
cv::Mat job_input_layer(context_t context, std::string jobName, std::string layerName, cv::Mat layer=cv::Mat());
bool do_jobs(context_t &context);
bool do_outputs(context_t &context);
path_t simplify_path(const path_t &src, double tol=1.0);

typedef std::vector<std::string> (*formatter_t)(context_t &context, const metapaths_t &paths, bool mirror);
std::vector<std::string> out_gcode(context_t &context, const metapaths_t &paths, bool mirror);
std::vector<std::string> out_hpgl(context_t &context, const metapaths_t &paths, bool mirror);
std::vector<std::string> out_ncdrill(context_t &context, const metapaths_t &paths, bool mirror);
std::vector<std::string> out_preview(context_t &context, const metapaths_t &paths, bool mirror);

inline cv::Scalar tool_color(tool_t t) {
	if (t.diameter > 0.5)
		return cv::Scalar(0,255,0);
	if (t.diameter < 0.25)
		return cv::Scalar(0,0,255);
	if (t.runs > 1)
		return cv::Scalar(255,128,128);
	return cv::Scalar(255,0,0);
}

inline paths_t findContours(cv::Mat source, point_t offset={0,0}) {
	std::vector<points_t> vvp;
	cv::findContours(source, vvp, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE, offset);
	paths_t paths;
	for (auto &&points : vvp)
		paths.emplace_back(points);
	return paths;
}

template <class... Args>
inline void drawContours(cv::Mat &image, const paths_t &paths, int which, cv::Scalar color, int thickness) {
	std::vector<points_t> vvp;
	for (auto &path : paths)
		vvp.emplace_back(path.points);
	drawContours(image, vvp, which, color, thickness);
}


}


#endif // PCB2GODE_H
