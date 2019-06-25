#include "common/stl_utils.hpp"

int count_directories_in_directory(const std::filesystem::path &dir) {
    if (!std::filesystem::is_directory(dir))
        return -1;
    return std::count_if(std::filesystem::directory_iterator(dir), {}, (bool (*)(const std::filesystem::path &))std::filesystem::is_directory);
}

int random(int L, int R) {
    return L + std::rand() / ((RAND_MAX + 1u) / (R - L));
}
