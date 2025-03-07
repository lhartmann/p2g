#include <fstream>
#include <pcb2gcode.hpp>
#include <regex>
#include <string>

namespace pcb2gcode {

static inline std::string strClean(std::string in) {
	std::string out;
	for (auto ch : in)
		if (std::string("\r\n\t\f\a\b").find_first_of(ch) == std::string::npos)
			out += ch;
	return out;
}

cv::Rect2d gerber_bounds(std::string edgeFileName) {
	std::ifstream in(edgeFileName.c_str(), std::ios::in | std::ios::binary);
	if (!in) throw pcb2gcode::error("Could not open input file: "+edgeFileName);

	std::string line;

	cv::Rect2d l;
	l.x      = +1./0.;
	l.y      = +1./0.;
	l.width  = -1./0.; // For now use as absolute coordinate
	l.height = -1./0.;

	double scalingx = 0./0.;
	double scalingy = 0./0.;
	bool leadingZeroesOmitted = true;
	int digitsx = 0;
	int digitsy = 0;
	bool metric = true;
	std::regex reFS("%FS([TL])AX([0-9])([0-9])Y([0-9])([0-9])\\*%");
	std::regex reX(".*X([+-]?[0-9]+).*D.*");
	std::regex reY(".*Y([+-]?[0-9]+).*D.*");

	auto valuex = [&](std::string s) -> double {
		if (!leadingZeroesOmitted) {
			s += "000000000000000000000";
			s = s.substr(0,digitsx);
		}
		return stod(s) / scalingx;
	};
	auto valuey = [&](std::string s) -> double {
		if (!leadingZeroesOmitted) {
			s += "000000000000000000000";
			s = s.substr(0,digitsy);
		}
		return stod(s) / scalingy;
	};

	while (getline(in, line)) {
		std::smatch mr;
		line = strClean(line);
		
		if (regex_match(line, mr, reFS)) {
			leadingZeroesOmitted = mr[1] == 'L';

			scalingx = pow(10, stoi(mr[3]));
			digitsx = stoi(mr[2]) + stoi(mr[3]);

			scalingy = pow(10, stoi(mr[5]));
			digitsy = stoi(mr[4]) + stoi(mr[5]);

			break;
		}
	}

	if (isnan(scalingx) || isnan(scalingy))
		throw pcb2gcode::error("Failed to find format specification line.");

	while (getline(in, line)) {
		line = strClean(line);
		
		if (line == "%MOIN*%") {
			metric = false;
			continue;
		}

		std::smatch mr;
		if (regex_match(line, mr, reX)) {
			double x = valuex(mr[1]);

			if (l.x > x)
				l.x = x;

			if (l.width < x)
				l.width = x;
		}

		if (regex_match(line, mr, reY)) {
			double y = valuey(mr[1]);

			if (l.y > y)
				l.y      = y;

			if (l.height < y)
				l.height = y;
		}
	}

	l.width  -= l.x;
	l.height -= l.y;

	if (!metric) {
		l.x      *= 25.4;
		l.y      *= 25.4;
		l.width  *= 25.4;
		l.height *= 25.4;
	}

	return l;
}

}
