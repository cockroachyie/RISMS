#include "ConfigStore.h"

#include <fstream>
#include <sstream>

namespace risms {

ConfigStore& ConfigStore::instance() {
    static ConfigStore store;
    return store;
}

Result<void> ConfigStore::load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return Result<void>::Fail(ErrorCode::FileIOError,
                                   "Could not open config file: " + path);
    }

    values_.clear();
    std::string line;
    while (std::getline(file, line)) {
        // strip comments and whitespace
        size_t hash = line.find('#');
        if (hash != std::string::npos) line = line.substr(0, hash);

        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue; // blank line
        size_t end = line.find_last_not_of(" \t\r\n");
        line = line.substr(start, end - start + 1);
        if (line.empty()) continue;

        size_t eq = line.find('=');
        if (eq == std::string::npos) continue; // malformed line, skip

        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);

        // trim key/value independently
        size_t ks = key.find_first_not_of(" \t");
        size_t ke = key.find_last_not_of(" \t");
        size_t vs = value.find_first_not_of(" \t");
        size_t ve = value.find_last_not_of(" \t");

        if (ks == std::string::npos) continue;
        key = key.substr(ks, ke - ks + 1);
        value = (vs == std::string::npos) ? "" : value.substr(vs, ve - vs + 1);

        values_[key] = value;
    }

    return Result<void>::Ok();
}

std::string ConfigStore::getString(const std::string& key, const std::string& defaultValue) const {
    auto it = values_.find(key);
    return (it != values_.end()) ? it->second : defaultValue;
}

int ConfigStore::getInt(const std::string& key, int defaultValue) const {
    auto it = values_.find(key);
    if (it == values_.end()) return defaultValue;
    try {
        return std::stoi(it->second);
    } catch (...) {
        return defaultValue;
    }
}

} // namespace risms
