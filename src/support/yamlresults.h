#include <vector>
#include <map>
#include <fstream>
#include <string>
#include "yaml-cpp/yaml.h"


namespace helpers {

//--- Results holder ---
struct result_holder {
    YAML::Emitter out;
    std::map<std::string, YAML::Node> nodes;
    std::map<std::string, std::map<std::string, YAML::Node>> subnodes;
    bool initialized = false;
    void init() {
        out << YAML::BeginDoc;
        out << YAML::BeginMap;
        initialized = true;
    }
    template <typename T>
    void put(const std::string &section, const std::string &key, const T &value) {
        YAML::Node &node = nodes[section];
        node[key] = value;
        node[key].SetStyle(YAML::EmitterStyle::Flow);
    }
    template <typename T>
    void put(const std::string &section, const std::string &subsection, 
             const std::string &key, const T &value) {
        YAML::Node &node = nodes[section];
        if (node[subsection].IsDefined()) {
            const auto &item = node[subsection];
            throw std::runtime_error("parameter already recorded as non-map");
        } 
            
        auto &s = subnodes[section];
        auto &snode = s[subsection];
        snode[key] = value;
        snode[key].SetStyle(YAML::EmitterStyle::Flow);
    }
    template <typename T>
    void putvec(const std::string &section, const std::string &key, const std::vector<T> &vec) {
        YAML::Node &node = nodes[section];
        node[key] = vec;
        node[key].SetStyle(YAML::EmitterStyle::Flow);
    }
    void save(const std::string &result_filename) {
        if (!initialized)
            init();
        if (result_filename.empty())
            return;
        std::ofstream ofs(result_filename);
        for (auto &n : nodes) {
            auto &key = n.first;
            auto &value = n.second;
            if (subnodes.find(key) != subnodes.end()) {
                auto &s = subnodes[key];
                for (auto &snode : s) {
                    value[snode.first] = snode.second;
                }
            }
            out << YAML::Flow << YAML::Key << key << YAML::Value << value;
        }
        out << YAML::Newline;
        out << YAML::EndMap;
        out << YAML::EndDoc;
        ofs << std::string(out.c_str());
        ofs.close();
    }
};

}

