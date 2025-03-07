#include <iostream>
#include <string>
#include <boost/format.hpp>
#include <pcb2gcode.hpp>
#include <sys/stat.h>

using namespace std;
void dump(const pcb2gcode::tool_t &tool) {
    cout << "Tool {" << endl
         << "  " << tool.description << endl
         << "  " << tool.type << endl
         << "  " << tool.diameter << endl
         << "  " << tool.angle << endl
         << "  " << tool.speed << endl
         << "  " << tool.feed << endl
         << "  " << tool.plunge << endl
         << "  " << tool.infeed << endl
         << "  " << tool.depth << endl
         << "  " << tool.climb << endl
         << "  " << tool.runs << endl
         << "  " << tool.overlap << endl
         << "}" << endl;
}

void dump(const pcb2gcode::job_tool_paths_t &jtp) {
    for (auto &i : jtp) {
        for (auto &j : i.second) {
            cout << i.first << "/" << j.first << " = " << j.second.size() << " paths." << endl;
        }
    }
}

int main(int argc, char *argv[])
{
    srand(time(0));
    if (argc < 2) {
        cout << "Use: pcb2gcode file.p2g" << endl;
        return 0;
    }

    try {
        DEBUG("Loading configuration file...");
        pcb2gcode::context_t config;
        config.fileName = pcb2gcode::getRealPath(argv[1]);
        config.yaml = YAML::LoadFile(config.fileName);

        if (config.yaml["debug"].as<bool>(false))
            mkdir("p2g-debug-out", 0700);

        DEBUG("Loading tools...");
        load_tools(config);
        do_inputs(config);
        do_jobs(config);
        do_outputs(config);
    } catch (pcb2gcode::error e) {
        cout << "ERROR: " << e << endl;
    }

    return 0;
}
