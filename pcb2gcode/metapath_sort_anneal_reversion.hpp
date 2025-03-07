#pragma once
#include <pcb2gcode.hpp>
#include <pcb2gcode/dxdk.hpp>

namespace pcb2gcode {

double cost_cnc_modified(point_t a, point_t b) {
	// Same point does not require pen-up.
	if (a == b)
		return 0;

	double dx = fabs(a.x - b.x);
	double dy = fabs(a.y - b.y);

	// CNC modified
	// 2000 (20mm) is the penalty for pen-up
	return 2000 + fmax(dx, dy) + 0.2*fmin(dx,dy);
};

double cost_euclidian(point_t a, point_t b) {
	double dx = a.x - b.x;
	double dy = a.y - b.y;
	return sqrt(dx*dx + dy*dy);
};

double paths_length(
	metapaths_t::iterator begin,
	metapaths_t::iterator end,
	std::function<double(const point_t &a, const point_t &b)> cost = cost_euclidian
) {
	double length = 0;
	while (begin != end) {
		auto &points = begin->path->points;
		for (size_t i=1; i < points.size(); i++) {
			length += cost(points[i-1], points[i]);
		}
		begin++;
	}
	return length;
}

static void metapath_sort_anneal_reversal(
	metapaths_t::iterator begin, metapaths_t::iterator end,
	std::function<double(const point_t &a, const point_t &b)> cost = cost_cnc_modified
) {
	using std::max, std::abs, std::sqrt;
	size_t n = std::distance(begin, end);
	auto wasted_cost = [&]() {
		double c=0;
		for (size_t i=1; i<n; i++) {
			c += cost(begin[i-1].exit, begin[i].entry);
		}
		return c + cost(begin[0].exit, begin[n-1].entry);
	};
	double initial_cost = wasted_cost();

	DEBUG("    Optimizing " << n << " paths with priority " << begin->priority << "...");
	size_t iterlimit = 100000;
	size_t changed = 1;

	dxdk<200> dtc_dk;

	auto path_reverse = [&begin](size_t b, size_t e) {
		std::reverse(begin+b, begin+e);
		for (size_t k=b; k<e; k++)
			begin[k].reverse();
	};

	auto reverse_gain = [&begin, &cost, &n](size_t b, size_t e) {
		double oldcost = cost(begin[b-1].exit, begin[b].entry ) + cost(begin[e-1].exit, begin[e%n].entry);
		double newcost = cost(begin[b-1].exit, begin[e-1].exit) + cost(begin[b].entry,  begin[e%n].entry);
		return oldcost / newcost;
	};

	double tc = wasted_cost();
	for (size_t i=0; i<iterlimit; i++) {
		// Choose path cut point at random, favoring longer travels.
		double cut_length = rand() * tc / RAND_MAX;
		size_t cut_index=0;
		while (cut_length>0) {
			size_t next = (cut_index + 1) % n;
			cut_length -= cost(begin[cut_index].exit, begin[next].entry);
			cut_index = next;
		}
		//cut_index = rand() % n;

		// Rotate so the chosen travel goes from [0] to [1].
		std::rotate(begin, begin+cut_index, end);

		double heat = 0.1 * pow(0.99, i);
		changed = 0;
		for (size_t i=1; i<n; i++) {
			// paths [1]..[i] are reversed, so make sure we are allowed to do so.
			if (!begin[i].reversible) break;

			double gain = reverse_gain(1,i);
			if (gain > 1 || heat * gain - 0.01 >= double(rand()) / RAND_MAX) {
				changed++;
				path_reverse(1, i);
			}
		}

		tc = wasted_cost();
		double dtc = dtc_dk(tc);

		DEBUGL("      Annealing iteration " << i << "/" << iterlimit << ", heat " << heat << ", cost " << int(1000*tc/initial_cost)/10.0 << "%, change " << dtc << "...\x1B[K\r");

		if (!isnan(dtc) && dtc > -0.001) {
			DEBUG("");
			DEBUGL("      Cost stagnated over last " << dtc_dk.size() << " iterations.");
			break;
		}
	}
	
	// Move a non-optimizable pen up/down to the start of the list.
	for (size_t i = 1; i < n; i++) {
		if (cost(begin[i-1].exit, begin[i].entry) != 0) {
			std::rotate(begin, begin+i, end);
			break;
		}
	}
	
	DEBUG("");
	DEBUG("      Final cost is " << int(1000*wasted_cost()/initial_cost)/10.0 << "% of original.");
}

}
