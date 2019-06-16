#include "server/asset.hpp"
#include <fstream>

namespace judge::server {
using namespace std;

asset::asset(const string &name) : name(name) {}

local_asset::local_asset(const string &name, const filesystem::path &path)
    : asset(name), path(path) {}

void local_asset::fetch(const filesystem::path &path) {
    filesystem::copy(this->path, path);
}

text_asset::text_asset(const string &name, const string &text)
    : asset(name), text(text) {}

void text_asset::fetch(const filesystem::path &path) {
    ofstream fout(path);
    fout << text;
}

}  // namespace judge::server