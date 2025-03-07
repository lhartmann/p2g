#include <pcb2gcode.hpp>
#include <yaml-cpp/yaml.h>

namespace pcb2gcode {

bool load_tools(context_t &context) {
//    DEBUG("Loading tools...");
    tool_map_t &tools = context.tools;

    auto get = [&context](std::string tool_name, std::string opt) -> YAML::Node {
        while (!tool_name.empty()) {
//            DEBUG("    Loading parameter " << opt << " for " << tool_name << "...");
            YAML::Node data = context.yaml["tools"][tool_name][opt];
            if (data) return data;
            tool_name = context.yaml["tools"][tool_name]["like"].as<std::string>("");
        }
        return {};
    };

    if (!context.yaml["tools"].IsDefined())
        throw error("Missing tools section.");

    for (auto c : context.yaml["tools"]) {
        tool_t tool;
        std::string name = c.first.as<std::string>();
//        DEBUG("  " << name);
        tool.feed = get(name, "feed").as<double>(0);
        tool.runs = get(name, "runs").as<int>(1);
        tool.angle = get(name, "angle").as<double>(0);
        tool.climb = get(name, "climb").as<bool>(true);
        tool.depth = get(name, "depth").as<double>(0);
        tool.speed = get(name, "speed").as<double>(0);
        tool.infeed = get(name, "infeed").as<double>(0);
        tool.plunge = get(name, "plunge").as<double>(0);
        tool.overlap = get(name, "overlap").as<double>(0);
        tool.diameter = get(name, "diameter").as<double>(0);
        tool.description = get(name, "description").as<std::string>("");
        tool.predrill = get(name, "predrill").as<std::string>("");

        std::string type = get(name, "type").as<std::string>("");
        tool.type =
            type == "mill" ? tool_t::mill  :
            type == "drill"? tool_t::drill :
            tool_t::undefined;

        tools[name] = tool;
    }

    // Create a list of tools that need predrilling.
    for (auto &t : context.tools) {
        auto &predrill = t.second.predrill;
        if (predrill.empty())
            continue;

        auto &tool = t.first;
        auto &order = context.tool_predrill_order;

        auto it = std::find(order.begin(), order.end(), predrill);
        order.insert(it, tool);
    }

    return true;
}

}
