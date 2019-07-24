#include "runguard.hpp"
#include <boost/lexical_cast.hpp>
#include <fstream>
#include <map>

namespace judge {
using namespace std;

static map<string, string> read_metadata(const filesystem::path &metadata_file) {
    map<string, string> mp;
    ifstream fin(metadata_file);
    string line;
    while (getline(fin, line)) {
        size_t end = 0;
        while (end + 1 < line.length()) {
            if (line[end] == ':' && line[end + 1] == ' ')
                break;
            ++end;
        }
        if (end + 2 > line.length()) continue;
        string key = line.substr(0, end);
        string value = end + 2 == line.length() ? "" : line.substr(end + 2);
        mp[key] = value;
    }
    return mp;
}

template <typename T>
void try_to_parse(const string &text, T &value) {
    try {
        value = boost::lexical_cast<T>(text);
    } catch (boost::bad_lexical_cast &) {
        // ignore exception
    }
}

runguard_result read_runguard_result(const filesystem::path &metafile) {
    auto metadata = read_metadata(metafile);
    runguard_result result;
    if (metadata.count("cpu-time")) try_to_parse(metadata.at("cpu-time"), result.cpu_time);
    if (metadata.count("sys-time")) try_to_parse(metadata.at("sys-time"), result.sys_time);
    if (metadata.count("user-time")) try_to_parse(metadata.at("user-time"), result.user_time);
    if (metadata.count("wall-time")) try_to_parse(metadata.at("wall-time"), result.wall_time);
    if (metadata.count("exitcode")) try_to_parse(metadata.at("exitcode"), result.exitcode);
    if (metadata.count("signal")) try_to_parse(metadata.at("signal"), result.signal);
    if (metadata.count("memory-bytes")) try_to_parse(metadata.at("memory-bytes"), result.memory);
    if (metadata.count("time-result")) try_to_parse(metadata.at("time-result"), result.time_result);
    if (metadata.count("internal-error")) try_to_parse(metadata.at("internal-error"), result.internal_error);
    return result;
}

}  // namespace judge
