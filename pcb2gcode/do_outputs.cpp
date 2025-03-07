#include <fstream>
#include <limits>
#include <pcb2gcode.hpp>
#include <pcb2gcode/metapath_sort_anneal_reversion.hpp>
#include <pcb2gcode/metapath_sort_greed_insert.hpp>

namespace pcb2gcode {

bool output_filter_matches(std::string job, std::string tool, const YAML::Node &filters) {
	if (!filters.IsSequence()) {
		throw error("outputs[].paths must be sequences.");
	}
	
	for (const auto &filter : filters) {
		std::string f_job = filter["job"].as<std::string>("");
		std::string f_tool = filter["tool"].as<std::string>("");
		bool f_exclude = filter["exclude"].as<bool>(false);
		
		if (!f_job.empty() && f_job != job)
			continue;
		
		if (!f_tool.empty() && f_tool != tool)
			continue;
		
		return !f_exclude;
	}
	return false;
}

struct formatter_table_entry_t {
	const char *name;
	formatter_t fcn;
	bool sort_by_default;
	const char *extensions;
};

static formatter_table_entry_t formatters [] {
	{"gcode",   out_gcode,   true,  ".g .gcode "},
	{"hpgl",    out_hpgl,    true,  ".plt .hpgl "},
	{"ncdrill", out_ncdrill, true,  ".nc .ncd .xln "},
	{"preview", out_preview, false, ".png .jpg "},
};

static void for_all_points(context_t &context, std::function<void(point_t &p)> f) {
	for (auto &job_tool_paths : context.job_tool_paths)
		for (auto &tool_paths : job_tool_paths.second)
			for (auto &path : tool_paths.second)
				for (auto &point: path.points)
					f(point);
}

// Will call f(tool) multiple times, if tool is used more than once
static void for_used_tools(context_t &context, std::function<void(std::string toolname)> f) {
	for (auto &job_tool_paths : context.job_tool_paths)
		for (auto &tool_paths : job_tool_paths.second)
			f(tool_paths.first);
}

// pixel-space
static cv::Rect2i get_toolpath_bounds(context_t &context) {
	int x0 = std::numeric_limits<int>::max();
	int x1 = std::numeric_limits<int>::min();
	int y0 = x0;
	int y1 = x1;
	for_all_points(context, [&](point_t &p) {
		x0 = std::min(x0, p.x);
		x1 = std::max(x1, p.x);
		y0 = std::min(y0, p.y);
		y1 = std::max(y1, p.y);
	});
	DEBUG("AAA X" << x0<<"-"<<x1<<", Y"<<y0<<"-"<<y1);
	return cv::Rect2i(x0, y0, x1-x0, y1-y0);
}

static path_t path_translate(path_t path, int x, int y) {
	for (auto &pt : path.points) {
		pt.x += x;
		pt.y += y;
	}

	return path;
}

static paths_t paths_translate(const paths_t &paths, int x, int y) {
	paths_t r;
	r.reserve(paths.size());

	for (auto &path : paths)
		r.emplace_back(path_translate(path, x, y));

	return r;
}

static paths_t paths_replicate(const paths_t &paths, int nx, int ny, int dx, int dy) {
	paths_t r;
	r.reserve(paths.size()*nx*ny);

	for (int ix=0; ix<nx; ix++)
		for (int iy=0; iy<ny; iy++)
			for (auto &path: paths)
				r.emplace_back(path_translate(path, ix*dx, iy*dy));

	return r;
}

static bool do_outputs_replication(context_t &context) {
	auto opt = context.yaml["export-options"]["replicate"];
	if (!opt.IsDefined())
		return true;

	unsigned rows = opt["rows"].as<unsigned>(1);
	unsigned cols = opt["cols"].as<unsigned>(1);
	double clearance = opt["clearance"].as<double>(NAN);

	// Sanity check
	if (rows < 1 || cols < 1)
		return false;

	// Nothing to do
	if (rows==1 && cols == 1)
		return true;

	// Clearance defaults to half-diameter or largest used tool
	if (isnan(clearance)) {
		clearance = 0;
		for_used_tools(context, [&](std::string toolname){
			clearance = fmax(clearance, context.tools[toolname].diameter);
		});
		clearance /= 2;
	}

	DEBUG("Replicating layout grid " << rows << " rows by " << cols << " cols, with " << clearance << "mm of clearance...");

	auto bounds_px = get_toolpath_bounds(context);
	DEBUG("Toolpath bonds " << bounds_px  << "px");

	int x_step = int(clearance * context.ppmm +.5) + bounds_px.width;
	int y_step = int(clearance * context.ppmm +.5) + bounds_px.height;

	job_tool_paths_t job_tool_paths;
	for (auto &job_tool_paths : context.job_tool_paths)
		for (auto &tool_paths : job_tool_paths.second)
			tool_paths.second = paths_replicate(tool_paths.second, cols, rows, x_step, y_step);

	// Fix bounds
	context.bounds.width  += x_step*(cols-1) / context.ppmm;
	context.bounds.height += y_step*(rows-1) / context.ppmm;
	context.bounds.y      -= y_step*(rows-1) / context.ppmm;

	return true;
}

bool do_outputs_rotation(context_t &context) {
	auto angle = context.yaml["export-options"]["rotate"].as<double>(0.0)*M_PI/180;
	if (!angle)
		return true;

	DEBUG("Rotating data points...");
	double C = cos(angle);
	double S = sin(angle);
	auto rotate = [S,C](point_t &point) {
		auto p = point;
		point.x = C*p.x - S*p.y;
		point.y = S*p.x + C*p.y;
	};
	for_all_points(context, rotate);

	return true;
}

bool do_outputs_translation(context_t &context) {
	auto opt = context.yaml["export-options"]["translate"];
	if (!opt.IsDefined())
		return true;

	if (opt["zero"].as<std::string>("") == "lower-left") {
		DEBUG("Translating points...");
		// Find lower-left.
		double x0 = +INFINITY, y0 = +INFINITY;
		double x1 = -INFINITY, y1 = -INFINITY;
		for_all_points(context, [&](point_t &p) {
			x0 = std::fmin(x0, p.x);
			x1 = std::fmax(x1, p.x);
			y0 = std::fmin(y0, p.y);
			y1 = std::fmax(y1, p.y);
		});

		// Add 10mm margin
		x0 -= 10 * context.ppmm;
		x1 += 10 * context.ppmm;
		y0 -= 10 * context.ppmm;
		y1 += 10 * context.ppmm;

		// Move to reference
		for_all_points(context, [&](point_t &p) {
			p.x -= x0;
			p.y -= y0;
		});

		// Patch bounds
		context.bounds.x      =      0  / context.ppmm;
		context.bounds.width  = (x1-x0) / context.ppmm;
		context.bounds.y      =      0  / context.ppmm;
		context.bounds.height = (y1-y0) / context.ppmm;
	}
}

bool do_outputs(context_t &context) {
	// Translate and rotate all curves as required [TODO: Pre-alpha]
	do_outputs_replication(context);
	do_outputs_rotation(context);
	do_outputs_translation(context);

	DEBUG("Running output jobs...");
	for (const auto &co : context.yaml["outputs"]) {
		// Skip disabled outputs
		if (!co["enabled"].as<bool>(true))
			continue;

		std::string file = co["file"].as<std::string>();
		std::string file_ext = std::string(file, file.find_last_of('.'));
		DEBUG("  For " << file << "...");
		
		std::string formatter_type = co["type"].as<std::string>("gcode");
		const formatter_table_entry_t *formatter=0;
		for (const auto &f : formatters) {
			if (std::string(f.extensions).find(file_ext) != std::string::npos) {
				formatter = &f;
			}
			if (f.name == formatter_type) {
				formatter = &f;
				break;
			}
		}
		if (!formatter) {
			DEBUG("    Formatter " << formatter_type << " is unknown.");
			continue;
		}

		// Safety check: Outputs must be under the same folder as config.
		file = getRealPath(file);

		// Start by merging paths
		metapaths_t paths;
		iterate_job_tool_paths(context.job_tool_paths,
			[&](std::string job, std::string tool, paths_t &jtp) {
			
			// Filter
			if (!output_filter_matches(job, tool, co["paths"]))
				return;
		
			bool is_mill = context.tools[tool].type == tool_t::mill;
			bool needs_predrill = !context.tools[tool].predrill.empty();

			DEBUG("	" << jtp.size() << " paths from " << job << "/" << tool << ".");

			for (auto &path : jtp) {
				auto &points = path.points;
				if (!points.size())
					continue;

				metapath_t mp;
				mp.tool = &context.tools[tool];
				mp.path = &path;

				mp.priority = context.yaml["jobs"][job]["priority"].as<int>(0)+ co["priority"].as<int>(0);
				mp.priority += path.priority;

				// TODO: fix reversible
				mp.reversible = path.reversible;
				mp.backwards = false;
				if (mp.reversible) {
					mp.entry = points.front();
					mp.exit  = points.back();
					mp.rentry = points.back();
					mp.rexit  = points.front();
				} else {
					mp.entry = points.front();
					mp.exit  = points.back();
					mp.rentry = points.front();
					mp.rexit  = points.back();
				}

				paths.push_back(mp);
			}
		});

		if (paths.empty()) {
			DEBUG("	No paths found, skipping file.");
			continue;
		}

		// Simple sort by priority and tool diameter
		std::sort(paths.begin(), paths.end(),
			[](const metapath_t &a, const metapath_t &b) {
				if (a.priority != b.priority)
					return a.priority < b.priority;
				if (a.tool->diameter != b.tool->diameter)
					return a.tool->diameter < b.tool->diameter;
				if (a.reversible != b.reversible)
					return a.reversible < b.reversible;
				return false;
			}
		);

		// Cost-based sorting
		if (co["sort"].as<bool>(formatter->sort_by_default)) {
			// Priority and tool from previous sort must be respected, so data is segmented.
			auto begin = paths.begin();
			auto end = paths.end();
			while (begin != end) {
				// Pick a group with same priority, tool, and reversible-ness
				auto middle = begin;
				while (middle!=end && begin->priority == middle->priority && begin->tool == middle->tool && begin->reversible == middle->reversible)
					middle++;

				// Simmulated annealing is better, but requires reversible paths, and is too slow for large node count.
				size_t n = std::distance(begin, middle);
				if (!begin->reversible || n > 20000)
					metapath_sort_greed_insert(begin, middle);
				else if (n > 3)
					metapath_sort_anneal_reversal(begin, middle);

				begin = middle;
			}
		}

		bool mirror = co["side"].as<std::string>("bottom") == "bottom";
		
		DEBUG("	Generating " << formatter_type << (mirror ? " (mirrored)" : "") << "...");
		std::vector<std::string> vs = formatter->fcn(context, paths, mirror);

		if (vs.empty()) continue;

		std::ofstream out(file, std::ios::out);
		std::string eol = co["crlf"].as<bool>(false) ? "\r\n" : "\n";
		for (auto s : vs) out << s << eol;
	}

	return true;
}

}
