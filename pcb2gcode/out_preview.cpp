#include <pcb2gcode.hpp>
#include <boost/range/adaptor/reversed.hpp>

using namespace std;
namespace pcb2gcode {

//int DebugImageSave_counter=0;

#define fail(s) (throw std::string(s))

std::vector<std::string> out_preview(context_t &context, const metapaths_t &cr_paths, bool mirror) {
	metapaths_t paths = cr_paths; // Preview requires a local copy.
	if (paths.empty())
		return {};
	
	// Sort by depth
	std::sort(paths.begin(), paths.end(),
		[](const metapath_t &a, const metapath_t &b) {
			return a.tool->depth < b.tool->depth;
		}
	);

	std::vector<std::string> r;
	uint32_t height = context.bounds.height * context.ppmm;
	uint32_t width  = context.bounds.width  * context.ppmm;

	// HACK: Somehow the bounds do not match gerbv exported size. Copy from inputs.
	height = context.inputs.begin()->second.rows;
	width  = context.inputs.begin()->second.cols;

	// Default void is black
//	DEBUG("Creating Mat2b with size " << width << "x" << height << "...");
	cv::Mat mPreview(cv::Size(width, height), CV_8UC1);
	cv::rectangle(mPreview, cv::Point(0,0), mPreview.size(), 0, -1);
//	DEBUG("Created Mat2b. Size is " << mPreview.size << ", data is " << mPreview.data);
	for (auto &metapath : paths) {
		auto &points = metapath.path->points;
		if (!points.size()) continue;
		
		auto &tool = *metapath.tool;
		// Through-cuts and drills go white.
		// Surface milling goes gray.
		uint8_t color = tool.depth > 1 ? 255 : 128;
		int thickness = tool.diameter * context.ppmm;
		
		point_t p0 = points.front();
		for (auto p1 : points) {
			cv::line(mPreview, p0, p1, color, thickness);
			p0 = p1;
		}
	}

	if (mirror)
		cv::flip(mPreview, mPreview, 0);
	
	// HACK: Formatters must output vector of strings. Needs rework.
	std::vector<unsigned char> data;
	cv::imencode(".png", mPreview, data);
	const char *p = (const char *)data.data();
	
	return {std::string(p, data.size())};
}

}
