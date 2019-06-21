#include "common/metadata.hpp"
#include <fstream>
#include <nlohmann/json.hpp>

namespace judge {
using namespace std;
using namespace nlohmann;

map<string, string> read_metadata(const filesystem::path &metadata_file) {
    map<string, string> mp;
    ifstream fin(metadata_file);
    json p;
    fin >> p;
    for (auto &[key, value] : p.items())
        if (value.is_string())
            mp[key] = value.get<string>();
    return mp;
}

}  // namespace judge
